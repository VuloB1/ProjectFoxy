#include "VideoFrameReader.h"

#include <QElapsedTimer>
#include <QtTest>

// tests/fixtures/video_20frames_10fps.mp4: 64x48 H.264, 20 frames at 10 fps (a key frame every 5), frame i is
// solid gray 10 + 12*i. It tells which frame a request really got.
class TestVideoFrames : public QObject {
    Q_OBJECT

    static int grayOf(const QImage &image) { return qGray(image.pixel(image.width() / 2, image.height() / 2)); }
    static QString video() { return QStringLiteral(FIXTURES_DIR "/video_20frames_10fps.mp4"); }

private slots:
    void theVideoOpensWithItsDuration()
    {
        VideoFrameReader reader;
        QSignalSpy opened(&reader, &VideoFrameReader::opened);
        reader.open(video());
        QVERIFY2(opened.wait(10000), "the video did not open");
        qInfo() << "duration" << reader.durationMs() << "ms";
        QVERIFY(qAbs(reader.durationMs() - 2000) <= 150);
    }

    void aRequestedTimeGivesTheFrameShownThen()
    {
        VideoFrameReader reader;
        QSignalSpy opened(&reader, &VideoFrameReader::opened);
        QSignalSpy frames(&reader, &VideoFrameReader::frameReady);
        QSignalSpy failed(&reader, &VideoFrameReader::failed);
        reader.open(video());
        QVERIFY2(opened.wait(10000), "the video did not open");

        // the middle of frames 0, 7 (inside a group of 5: needs the key frame before it), 13 and the last one
        for (int index : {0, 7, 13, 19}) {
            frames.clear();
            QElapsedTimer timer;
            timer.start();
            reader.requestFrame(index * 100 + 50);
            const QString why = failed.isEmpty() ? QString() : failed.last().at(0).toString();
            QVERIFY2(frames.wait(5000), qPrintable(QStringLiteral("no frame for %1 %2").arg(index).arg(why)));
            const QImage image = frames.last().at(1).value<QImage>();
            const qint64 start = frames.last().at(0).toLongLong();
            qInfo() << "frame" << index << "starts at" << start << "ms, gray" << grayOf(image) << "after" << timer.elapsed() << "ms";
            QCOMPARE(image.size(), QSize(64, 48));
            QVERIFY2(qAbs(grayOf(image) - (10 + 12 * index)) <= 6,
                     qPrintable(QStringLiteral("frame %1 came out gray %2").arg(index).arg(grayOf(image))));
        }
    }

    void everyFrameCanBeAskedInOrderAndHowLongThatTakes()
    {
        VideoFrameReader reader;
        QSignalSpy opened(&reader, &VideoFrameReader::opened);
        QSignalSpy frames(&reader, &VideoFrameReader::frameReady);
        reader.open(video());
        QVERIFY2(opened.wait(10000), "the video did not open");

        QElapsedTimer timer;
        timer.start();
        int right = 0;
        for (int index = 0; index < 20; ++index) {
            frames.clear();
            reader.requestFrame(index * 100 + 50);
            if (!frames.wait(5000))
                continue;
            if (qAbs(grayOf(frames.last().at(1).value<QImage>()) - (10 + 12 * index)) <= 6)
                ++right;
        }
        qInfo() << right << "of 20 frames right in" << timer.elapsed() << "ms";
        QCOMPARE(right, 20);
    }

    // Not part of the normal run: PROJECTFOXY_TEST_VIDEO=<file> times the same thing on a real video (a frame every
    // 100 ms, as a 10 fps GIF would take them).
    void aRealVideoAtTenFramesPerSecond()
    {
        const QString path = qEnvironmentVariable("PROJECTFOXY_TEST_VIDEO");
        if (path.isEmpty())
            QSKIP("PROJECTFOXY_TEST_VIDEO is not set");
        VideoFrameReader reader;
        QSignalSpy opened(&reader, &VideoFrameReader::opened);
        QSignalSpy frames(&reader, &VideoFrameReader::frameReady);
        reader.open(path);
        QVERIFY2(opened.wait(15000), "the video did not open");
        const qint64 duration = reader.durationMs();
        QElapsedTimer timer;
        timer.start();
        int got = 0, asked = 0;
        for (qint64 t = 0; t < duration; t += 100, ++asked) {
            frames.clear();
            reader.requestFrame(t + 10);
            if (frames.wait(5000))
                ++got;
            else
                qInfo() << "no frame came for" << (t + 10) << "ms";
        }
        qInfo() << got << "of" << asked << "frames of a" << duration << "ms video," << reader.frameSize() << "in" << timer.elapsed()
                << "ms:" << (timer.elapsed() / qMax(1, got)) << "ms per frame";
        QCOMPARE(got, asked);
    }
};

QTEST_MAIN(TestVideoFrames)
#include "test_video_frames.moc"
