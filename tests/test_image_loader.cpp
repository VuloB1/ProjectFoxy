#include "DecoderRegistry.h"
#include "ImageLoader.h"
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtTest>
#include <atomic>

using core::DecodeResult;
using core::IImageDecoder;

namespace {

// A decoder the tests control: the file NAME says how it behaves, no file needs to
// exist. "slow*" takes 300 ms, "fast*" 10 ms; "*bad*" fails after that time instead
// of producing a picture. The pixels' red channel tells which file made them.
class FakeDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override { return filePath.endsWith(QLatin1String(".fake")); }
    QStringList supportedExtensions() const override { return {QStringLiteral("fake")}; }

    DecodeResult decode(const QString &filePath, QSize) override
    {
        const QString name = QFileInfo(filePath).fileName();
        QThread::msleep(name.startsWith(QLatin1String("slow")) ? 300 : 10);
        ++decodes;

        DecodeResult result;
        if (name.contains(QLatin1String("bad"))) {
            result.error = QStringLiteral("decoder says no: %1").arg(name);
            return result;
        }
        result.ok = true;
        result.image = QImage(4, 4, QImage::Format_RGBA8888);
        result.image.fill(QColor(name.startsWith(QLatin1String("slow")) ? 255 : 0, 0, 0));
        result.sourceSize = result.image.size();
        return result;
    }

    std::atomic<int> decodes{0};
};

} // namespace

class TestImageLoader : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        m_decoder = std::make_shared<FakeDecoder>();
        core::DecoderRegistry::instance().registerDecoder(m_decoder);
    }

    // The scenario that used to publish the OLD picture over the NEW one: a slow load
    // is superseded by a fast one under the same request id, and finishes last.
    void newestRequestWins()
    {
        core::ImageLoader loader;
        QStringList loaded;
        connect(&loader, &core::ImageLoader::loaded, this,
                [&](const QString &, core::ImageDocument doc) { loaded << QFileInfo(doc.filePath).fileName(); });

        loader.requestLoad(QStringLiteral("current"), QStringLiteral("slow_a.fake"));
        loader.requestLoad(QStringLiteral("current"), QStringLiteral("fast_b.fake"));
        QTest::qWait(700); // long enough for the slow decode to have finished too

        QCOMPARE(loaded, QStringList{QStringLiteral("fast_b.fake")});
    }

    // Same, but the superseded request FAILS late: its error must not be shown over
    // the picture that loaded fine.
    void staleFailureIsIgnored()
    {
        core::ImageLoader loader;
        int loadedCount = 0;
        QStringList errors;
        connect(&loader, &core::ImageLoader::loaded, this, [&](const QString &, core::ImageDocument) { ++loadedCount; });
        connect(&loader, &core::ImageLoader::failed, this, [&](const QString &, QString error) { errors << error; });

        loader.requestLoad(QStringLiteral("current"), QStringLiteral("slow_bad.fake"));
        loader.requestLoad(QStringLiteral("current"), QStringLiteral("fast_ok.fake"));
        QTest::qWait(700);

        QCOMPARE(loadedCount, 1);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('|'))));
    }

    // The newest request is the one that is delivered even when it is the one that
    // fails: the user asked for it last, so its error is the answer.
    void newestFailureIsReported()
    {
        core::ImageLoader loader;
        int loadedCount = 0;
        QStringList errors;
        connect(&loader, &core::ImageLoader::loaded, this, [&](const QString &, core::ImageDocument) { ++loadedCount; });
        connect(&loader, &core::ImageLoader::failed, this, [&](const QString &, QString error) { errors << error; });

        loader.requestLoad(QStringLiteral("current"), QStringLiteral("slow_ok.fake"));
        loader.requestLoad(QStringLiteral("current"), QStringLiteral("fast_bad.fake"));
        QTest::qWait(700);

        QCOMPARE(loadedCount, 0);
        QCOMPARE(errors.size(), 1);
        QVERIFY(errors.first().contains(QLatin1String("fast_bad")));
    }

    void cancelSuppressesTheResult()
    {
        core::ImageLoader loader;
        int signalCount = 0;
        connect(&loader, &core::ImageLoader::loaded, this, [&](const QString &, core::ImageDocument) { ++signalCount; });
        connect(&loader, &core::ImageLoader::failed, this, [&](const QString &, QString) { ++signalCount; });

        loader.requestLoad(QStringLiteral("current"), QStringLiteral("slow_a.fake"));
        loader.cancel(QStringLiteral("current"));
        QTest::qWait(600);

        QCOMPARE(signalCount, 0);
    }

    // Requests with different ids are independent of each other.
    void differentIdsDoNotCancelEachOther()
    {
        core::ImageLoader loader;
        QStringList ids;
        connect(&loader, &core::ImageLoader::loaded, this,
                [&](const QString &id, core::ImageDocument) { ids << id; });

        loader.requestLoad(QStringLiteral("one"), QStringLiteral("slow_a.fake"));
        loader.requestLoad(QStringLiteral("two"), QStringLiteral("fast_b.fake"));
        QTest::qWait(700);

        ids.sort();
        QCOMPARE(ids, (QStringList{QStringLiteral("one"), QStringLiteral("two")}));
    }

    // A request that is replaced while it is still waiting in the queue is not even
    // decoded (one worker thread, a long first job holding it up).
    void supersededRequestsAreNotDecoded()
    {
        core::ImageLoader loader;
        const int before = m_decoder->decodes;

        // More requests for one id than the pool has threads, issued back to back: only
        // the last one counts, so those still queued when it arrives must be skipped.
        const int requests = QThread::idealThreadCount() + 5;
        for (int i = 0; i < requests; ++i)
            loader.requestLoad(QStringLiteral("current"), QStringLiteral("slow_%1.fake").arg(i));
        loader.requestLoad(QStringLiteral("current"), QStringLiteral("fast_last.fake"));
        QTest::qWait(2000);

        // Without the early-out every single request would have been decoded.
        const int decoded = m_decoder->decodes - before;
        QVERIFY2(decoded < requests + 1, qPrintable(QStringLiteral("%1 decodes for %2 requests").arg(decoded).arg(requests + 1)));
    }

    // A picture prefetched is decoded once: opening it afterwards is answered from the cache, and a file that was
    // written over since is decoded again.
    void prefetchedPicturesAreNotDecodedAgain()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("fast_cached.fake"));
        auto write = [&](const QByteArray &bytes) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        };
        write("one");

        core::ImageLoader loader;
        int loadedCount = 0;
        connect(&loader, &core::ImageLoader::loaded, this, [&](const QString &, core::ImageDocument) { ++loadedCount; });
        const int before = m_decoder->decodes;

        loader.prefetch({path});
        QTest::qWait(300);
        loader.requestLoad(QStringLiteral("current"), path);
        QTRY_COMPARE(loadedCount, 1);
        loader.requestLoad(QStringLiteral("current"), path);
        QTRY_COMPARE(loadedCount, 2);
        QCOMPARE(m_decoder->decodes - before, 1); // prefetch decoded it; both opens came from the cache

        write("a different, longer file"); // size changed: the cached picture is stale
        loader.requestLoad(QStringLiteral("current"), path);
        QTRY_COMPARE(loadedCount, 3);
        QCOMPARE(m_decoder->decodes - before, 2);
    }

    // A big JPEG is first shown reduced (previewLoaded), then whole (loaded), in that order; once it is in the cache
    // there is no reduced look any more, the whole picture comes at once.
    void bigJpegShowsAReducedLookFirst()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QImage noise(3000, 2200, QImage::Format_RGB32);
        for (int y = 0; y < noise.height(); ++y)
            for (int x = 0; x < noise.width(); ++x)
                noise.setPixel(x, y, qRgb(QRandomGenerator::global()->bounded(256), (x * 7 + y) & 255, y & 255));
        const QString path = dir.filePath(QStringLiteral("big.jpg"));
        QVERIFY(noise.save(path, "JPG", 95));
        QVERIFY(QFileInfo(path).size() > (1 << 20));

        core::ImageLoader loader;
        QStringList order;
        QSize previewSize, previewSource, fullSize;
        connect(&loader, &core::ImageLoader::previewLoaded, this, [&](const QString &, core::ImageDocument d) {
            order << QStringLiteral("preview");
            previewSize = d.pixels.size();
            previewSource = d.sourceSize;
            QVERIFY(d.isPreview);
        });
        connect(&loader, &core::ImageLoader::loaded, this, [&](const QString &, core::ImageDocument d) {
            order << QStringLiteral("loaded");
            fullSize = d.pixels.size();
        });

        loader.requestLoad(QStringLiteral("current"), path);
        QTRY_COMPARE_WITH_TIMEOUT(order.size(), 2, 10000);
        QCOMPARE(order, (QStringList{QStringLiteral("preview"), QStringLiteral("loaded")}));
        QCOMPARE(fullSize, noise.size());
        QCOMPARE(previewSource, noise.size());
        QVERIFY(qMax(previewSize.width(), previewSize.height()) <= core::kQuickPreviewSide);

        order.clear(); // now cached
        loader.requestLoad(QStringLiteral("current"), path);
        QTRY_COMPARE(order, (QStringList{QStringLiteral("loaded")}));
    }

private:
    std::shared_ptr<FakeDecoder> m_decoder;
};

QTEST_MAIN(TestImageLoader)
#include "test_image_loader.moc"
