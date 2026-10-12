#include "PictureCache.h"
#include <QtTest>

using core::edit::AdjustOp;

namespace {

QImage solid(int w, int h, QColor c)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(c);
    return img;
}

int peakBin(const QVariantList &bins)
{
    int best = 0;
    for (int i = 1; i < bins.size(); ++i)
        if (bins.at(i).toDouble() > bins.at(best).toDouble())
            best = i;
    return best;
}

} // namespace

// What is derived from the picture and kept until it changes: made once, made again only after the right
// invalidation, and never stale.
class TestPictureCache : public QObject {
    Q_OBJECT

private slots:
    void theStructuralPictureIsMadeOnceUntilInvalidated()
    {
        int calls = 0;
        QImage current = solid(10, 10, Qt::red);
        PictureCache cache([&]() { ++calls; return current; });
        QVERIFY(!cache.structuralIsReady());
        QVERIFY(cache.structuralIfReady().isNull()); // would have to be calculated: the caller replays instead
        QCOMPARE(calls, 0);                          // nothing is made until it is asked for

        QCOMPARE(cache.structural().pixelColor(1, 1), QColor(Qt::red));
        cache.structural();
        QCOMPARE(calls, 1);
        QVERIFY(cache.structuralIsReady());
        QCOMPARE(cache.structuralIfReady().size(), QSize(10, 10));

        current = solid(10, 10, Qt::blue);
        QCOMPARE(cache.structural().pixelColor(1, 1), QColor(Qt::red)); // still the cached one
        cache.invalidatePicture();
        QVERIFY(!cache.structuralIsReady());
        QCOMPARE(cache.structural().pixelColor(1, 1), QColor(Qt::blue));
        QCOMPARE(calls, 2);
    }

    void aKnownResultIsUsedWithoutCalculating()
    {
        int calls = 0;
        PictureCache cache([&]() { ++calls; return solid(4, 4, Qt::red); });
        cache.setStructural(solid(6, 6, Qt::green)); // e.g. the exact result of an effect, or a save worker's
        QVERIFY(cache.structuralIsReady());
        QCOMPARE(cache.structural().size(), QSize(6, 6));
        QCOMPARE(calls, 0);
    }

    void aSetResultDoesNotRescueTheThingsDerivedFromTheOldOne()
    {
        QImage current = solid(20, 20, Qt::black);
        PictureCache cache([&]() { return current; });
        const int darkPeak = peakBin(cache.histogramChannels().value("luma").toList());
        QCOMPARE(darkPeak, 0);
        cache.invalidatePicture();
        cache.setStructural(solid(20, 20, Qt::white)); // invalidate first, then hand over the result: the usual order
        QCOMPARE(peakBin(cache.histogramChannels().value("luma").toList()), 255);
    }

    void theHistogramFollowsThePicture()
    {
        QImage current = solid(20, 20, Qt::black);
        PictureCache cache([&]() { return current; });
        const QVariantList luma = cache.histogramChannels().value("luma").toList();
        QCOMPARE(luma.size(), 256);
        QCOMPARE(peakBin(luma), 0);
        QCOMPARE(cache.histogram().size(), 256);
        for (const char *ch : {"r", "g", "b"})
            QCOMPARE(cache.histogramChannels().value(QLatin1String(ch)).toList().size(), 256);

        current = solid(20, 20, Qt::white);
        QCOMPARE(peakBin(cache.histogramChannels().value("luma").toList()), 0); // not asked to change: still the old one
        cache.invalidatePicture();
        QCOMPARE(peakBin(cache.histogramChannels().value("luma").toList()), 255);
    }

    void theSmallCopyIsAtMost384AndKeepsTheShape()
    {
        PictureCache big([]() { return solid(1920, 1080, Qt::red); });
        QCOMPARE(big.proxy().size(), QSize(384, 216));
        PictureCache small([]() { return solid(100, 50, Qt::red); });
        QCOMPARE(small.proxy().size(), QSize(100, 50)); // not enlarged
        PictureCache none([]() { return QImage(); });
        QVERIFY(none.proxy().isNull());
        QVERIFY(none.structural().isNull());
    }

    void theAdjustedHistogramFollowsTheLiveOverlayAndIsCachedUntilTold()
    {
        PictureCache cache([]() { return solid(40, 40, QColor(100, 100, 100)); });
        AdjustOp neutral;
        const int plain = peakBin(cache.adjustedHistogram(neutral, QString(), 1.0).value("luma").toList());

        AdjustOp bright;
        bright.brightness = 0.6;
        // same cache, other overlay, but nobody said the live state changed: the old answer
        QCOMPARE(peakBin(cache.adjustedHistogram(bright, QString(), 1.0).value("luma").toList()), plain);
        cache.invalidateAdjusted();
        const int lifted = peakBin(cache.adjustedHistogram(bright, QString(), 1.0).value("luma").toList());
        QVERIFY2(lifted > plain, qPrintable(QString("%1 vs %2").arg(lifted).arg(plain)));

        cache.invalidateAdjusted();
        QCOMPARE(peakBin(cache.adjustedHistogram(neutral, QString(), 1.0).value("luma").toList()), plain);
    }

    void aNewPictureAlsoInvalidatesTheAdjustedHistogramAndTheLookPreviews()
    {
        QImage current = solid(40, 40, QColor(30, 30, 30));
        PictureCache cache([&]() { return current; });
        const AdjustOp neutral;
        const int dark = peakBin(cache.adjustedHistogram(neutral, QString(), 1.0).value("luma").toList());
        QVERIFY(cache.lookThumbsStale());
        cache.lookThumbsMade();
        QVERIFY(!cache.lookThumbsStale());

        current = solid(40, 40, QColor(220, 220, 220));
        cache.invalidatePicture();
        QVERIFY(cache.lookThumbsStale()); // the previews were made from the old pixels
        QVERIFY(peakBin(cache.adjustedHistogram(neutral, QString(), 1.0).value("luma").toList()) > dark + 100);
    }

    void aLiveChangeLeavesThePicturesCachesAlone()
    {
        int calls = 0;
        PictureCache cache([&]() { ++calls; return solid(20, 20, Qt::gray); });
        cache.structural();
        cache.proxy();
        cache.histogramChannels();
        cache.lookThumbsMade();
        cache.invalidateAdjusted(); // a slider moved
        cache.structural();
        cache.proxy();
        cache.histogramChannels();
        QCOMPARE(calls, 1);                 // nothing about the pixels was recalculated
        QVERIFY(!cache.lookThumbsStale());  // and the look previews are still good
    }
};

QTEST_MAIN(TestPictureCache)
#include "test_picture_cache.moc"
