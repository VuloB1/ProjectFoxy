#pragma once

#include <QImage>
#include <QMediaPlayer>
#include <QObject>
#include <QVideoSink>

// Reads frames of a video by time (Qt Multimedia): the base of "video to GIF". Two ways, one request at a time:
//  - requestFrame(ms): the frame shown at that moment (a seek per frame: about 70-370 ms each, more with big,
//    high-bitrate videos, because every seek decodes from the previous key frame);
//  - sample(from, to, step): the frames at from, from+step, ... by playing the video at 4x: a whole 36 s 1440p60 clip in
//    9 s (the seeks took 134 s), 12 s of a 1080p one in under 2. This is the one to use for GIFs.
// Both answer with frameReady(). Trials of how exact and how fast they are live in tests/test_video_frames.cpp.
class VideoFrameReader : public QObject {
    Q_OBJECT

public:
    explicit VideoFrameReader(QObject *parent = nullptr);

    void open(const QString &path);
    qint64 durationMs() const { return m_player.duration(); }
    QSize frameSize() const { return m_size; }

    // The frame shown at `ms` (frameReady, or failed when it does not come).
    void requestFrame(qint64 ms);
    // The frames at from, from+step, ... (< to): one frameReady() each, in order, then sampleFinished().
    void sample(qint64 from, qint64 to, qint64 step);
    // Gives up a sample() half way (sampleFinished() follows).
    void cancelSample();
    bool sampling() const { return m_sampling; }

signals:
    void opened();
    // `startMs`: when that frame starts, as the video says.
    void frameReady(qint64 startMs, const QImage &image);
    void sampleFinished();
    void failed(const QString &reason);

private:
    void onFrame(const QVideoFrame &frame);
    void finishSample();

    QMediaPlayer m_player;
    QVideoSink m_sink;
    QSize m_size;
    qint64 m_wanted = -1;
    bool m_sampling = false;
    qint64 m_next = 0, m_end = 0, m_step = 100;
    qint64 m_lastStart = -1, m_period = 0; // ms: the smallest gap seen between two frames (0: not known yet)
};
