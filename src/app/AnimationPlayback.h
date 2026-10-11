#pragma once

#include <QImage>
#include <QObject>
#include <QTimer>
#include <QVector>

// Playback of an animated GIF/APNG/WebP that is open in the viewer (see core::AnimatedDecoder): the frames, the
// delay of each, and a single-shot timer restarted per frame with that frame's own delay. A picture with one frame
// or none is a still one, and nothing here does anything.
class AnimationPlayback : public QObject {
    Q_OBJECT
public:
    explicit AnimationPlayback(QObject *parent = nullptr);

    // Replaces the animation (empty or one frame = a still picture) and, with more than one, starts playing it from the
    // first frame. The old timer is stopped first: it must never publish a frame of the picture being replaced. Does
    // not emit playingChanged(): the caller announces the new picture itself.
    void setAnimation(const QVector<QImage> &frames, const QVector<int> &delaysMs);

    bool isAnimated() const { return m_frames.size() > 1; }
    bool playing() const { return m_playing; }
    // Pause or resume; ignored for a still picture. Emits playingChanged() when the state changes.
    void setPlaying(bool playing);

signals:
    // The next frame is due: show it.
    void frameChanged(const QImage &frame);
    void playingChanged();

private:
    void advance();
    int delayOf(int frame) const;

    QVector<QImage> m_frames;
    QVector<int> m_delaysMs;
    int m_index = 0;
    bool m_playing = false;
    QTimer m_timer; // single-shot
};
