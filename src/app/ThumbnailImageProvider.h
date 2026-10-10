#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QThreadPool>
#include <atomic>
#include <QImage>

namespace core { class ThumbnailCache; }

// Decodes one filmstrip thumbnail on a QThreadPool worker and hands the
// result back to Qt Quick. Doubling as a QRunnable is the documented Qt
// pattern for QQuickAsyncImageProvider (see "Image Response Example") -
// run() executes off the GUI thread, then emits finished() which Qt Quick
// picks up via its usual cross-thread queued connection.
class ThumbnailImageResponse : public QQuickImageResponse, public QRunnable {
public:
    ThumbnailImageResponse(core::ThumbnailCache *cache, QString filePath, int edgeLength);

    void run() override;
    QQuickTextureFactory *textureFactory() const override;
    // The thumbnail scrolled out of view (or its cell was destroyed) before a worker reached it:
    // skip the work. A decode already under way finishes; its result is simply not used.
    void cancel() override { m_cancelled.store(true); }

private:
    core::ThumbnailCache *m_cache;
    QString m_filePath;
    int m_edgeLength;
    QImage m_image;
    std::atomic<bool> m_cancelled{false};
};

// Registered as "thumb" in main.cpp. FolderModel builds source URLs like
// "image://thumb/<percent-encoded absolute path>" so every distinct file
// gets its own cache entry regardless of special characters in the path.
class ThumbnailImageProvider : public QQuickAsyncImageProvider {
public:
    explicit ThumbnailImageProvider(core::ThumbnailCache *cache);

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    core::ThumbnailCache *m_cache;
    // Its own pool, at low priority and with fewer threads than the machine has, so a folder full
    // of thumbnails never competes with opening the picture the user is waiting for.
    QThreadPool m_pool;
};
