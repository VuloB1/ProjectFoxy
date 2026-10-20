#include "VideoSupport.h"

#include <QMediaFormat>
#include <QtTest>

// The video backend is there and reads what people record or download: MP4 (H.264), QuickTime, Matroska (OBS).
// (With the Qt 6.7.3 binaries on Windows the backend decodes H.264/H.265/MPEG but not WebM/VP9/AV1.)
class TestMultimedia : public QObject {
    Q_OBJECT

private slots:
    void videoSupportListsTheUsualExtensions()
    {
        VideoSupport support;
        QVERIFY(support.available());
        for (const char *ext : {"mp4", "mov", "mkv"}) // WebM / VP9 are not in the backend Qt ships on Windows: see the next test
            QVERIFY2(support.extensions().contains(QLatin1String(ext)), ext);
    }

    void theBackendReadsTheUsualVideoFiles()
    {
        QMediaFormat format;
        const auto files = format.supportedFileFormats(QMediaFormat::Decode);
        qInfo() << "decodable containers:" << files;
        QVERIFY(files.contains(QMediaFormat::MPEG4));
        QVERIFY(files.contains(QMediaFormat::QuickTime));
        QVERIFY(files.contains(QMediaFormat::Matroska));
    }

    void theVideoCodecsAreListed()
    {
        // Which codecs the list names depends on the FFmpeg build behind Qt (the Linux one decodes MP4/H.264 without listing
        // it): the formats above are what matters, this only shows what there is.
        QMediaFormat format;
        qInfo() << "decodable video codecs:" << format.supportedVideoCodecs(QMediaFormat::Decode);
        QVERIFY(!format.supportedVideoCodecs(QMediaFormat::Decode).isEmpty());
    }
};

QTEST_MAIN(TestMultimedia)
#include "test_multimedia.moc"
