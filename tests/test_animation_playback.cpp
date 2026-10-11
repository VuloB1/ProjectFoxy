#include "AnimationPlayback.h"
#include <QtTest>

namespace {
QImage frameOf(QColor c)
{
    QImage img(4, 4, QImage::Format_RGBA8888);
    img.fill(c);
    return img;
}
} // namespace

// The playback of an animated picture open in the viewer: frames in order, each held for its own delay (never less
// than 20 ms), pause and resume, and a clean hand-over when another picture replaces it.
class TestAnimationPlayback : public QObject {
    Q_OBJECT

private slots:
    void aStillPictureDoesNothing()
    {
        AnimationPlayback playback;
        QSignalSpy frames(&playback, &AnimationPlayback::frameChanged);
        playback.setAnimation({}, {});
        QVERIFY(!playback.isAnimated());
        QVERIFY(!playback.playing());
        playback.setAnimation({frameOf(Qt::red)}, {50});
        QVERIFY(!playback.isAnimated());
        QSignalSpy toggled(&playback, &AnimationPlayback::playingChanged);
        playback.setPlaying(true); // nothing to play: ignored
        QVERIFY(!playback.playing());
        QCOMPARE(toggled.count(), 0);
        QTest::qWait(100);
        QCOMPARE(frames.count(), 0);
    }

    void framesFollowOneAnotherAndLoop()
    {
        AnimationPlayback playback;
        QSignalSpy frames(&playback, &AnimationPlayback::frameChanged);
        playback.setAnimation({frameOf(Qt::red), frameOf(Qt::green), frameOf(Qt::blue)}, {20, 20, 20});
        QVERIFY(playback.isAnimated());
        QVERIFY(playback.playing()); // starts by itself
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 4, 3000);
        QCOMPARE(frames.at(0).at(0).value<QImage>().pixelColor(0, 0), QColor(Qt::green)); // frame 0 is already on show
        QCOMPARE(frames.at(1).at(0).value<QImage>().pixelColor(0, 0), QColor(Qt::blue));
        QCOMPARE(frames.at(2).at(0).value<QImage>().pixelColor(0, 0), QColor(Qt::red));   // and round again
        QCOMPARE(frames.at(3).at(0).value<QImage>().pixelColor(0, 0), QColor(Qt::green));
    }

    void aFrameIsHeldForItsOwnDelayWithAFloorOf20Ms()
    {
        AnimationPlayback playback;
        QSignalSpy frames(&playback, &AnimationPlayback::frameChanged);
        QElapsedTimer clock;
        clock.start();
        playback.setAnimation({frameOf(Qt::red), frameOf(Qt::green)}, {0, 0}); // 0 would spin: raised to 20
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 1, 2000);
        QVERIFY2(clock.elapsed() >= 15, "a 0 ms delay must not make the animation spin");

        AnimationPlayback slow;
        QSignalSpy slowFrames(&slow, &AnimationPlayback::frameChanged);
        slow.setAnimation({frameOf(Qt::red), frameOf(Qt::green)}, {400, 400});
        QTest::qWait(150);
        QCOMPARE(slowFrames.count(), 0); // still holding the first frame
        QTRY_VERIFY_WITH_TIMEOUT(slowFrames.count() == 1, 2000);
    }

    void pauseAndResume()
    {
        AnimationPlayback playback;
        QSignalSpy frames(&playback, &AnimationPlayback::frameChanged);
        QSignalSpy toggled(&playback, &AnimationPlayback::playingChanged);
        playback.setAnimation({frameOf(Qt::red), frameOf(Qt::green)}, {20, 20});
        playback.setPlaying(false);
        QVERIFY(!playback.playing());
        QCOMPARE(toggled.count(), 1);
        const int before = frames.count();
        QTest::qWait(150);
        QCOMPARE(frames.count(), before); // paused
        playback.setPlaying(false);
        QCOMPARE(toggled.count(), 1);     // no change, no signal
        playback.setPlaying(true);
        QCOMPARE(toggled.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() > before, 2000);
    }

    // Opening another picture while one plays: the old timer must not publish one more frame of the old one.
    void replacingTheAnimationDropsTheOldTimer()
    {
        AnimationPlayback playback;
        QSignalSpy frames(&playback, &AnimationPlayback::frameChanged);
        QSignalSpy toggled(&playback, &AnimationPlayback::playingChanged);
        playback.setAnimation({frameOf(Qt::red), frameOf(Qt::green)}, {40, 40});
        playback.setAnimation({}, {}); // a still picture now
        QVERIFY(!playback.playing());
        QCOMPARE(toggled.count(), 0); // setAnimation announces nothing: the caller does
        QTest::qWait(200);
        QCOMPARE(frames.count(), 0);

        playback.setAnimation({frameOf(Qt::red), frameOf(Qt::green)}, {20, 20});
        playback.setAnimation({frameOf(Qt::blue), frameOf(Qt::blue)}, {20, 20}); // replaced again at once
        QTRY_VERIFY_WITH_TIMEOUT(frames.count() >= 1, 2000);
        QCOMPARE(frames.at(0).at(0).value<QImage>().pixelColor(0, 0), QColor(Qt::blue)); // only the newest plays
    }
};

QTEST_MAIN(TestAnimationPlayback)
#include "test_animation_playback.moc"
