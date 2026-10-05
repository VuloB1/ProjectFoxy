#pragma once

#include <QCache>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QSize>

// The pictures of the "Varias imágenes" view (qml/MultiView.qml): each pane shows its own file, at the
// resolution it needs right now. A pane asks for "image://pane/<percent-encoded path>" with a sourceSize:
// that is the largest side wanted, so a pane that only shows the picture fitted gets a modest copy and one
// zoomed in a lot gets the real pixels (or as many as the view allows).
//
// The store decodes through the same decoders as the main viewer (so RAW, HEIC, AVIF... work and every
// picture is already in sRGB), remembers what it decoded for a while (switching layouts or toggling the
// link does not decode again) and knows each file's real size once it has been decoded.
class PaneImageStore : public QObject {
    Q_OBJECT
public:
    explicit PaneImageStore(QObject *parent = nullptr);

    // Decodes `path` so that its longer side is at most `maxSide` (<= 0: as large as it is). Safe to call
    // from several threads. A null image when the file cannot be read.
    QImage image(const QString &path, int maxSide, QString *error = nullptr);

    // The real size of the file's picture, or an invalid size until it has been decoded once.
    Q_INVOKABLE QSize sourceSize(const QString &path) const;

signals:
    void sourceSizeKnown(const QString &path);

private:
    mutable QMutex m_mutex;
    QCache<QString, QImage> m_cache;
    QHash<QString, QSize> m_sizes;
};

class PaneImageResponse : public QQuickImageResponse, public QRunnable {
public:
    PaneImageResponse(PaneImageStore *store, QString path, int maxSide);

    void run() override;
    QQuickTextureFactory *textureFactory() const override;
    QString errorString() const override { return m_error; }

private:
    PaneImageStore *m_store;
    QString m_path;
    int m_maxSide;
    QImage m_image;
    QString m_error;
};

// Registered as "pane" in main.cpp.
class PaneImageProvider : public QQuickAsyncImageProvider {
public:
    explicit PaneImageProvider(PaneImageStore *store) : m_store(store) {}
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    PaneImageStore *m_store;
};
