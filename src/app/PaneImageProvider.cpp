#include "PaneImageProvider.h"
#include "DecoderRegistry.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThreadPool>
#include <QUrl>
#include "Translate.h"

namespace {
// What the decoded copies may add up to before the least recently used are dropped.
constexpr qint64 kCacheBytes = 384LL * 1024 * 1024;
// The most a single pane may ask for, whatever its sourceSize says (a 100 MP file zoomed in on does not
// need all of it in a window).
constexpr int kMostPixelsAside = 16384;
}

PaneImageStore::PaneImageStore(QObject *parent) : QObject(parent)
{
    m_cache.setMaxCost(int(kCacheBytes / 1024)); // the cost of an entry is its size in KiB
}

QImage PaneImageStore::image(const QString &path, int maxSide, QString *error)
{
    maxSide = maxSide > 0 ? std::min(maxSide, kMostPixelsAside) : 0;
    const QString key = QStringLiteral("%1|%2|%3").arg(path).arg(QFileInfo(path).lastModified().toMSecsSinceEpoch()).arg(maxSide);
    {
        QMutexLocker lock(&m_mutex);
        if (const QImage *cached = m_cache.object(key))
            return *cached;
    }
    const auto decoder = core::DecoderRegistry::instance().decoderFor(path);
    if (!decoder) {
        if (error)
            *error = core::tr("Formato no compatible: %1").arg(path);
        return {};
    }
    const core::DecodeResult result = decoder->decode(path, maxSide > 0 ? QSize(maxSide, maxSide) : QSize());
    if (!result.ok || result.image.isNull()) {
        if (error)
            *error = result.error;
        return {};
    }
    const QSize real = result.sourceSize.isValid() ? result.sourceSize : result.image.size();
    bool firstTime = false;
    {
        QMutexLocker lock(&m_mutex);
        firstTime = m_sizes.value(path) != real;
        m_sizes.insert(path, real);
        const qint64 bytes = result.image.sizeInBytes();
        if (bytes < kCacheBytes / 2)
            m_cache.insert(key, new QImage(result.image), int(bytes / 1024));
    }
    if (firstTime)
        QMetaObject::invokeMethod(this, [this, path]() { emit sourceSizeKnown(path); }, Qt::QueuedConnection);
    return result.image;
}

QSize PaneImageStore::sourceSize(const QString &path) const
{
    QMutexLocker lock(&m_mutex);
    return m_sizes.value(path);
}

PaneImageResponse::PaneImageResponse(PaneImageStore *store, QString path, int maxSide)
    : m_store(store), m_path(std::move(path)), m_maxSide(maxSide)
{
    // the engine deletes a response once it has been consumed: the pool must not too
    setAutoDelete(false);
}

void PaneImageResponse::run()
{
    m_image = m_store->image(m_path, m_maxSide, &m_error);
    emit finished();
}

QQuickTextureFactory *PaneImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

QQuickImageResponse *PaneImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // "<percent-encoded path>?anything": the query only makes QML ask again
    const QString encoded = id.left(id.indexOf(QLatin1Char('?')) < 0 ? id.size() : id.indexOf(QLatin1Char('?')));
    const QString path = QUrl::fromPercentEncoding(encoded.toUtf8());
    const int side = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 0;
    auto *response = new PaneImageResponse(m_store, path, side);
    QThreadPool::globalInstance()->start(response);
    return response;
}
