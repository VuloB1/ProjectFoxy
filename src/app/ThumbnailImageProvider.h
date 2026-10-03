#pragma once

#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
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

private:
    core::ThumbnailCache *m_cache;
    QString m_filePath;
    int m_edgeLength;
    QImage m_image;
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
};
