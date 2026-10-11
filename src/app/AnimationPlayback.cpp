#include "AnimationPlayback.h"

#include <QtGlobal>

AnimationPlayback::AnimationPlayback(QObject *parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &AnimationPlayback::advance);
}

int AnimationPlayback::delayOf(int frame) const
{
    return qBound(20, m_delaysMs.value(frame, 100), 10000);
}

void AnimationPlayback::setAnimation(const QVector<QImage> &frames, const QVector<int> &delaysMs)
{
    // stop() first: a pending timer from the PREVIOUS animation must never fire after this point, since it would
    // publish a stale frame from the image being replaced right now.
    m_timer.stop();
    m_frames = frames;
    m_delaysMs = delaysMs;
    m_index = 0;
    m_playing = isAnimated();
    if (m_playing)
        m_timer.start(delayOf(0));
}

void AnimationPlayback::advance()
{
    if (!isAnimated())
        return;
    m_index = (m_index + 1) % m_frames.size();
    emit frameChanged(m_frames.at(m_index));
    if (m_playing)
        m_timer.start(delayOf(m_index));
}

void AnimationPlayback::setPlaying(bool playing)
{
    if (!isAnimated() || playing == m_playing)
        return;
    m_playing = playing;
    if (playing)
        m_timer.start(delayOf(m_index));
    else
        m_timer.stop();
    emit playingChanged();
}
