#pragma once

#include "ImageDocument.h"
#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <atomic>
#include <memory>

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
    void preloadNeighbors(const QStringList &filePaths, QSize maxSize);

signals:
    void loaded(const QString &requestId, ImageDocument document);
    void failed(const QString &requestId, QString error);

private:
    // One per request. Set when the request is superseded or cancelled; the worker
    // polls it between its steps.
    using CancelFlag = std::shared_ptr<std::atomic<bool>>;
    // Hands a finished (or failed) request to the UI thread, which publishes it only
    // if it is still the newest for its id.
    void deliver(const QString &requestId, const CancelFlag &flag, const ImageDocument &doc, const QString &error);

    QThreadPool m_pool;
    // The newest request per id. Touched only from the loader's own thread.
    QHash<QString, CancelFlag> m_active;
};

} // namespace core
