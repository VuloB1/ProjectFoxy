#pragma once

#include "ImageDocument.h"
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QThreadPool>
#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>

namespace core {

// Async, cancellable image loading. Runs decoding on a dedicated thread pool
// (separate from the Qt Concurrent global pool used elsewhere) so image
// decode never competes with, or is starved by, other background work.
//
// Usage pattern from the UI: request a fast low-res preview first (from
// ThumbnailCache if available), then request full resolution; whichever
// finishes displays, and a later full-res result replaces an earlier preview.
//
// Contract: for one request id, only the NEWEST request ever reaches `loaded` /
// `failed`. An older one that is still decoding when it is superseded (or
// cancelled) finishes its decode - nothing can interrupt a library call halfway -
// but its result is thrown away, and it skips whatever step it had not started yet
// (a request still waiting in the queue is not decoded at all). Call requestLoad()
// and cancel() from the thread the loader lives in.
class ImageLoader : public QObject {
    Q_OBJECT
public:
    explicit ImageLoader(QObject *parent = nullptr);
    ~ImageLoader() override;

    // Supersedes any request already running for `requestId`.
    void requestLoad(const QString &requestId, const QString &filePath, QSize maxSize = QSize());
    // After this, nothing more is reported for `requestId` until a new requestLoad().
    void cancel(const QString &requestId);
    // Decodes these files ahead of time into the cache, one at a time and after anything already queued; a later
    // call replaces the files still waiting. A requestLoad() for a file that is being prefetched waits for it
    // instead of decoding it again, and for one that is done it answers at once.
    void prefetch(const QStringList &filePaths);

signals:
    // A big JPEG decodes much faster reduced (the decoder skips most of the work): this is the first look at it,
    // `document.isPreview`, up to kQuickPreviewSide on its long side and with the real size in `sourceSize`. Always
    // followed by `loaded` (or `failed`) for the same request, never delivered after them or for a superseded request.
    void previewLoaded(const QString &requestId, ImageDocument document);
    void loaded(const QString &requestId, ImageDocument document);
    void failed(const QString &requestId, QString error);

private:
    // One per request. Set when the request is superseded or cancelled; the worker
    // polls it between its steps.
    using CancelFlag = std::shared_ptr<std::atomic<bool>>;
    // Hands a finished (or failed) request to the UI thread, which publishes it only
    // if it is still the newest for its id.
    void deliver(const QString &requestId, const CancelFlag &flag, const ImageDocument &doc, const QString &error);

    // The finished document of `path` from the cache, or decoded now (and cached). False with `error` set on failure.
    bool fetch(const QString &path, ImageDocument &doc, QString &error);
    // Worth a reduced first look: a JPEG of a megabyte or more that is not in the cache already.
    bool wantsQuickPreview(const QString &path);
    static bool decodeUncached(const QString &path, QSize maxSize, ImageDocument &doc, QString &error);

    // Decoded pictures that were looked at or prefetched, newest first. Keyed by what the file was when decoded, so
    // a file that was saved over or replaced is read again. Not kept: animations (their frames are huge) and
    // files that do not exist (tests).
    struct CacheEntry {
        QString path;
        qint64 size = 0;
        QDateTime modified;
        ImageDocument doc;
    };
    std::mutex m_cacheMutex;
    std::condition_variable m_cacheChanged;
    std::list<CacheEntry> m_cache;
    QSet<QString> m_decoding; // files being decoded right now, by a load or a prefetch

    QThreadPool m_pool;
    QThreadPool m_prefetchPool; // one thread: libvips decodes one file at a time anyway
    std::atomic<int> m_prefetchGeneration{0};
    // The newest request per id. Touched only from the loader's own thread.
    QHash<QString, CancelFlag> m_active;
};

} // namespace core
