#include "ThumbnailImageProvider.h"
#include "ThumbnailCache.h"

#include <QUrl>
#include <QThread>

namespace {
constexpr int kDefaultEdgeLength = 160;
}

ThumbnailImageResponse::ThumbnailImageResponse(core::ThumbnailCache *cache, QString filePath, int edgeLength)
    : m_cache(cache)
    , m_filePath(std::move(filePath))
    , m_edgeLength(edgeLength)
{
    // QQuickImageResponse owns its own lifetime (the engine deletes it after
    // finished() is processed); QThreadPool must not also delete it.
    setAutoDelete(false);
}

void ThumbnailImageResponse::run()
{
    if (!m_cancelled.load())
        m_image = m_cache->thumbnail(m_filePath, m_edgeLength);
    emit finished();
}

QQuickTextureFactory *ThumbnailImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

ThumbnailImageProvider::ThumbnailImageProvider(core::ThumbnailCache *cache)
    : m_cache(cache)
{
    m_pool.setMaxThreadCount(qMax(1, QThread::idealThreadCount() / 2));
    m_pool.setThreadPriority(QThread::LowPriority);
}

QQuickImageResponse *ThumbnailImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    const QString filePath = QUrl::fromPercentEncoding(id.toUtf8());
    const int edge = requestedSize.isValid()
        ? qMax(requestedSize.width(), requestedSize.height())
        : kDefaultEdgeLength;

    auto *response = new ThumbnailImageResponse(m_cache, filePath, edge);
    m_pool.start(response);
    return response;
}
