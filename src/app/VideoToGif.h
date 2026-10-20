#pragma once

#include "VideoFrameReader.h"

#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QQuickImageProvider>
#include <QRect>
#include <QTimer>
#include <QVariantList>

// "Desde un vídeo" in the GIF studio: opens a video, shows its frames while the person picks the pieces to keep and
// the crop, then reads those frames (cropped and scaled to the chosen width) and hands them to the studio.
//
// The page (qml/VideoToGifPage.qml) drives it: open(), seek() for the preview, create() with the pieces, the frame
// rate, the crop (as fractions of the picture) and the width; progress while it works; `framesReady` carries the result
// (AppBootstrap connects it to AnimStudio::addFrames).
class VideoToGif : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY loadedChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY loadedChanged)
    Q_PROPERTY(int sourceWidth READ sourceWidth NOTIFY loadedChanged)
    Q_PROPERTY(int sourceHeight READ sourceHeight NOTIFY loadedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    // "" | "open" (the video could not be opened) | "read" (it stopped giving frames)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // Bumped when the preview picture changes: part of its URL, image://videoprev/<revision>.
    Q_PROPERTY(int previewRevision READ previewRevision NOTIFY previewChanged)
    Q_PROPERTY(int maxFrames READ maxFrames CONSTANT)

public:
    // The most frames one animation may have (AnimStudio's limit) and the memory the frames may take.
    static constexpr int kMaxFrames = 600;

    explicit VideoToGif(QObject *parent = nullptr);

    int maxFrames() const { return kMaxFrames; }
    bool loaded() const { return m_loaded; }
    QString fileName() const { return m_fileName; }
    int durationMs() const { return int(m_reader.durationMs()); }
    int sourceWidth() const { return m_source.width(); }
    int sourceHeight() const { return m_source.height(); }
    bool busy() const { return m_busy; }
    qreal progress() const { return m_progress; }
    QString error() const { return m_error; }
    int previewRevision() const { return m_revision; }
    QImage preview() const { QMutexLocker lock(&m_previewMutex); return m_preview; } // (any thread)

    // `file` is a path or a file:// URL.
    Q_INVOKABLE void open(const QString &file);
    Q_INVOKABLE void close();
    // The picture shown at `ms` (previewRevision changes when it arrives). Not while busy.
    Q_INVOKABLE void seek(int ms);
    // Whether the path has the extension of a video this computer can read.
    Q_INVOKABLE bool isVideo(const QString &file) const;

    // Reads the frames of `segments` ([[fromMs, toMs], ...], in order) at `fps` frames per second, cut to the crop
    // rectangle (x, y, w, h as 0..1 fractions of the picture) and scaled to `outWidth`. framesReady() when done.
    Q_INVOKABLE void create(const QVariantList &segments, int fps, qreal cropX, qreal cropY, qreal cropW, qreal cropH,
                            int outWidth);
    Q_INVOKABLE void cancel();

    // How many frames create() will make for these pieces (the same count the page shows).
    Q_INVOKABLE static int frameCount(const QVariantList &segments, int fps);

signals:
    void loadedChanged();
    void busyChanged();
    void progressChanged();
    void errorChanged();
    void previewChanged();
    // The frames, each of `holdMs`, ready to go into the studio.
    void framesReady(const QList<QImage> &frames, int holdMs, const QString &label);
    void cancelled();

private:
    void onFrame(qint64 startMs, const QImage &image);
    void nextSegment();
    void finish(bool ok);
    void setBusy(bool busy);
    void fail(const QString &what);
    static QString toPath(const QString &file);

    VideoFrameReader m_reader;
    QString m_fileName;
    QSize m_source;
    bool m_loaded = false;
    bool m_busy = false;
    bool m_cancelled = false;
    qreal m_progress = 0;
    QString m_error;
    mutable QMutex m_previewMutex;
    QImage m_preview;
    int m_revision = 0;
    qint64 m_pendingSeek = -1;

    // one create()
    QList<QPair<qint64, qint64>> m_segments;
    int m_segment = 0;
    qint64 m_step = 100;
    QRect m_crop;
    QSize m_outSize;
    int m_expected = 1;
    QList<QImage> m_frames;
    QTimer m_watchdog; // the video stopped giving frames
};

// image://videoprev/<revision>: the picture the preview shows.
class VideoPreviewProvider : public QQuickImageProvider {
public:
    explicit VideoPreviewProvider(VideoToGif *video) : QQuickImageProvider(QQuickImageProvider::Image), m_video(video) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    VideoToGif *m_video;
};
