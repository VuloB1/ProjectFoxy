#pragma once

#include <QImage>
#include <QMediaPlayer>
#include <QObject>
#include <QVideoSink>

// Reads single frames of a video by time (Qt Multimedia): the base of "video to GIF". One request at a time:
// requestFrame(ms) seeks there and answers with frameReady() once the frame that is shown at that moment arrives.
// A trial of how exact that is lives in tests/test_video_frames.cpp.
class VideoFrameReader : public QObject {
    Q_OBJECT

public:
    explicit VideoFrameReader(QObject *parent = nullptr);

    void open(const QString &path);
    qint64 durationMs() const { return m_player.duration(); }
    QSize frameSize() const { return m_size; }

    // The frame shown at `ms` (frameReady, or failed when it does not come).
    void requestFrame(qint64 ms);

signals:
    void opened();
    // `startMs`: when that frame starts, as the video says.
    void frameReady(qint64 startMs, const QImage &image);
    void failed(const QString &reason);

private:
    void onFrame(const QVideoFrame &frame);

    QMediaPlayer m_player;
    QVideoSink m_sink;
    QSize m_size;
    qint64 m_wanted = -1;
};
