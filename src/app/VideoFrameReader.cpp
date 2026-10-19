#include "VideoFrameReader.h"

#include <QUrl>
#include <QVideoFrame>

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
    });
    connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &text) { emit failed(text); });
}

void VideoFrameReader::open(const QString &path)
{
    m_wanted = -1;
    m_player.setSource(QUrl::fromLocalFile(path));
}

void VideoFrameReader::requestFrame(qint64 ms)
{
    m_wanted = ms;
    m_player.setPosition(ms);
    m_player.pause(); // a paused player shows the frame of the position it was moved to
}

void VideoFrameReader::onFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;
    m_size = frame.size();
    if (m_wanted < 0)
        return;
    const qint64 start = frame.startTime() / 1000; // microseconds -> ms
    const qint64 end = frame.endTime() > frame.startTime() ? frame.endTime() / 1000 : start + 50;
    // After a seek some backends first show the frame that was on screen: only the one that covers `wanted` counts.
    if (m_wanted < start || m_wanted >= end)
        return;
    m_wanted = -1;
    emit frameReady(start, frame.toImage().convertToFormat(QImage::Format_RGBA8888));
}
