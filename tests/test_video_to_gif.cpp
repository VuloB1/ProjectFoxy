#include "VideoToGif.h"

#include <QElapsedTimer>
#include <QtTest>

// tests/fixtures/video_20frames_10fps.mp4: 64x48, 20 frames at 10 fps, frame i is solid gray 10 + 12*i.
class TestVideoToGif : public QObject {
    Q_OBJECT

    static int grayOf(const QImage &image) { return qGray(image.pixel(image.width() / 2, image.height() / 2)); }
    static QString video() { return QStringLiteral(FIXTURES_DIR "/video_20frames_10fps.mp4"); }
    static QVariantList segs(std::initializer_list<std::pair<int, int>> list)
    {
        QVariantList out;
        for (auto &p : list)
            out << QVariant(QVariantList{p.first, p.second});
        return out;
    }

    // An opened video with its first picture shown.
    bool openIt(VideoToGif &v)
    {
        QSignalSpy loaded(&v, &VideoToGif::loadedChanged);
        v.open(video());
        return loaded.wait(10000) && v.loaded();
    }

private slots:
    void theVideoOpensWithItsSizeAndDuration()
    {
        VideoToGif v;
        QVERIFY2(openIt(v), "the video did not open");
        QCOMPARE(v.sourceWidth(), 64);
        QCOMPARE(v.sourceHeight(), 48);
        QVERIFY(qAbs(v.durationMs() - 2000) <= 150);
        QCOMPARE(v.fileName(), QStringLiteral("video_20frames_10fps.mp4"));
        QVERIFY(v.isVideo(video()));
        QVERIFY(!v.isVideo(QStringLiteral("picture.png")));
    }

    void theFrameCountFollowsThePiecesAndTheRate()
    {
        QCOMPARE(VideoToGif::frameCount(segs({{0, 2000}}), 10), 20);
        QCOMPARE(VideoToGif::frameCount(segs({{0, 500}, {1000, 1500}}), 10), 10);
        QCOMPARE(VideoToGif::frameCount(segs({{0, 1000}}), 20), 20);
        QCOMPARE(VideoToGif::frameCount(segs({{300, 300}}), 10), 0);
    }

    void aWholeVideoBecomesItsFramesScaledToTheWidth()
    {
        VideoToGif v;
        QVERIFY2(openIt(v), "the video did not open");
        QSignalSpy ready(&v, &VideoToGif::framesReady);
        v.create(segs({{0, 2000}}), 10, 0, 0, 1, 1, 32);
        QVERIFY(v.busy());
        QVERIFY2(ready.wait(15000), "no frames came");
        const QList<QImage> frames = ready.last().at(0).value<QList<QImage>>();
        QCOMPARE(frames.size(), 20);
        QCOMPARE(ready.last().at(1).toInt(), 100);
        for (int i = 0; i < 20; ++i) {
            QCOMPARE(frames[i].size(), QSize(32, 24));
            QVERIFY2(qAbs(grayOf(frames[i]) - (10 + 12 * i)) <= 6, qPrintable(QStringLiteral("frame %1 gray %2").arg(i).arg(grayOf(frames[i]))));
        }
        QVERIFY(!v.busy());
    }

    void piecesInTheMiddleAreLeftOut()
    {
        VideoToGif v;
        QVERIFY2(openIt(v), "the video did not open");
        QSignalSpy ready(&v, &VideoToGif::framesReady);
        // frames 0-4 and 10-14 only
        v.create(segs({{0, 500}, {1000, 1500}}), 10, 0, 0, 1, 1, 64);
        QVERIFY2(ready.wait(15000), "no frames came");
        const QList<QImage> frames = ready.last().at(0).value<QList<QImage>>();
        QCOMPARE(frames.size(), 10);
        const int wanted[] = {0, 1, 2, 3, 4, 10, 11, 12, 13, 14};
        for (int i = 0; i < 10; ++i)
            QVERIFY2(qAbs(grayOf(frames[i]) - (10 + 12 * wanted[i])) <= 6, qPrintable(QStringLiteral("slot %1 gray %2").arg(i).arg(grayOf(frames[i]))));
    }

    void aCropKeepsOnlyThatPartOfThePicture()
    {
        VideoToGif v;
        QVERIFY2(openIt(v), "the video did not open");
        QSignalSpy ready(&v, &VideoToGif::framesReady);
        // a square 48x48 taken from the right of 64x48, shown at 24 px
        v.create(segs({{0, 500}}), 10, 0.25, 0, 0.75, 1, 24);
        QVERIFY2(ready.wait(15000), "no frames came");
        const QList<QImage> frames = ready.last().at(0).value<QList<QImage>>();
        QCOMPARE(frames.size(), 5);
        QCOMPARE(frames[0].size(), QSize(24, 24));
    }

    void cancellingDropsEverythingAndStopsBeingBusy()
    {
        VideoToGif v;
        QVERIFY2(openIt(v), "the video did not open");
        QSignalSpy ready(&v, &VideoToGif::framesReady);
        QSignalSpy cancelled(&v, &VideoToGif::cancelled);
        v.create(segs({{0, 2000}}), 10, 0, 0, 1, 1, 32);
        v.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!v.busy(), 5000);
        QCOMPARE(cancelled.size(), 1);
        QCOMPARE(ready.size(), 0);
    }

    // Not part of the normal run: PROJECTFOXY_TEST_VIDEO=<file> makes a 10 fps, 480 px wide, square crop of the middle.
    void aRealVideoCroppedAndScaled()
    {
        const QString path = qEnvironmentVariable("PROJECTFOXY_TEST_VIDEO");
        if (path.isEmpty())
            QSKIP("PROJECTFOXY_TEST_VIDEO is not set");
        VideoToGif v;
        QSignalSpy loaded(&v, &VideoToGif::loadedChanged);
        v.open(path);
        QVERIFY2(loaded.wait(15000) && v.loaded(), "the video did not open");
        const qreal cw = qreal(v.sourceHeight()) / v.sourceWidth(); // a square, as a fraction of the width
        QSignalSpy ready(&v, &VideoToGif::framesReady);
        QElapsedTimer timer;
        timer.start();
        v.create(segs({{0, v.durationMs()}}), 10, (1 - cw) / 2, 0, cw, 1, 480);
        QVERIFY2(ready.wait(180000), "no frames came");
        const QList<QImage> frames = ready.last().at(0).value<QList<QImage>>();
        qInfo() << frames.size() << "frames" << frames.first().size() << "from a" << v.sourceWidth() << "x" << v.sourceHeight() << "video in" << timer.elapsed() << "ms";
        QCOMPARE(frames.first().size(), QSize(480, 480));
    }

    void aFileThatIsNotAVideoReportsAnError()
    {
        // (a picture is no test: FFmpeg opens a PNG as a video of one frame)
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("not a video.mp4")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write(QByteArray("this is text, not a video ").repeated(400));
        fake.close();
        VideoToGif v;
        v.open(fake.fileName());
        QTRY_VERIFY_WITH_TIMEOUT(v.error() == QLatin1String("open"), 15000);
        QVERIFY(!v.loaded());
    }
};

QTEST_MAIN(TestVideoToGif)
#include "test_video_to_gif.moc"
