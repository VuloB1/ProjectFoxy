#include "ThumbnailCache.h"
#include "DecoderRegistry.h"
#include "SystemThumbnails.h"

#include <QCache>
#include <QMutex>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>

namespace core {

namespace {

QString cacheKey(const QString &filePath, int edgeLength)
{
    // Including mtime means an edited/replaced file on disk naturally
    // invalidates its old thumbnail instead of needing an explicit evict.
    const QFileInfo info(filePath);
    return QStringLiteral("%1|%2|%3")
        .arg(filePath)
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(edgeLength);
}

// Content-addressable disk filename: hashing the same key used for the
// memory cache means the file IS the index, nothing else to keep in sync.
//
// "v2|": thumbnails with transparency used to be stored as JPEG, which has no alpha, so
// they came back with their hidden colour showing. The prefix makes those old files
// unreachable (they are simply never looked up again) instead of served.
QString diskFileName(const QString &key, const char *extension)
{
    return QString::fromLatin1(QCryptographicHash::hash(("v2|" + key).toUtf8(), QCryptographicHash::Sha1).toHex())
        + QLatin1Char('.') + QLatin1String(extension);
}

// Does any pixel actually let the background through? (Most pictures carry an alpha channel
// that is solid everywhere; those can still be stored as the much smaller JPEG.)
bool hasTranslucentPixels(const QImage &image)
{
    if (!image.hasAlphaChannel())
        return false;
    const QImage rgba = image.format() == QImage::Format_RGBA8888 ? image : image.convertToFormat(QImage::Format_RGBA8888);
    for (int y = 0; y < rgba.height(); ++y) {
        const uchar *line = rgba.constScanLine(y);
        for (int x = 0; x < rgba.width(); ++x)
            if (line[x * 4 + 3] != 255)
                return true;
    }
    return false;
}

} // namespace

struct ThumbnailCache::Impl {
    mutable QMutex mutex;
    QCache<QString, QImage> memoryCache;
};

ThumbnailCache::ThumbnailCache(QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->memoryCache.setMaxCost(m_memoryBudgetBytes);
}

ThumbnailCache::~ThumbnailCache() = default;

std::optional<QImage> ThumbnailCache::cachedThumbnail(const QString &filePath, int edgeLength) const
{
    const QString key = cacheKey(filePath, edgeLength);

    {
        QMutexLocker lock(&m_impl->mutex);
        if (const QImage *cached = m_impl->memoryCache.object(key))
            return *cached;
    }

    // A disk hit doesn't "touch the source file" (this method's contract) -
    // it only reads the cache's own thumbnail file - so it belongs here too,
    // promoting the result into the memory tier for next time.
    if (!m_diskLocation.isEmpty()) {
        // Opaque pictures are stored as small JPEGs, ones with transparency as PNG.
        for (const char *extension : {"jpg", "png"}) {
            QImage fromDisk;
            if (fromDisk.load(m_diskLocation + QLatin1Char('/') + diskFileName(key, extension))) {
                QMutexLocker lock(&m_impl->mutex);
                m_impl->memoryCache.insert(key, new QImage(fromDisk), fromDisk.sizeInBytes());
                return fromDisk;
            }
        }
    }

    return std::nullopt;
}

QImage ThumbnailCache::thumbnail(const QString &filePath, int edgeLength)
{
    if (auto cached = cachedThumbnail(filePath, edgeLength))
        return *cached;

    // The system may already have this one (Explorer's cache, the freedesktop store): reading a
    // small ready-made picture is far cheaper than decoding the original.
    {
        const QImage fromSystem = systemThumbnail(filePath, edgeLength);
        if (!fromSystem.isNull()) {
            QMutexLocker lock(&m_impl->mutex);
            m_impl->memoryCache.insert(cacheKey(filePath, edgeLength), new QImage(fromSystem), fromSystem.sizeInBytes());
            return fromSystem;
        }
    }

    auto decoder = DecoderRegistry::instance().decoderFor(filePath);
    if (!decoder)
        return {};

    const DecodeResult result = decoder->decode(filePath, QSize(edgeLength, edgeLength));
    if (!result.ok)
        return {};

    const QString key = cacheKey(filePath, edgeLength);
    {
        QMutexLocker lock(&m_impl->mutex);
        m_impl->memoryCache.insert(key, new QImage(result.image), result.image.sizeInBytes());
    }

    // Best-effort: a failed write (missing/unwritable directory) just means
    // no disk persistence for this entry, not a failed thumbnail load.
    if (!m_diskLocation.isEmpty()) {
        if (hasTranslucentPixels(result.image))
            result.image.save(m_diskLocation + QLatin1Char('/') + diskFileName(key, "png"), "PNG");
        else
            result.image.save(m_diskLocation + QLatin1Char('/') + diskFileName(key, "jpg"), "JPEG", 85);
    }

    return result.image;
}

void ThumbnailCache::ensureThumbnail(const QString &filePath, int edgeLength)
{
    const QImage image = thumbnail(filePath, edgeLength);
    if (!image.isNull())
        emit thumbnailReady(filePath, image);
}

void ThumbnailCache::setMemoryBudgetBytes(qint64 bytes)
{
    m_memoryBudgetBytes = bytes;
    QMutexLocker lock(&m_impl->mutex);
    m_impl->memoryCache.setMaxCost(bytes);
}

void ThumbnailCache::setDiskLocation(const QString &directoryPath)
{
    m_diskLocation = directoryPath;
    if (!m_diskLocation.isEmpty())
        QDir().mkpath(m_diskLocation);
}

} // namespace core
