#include "VideoFrameReader.h"

#include <QUrl>
#include <QVideoFrame>

namespace {
// Fast but not flat out: at 8x the hardware decoder runs out of frames and logs errors (4x: none, and 358 of 358 slots).
constexpr double kSampleRate = 4.0;
}

VideoFrameReader::VideoFrameReader(QObject *parent)
    : QObject(parent)
{
    m_player.setVideoSink(&m_sink);
    connect(&m_sink, &QVideoSink::videoFrameChanged, this, &VideoFrameReader::onFrame);
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia)
            emit opened();
        else if (status == QMediaPlayer::InvalidMedia)
            emit failed(m_player.errorString());
        else if (status == QMediaPlayer::EndOfMedia && m_sampling)
            finishSample(); // the last slots fall after the last frame
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &text) { emit failed(text); });
}

void VideoFrameReader::open(const QString &path)
{
    m_wanted = -1;
    m_sampling = false;
    m_player.setSource(QUrl::fromLocalFile(path));
}

void VideoFrameReader::requestFrame(qint64 ms)
{
    // The player shows no frame when moved to the very end (it is "at the end of the media"): stay a frame short of it.
    const qint64 last = m_player.duration() > 60 ? m_player.duration() - 60 : 0;
    ms = qMax<qint64>(1, qMin(ms, last)); // (position 0 is where a new player already is: nothing would be shown)
    m_wanted = ms;
    m_player.setPosition(ms);
    m_player.pause(); // a paused player shows the frame of the position it was moved to
}

void VideoFrameReader::sample(qint64 from, qint64 to, qint64 step)
{
    m_wanted = -1;
    m_sampling = true;
    m_next = qMax<qint64>(0, from);
    m_end = qMin(to, m_player.duration());
    m_step = qMax<qint64>(1, step);
    m_player.setPlaybackRate(kSampleRate);
    m_player.setPosition(m_next);
    m_player.play();
}

void VideoFrameReader::cancelSample()
{
    if (m_sampling)
        finishSample();
}

void VideoFrameReader::finishSample()
{
    m_sampling = false;
    m_player.pause();
    m_player.setPlaybackRate(1.0);
    emit sampleFinished();
}

void VideoFrameReader::onFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;
    m_size = frame.size();
    const qint64 start = frame.startTime() / 1000; // microseconds -> ms
    const qint64 end = frame.endTime() > frame.startTime() ? frame.endTime() / 1000 : start + 50;

    if (m_sampling) {
        // A frame still stands for every slot that falls before it ends (a slot a fast decoder skipped over gets the
        // next frame). Frames from before `from`, shown while the seek lands, end before the first slot: ignored.
        if (m_next >= end)
            return;
        QImage image = frame.toImage().convertToFormat(QImage::Format_RGBA8888);
        while (m_sampling && m_next < end && m_next < m_end) {
            m_next += m_step;
            emit frameReady(start, image); // (a signal handler may call cancelSample())
        }
        if (m_sampling && m_next >= m_end)
            finishSample();
        return;
    }

    if (m_wanted < 0)
        return;
    // After a seek some backends first show the frame that was on screen: only the one that covers `wanted` counts.
    if (m_wanted < start || m_wanted >= end)
        return;
    m_wanted = -1;
    emit frameReady(start, frame.toImage().convertToFormat(QImage::Format_RGBA8888));
}
