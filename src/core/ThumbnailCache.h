#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <optional>
#include <memory>

namespace core {

// Two-tier cache: an in-memory LRU (byte-cost based, see
// setMemoryBudgetBytes()) for the current session, backed by an on-disk
// store so reopening a folder shows thumbnails instantly instead of
// re-decoding every file. The disk tier is content-addressable - each
// thumbnail's filename is a hash of path+mtime+size+edgeLength - so the
// filename itself is the index (no separate database): an edited/replaced
// source file naturally misses (different mtime/size) instead of needing an
// explicit evict, and there's nothing to keep consistent besides the files
// themselves. Entries are never proactively evicted from disk (only ever
// added), which is fine for a per-user cache directory but would want a
// size cap if this ever needs to bound total disk usage.
class ThumbnailCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailCache(QObject *parent = nullptr);
    ~ThumbnailCache() override;

    // Returns a cached thumbnail if present, without touching the source
    // file. Thread-safe.
    std::optional<QImage> cachedThumbnail(const QString &filePath, int edgeLength) const;

    // Synchronous cache-or-decode: returns immediately from cache on a hit,
    // otherwise decodes via DecoderRegistry (shrink-on-load to edgeLength)
    // and stores the result before returning it. Thread-safe - this is what
    // ThumbnailImageProvider calls from its worker threads, so scrolling a
    // filmstrip never re-decodes a file it has already shown.
    QImage thumbnail(const QString &filePath, int edgeLength);

    // Fire-and-forget warm-up for background prefetch of off-screen
    // filmstrip entries: decodes on the calling thread and emits
    // thumbnailReady() once cached.
    void ensureThumbnail(const QString &filePath, int edgeLength);

    void setMemoryBudgetBytes(qint64 bytes);
    void setDiskLocation(const QString &directoryPath);

signals:
    void thumbnailReady(const QString &filePath, QImage thumbnail);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    qint64 m_memoryBudgetBytes = 256 * 1024 * 1024; // 256 MB default
    QString m_diskLocation;
};

} // namespace core
