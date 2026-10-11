#include "EffectPreview.h"
#include <QtTest>

namespace {

QImage gradient(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.setPixelColor(x, y, QColor(x * 255 / w, y * 255 / h, (x + y) * 255 / (w + h)));
    return img;
}

} // namespace

// The effect preview: what is picked, its sliders, and the calculations behind them (a generation per change, stale
// results never shown, the exact result kept for saving).
class TestEffectPreview : public QObject {
    Q_OBJECT

    const QImage m_picture = gradient(120, 80);
    EffectPreview::BaseProvider base() const
    {
        return [picture = m_picture]() { return picture; };
    }

private slots:
    void anUnknownEffectIsIgnored()
    {
        EffectPreview preview(base());
        QSignalSpy changed(&preview, &EffectPreview::changed);
        preview.select(QStringLiteral("no-such-effect"));
        QVERIFY(!preview.active());
        QCOMPARE(changed.count(), 0);
    }

    void pickingAnEffectPreviewsItAndKeepsTheExactResult()
    {
        EffectPreview preview(base());
        QSignalSpy changed(&preview, &EffectPreview::changed);
        QSignalSpy active(&preview, &EffectPreview::activeChanged);
        QSignalSpy ready(&preview, &EffectPreview::pictureReady);
        preview.select(QStringLiteral("posterize"));
        QVERIFY(preview.active());
        QCOMPARE(preview.id(), QStringLiteral("posterize"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(active.count(), 1);
        QVERIFY(preview.busy());

        QTRY_VERIFY_WITH_TIMEOUT(ready.count() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.busy(), 5000);
        const QImage shown = ready.last().at(0).value<QImage>();
        QCOMPARE(shown.size(), m_picture.size());
        QVERIFY(shown != m_picture); // posterized: not the original
        QVERIFY(preview.exactReady());
        QCOMPARE(preview.exactResult(), shown);
        QCOMPARE(preview.applyTo(m_picture), shown); // what a save would use: the exact one, not recalculated

        preview.select(QStringLiteral("blur")); // another effect: the same "active" session, no second activeChanged
        QCOMPARE(active.count(), 1);
    }

    void slidersAreClampedAndRoundedAndStartANewPreview()
    {
        EffectPreview preview(base());
        preview.select(QStringLiteral("posterize")); // "Niveles": 2..16, whole numbers, default 4
        QCOMPARE(preview.values().size(), 1);
        QCOMPARE(preview.values().at(0).toDouble(), 4.0);
        QSignalSpy values(&preview, &EffectPreview::valuesChanged);
        preview.setValue(0, 99);
        QCOMPARE(preview.values().at(0).toDouble(), 16.0);
        QCOMPARE(values.count(), 1);
        preview.setValue(0, 99); // same value: nothing
        QCOMPARE(values.count(), 1);
        preview.setValue(0, 7.6);
        QCOMPARE(preview.values().at(0).toDouble(), 8.0);
        preview.setValue(5, 1); // no such slider
        QCOMPARE(preview.values().size(), 1);
        preview.resetValues();
        QCOMPARE(preview.values().at(0).toDouble(), 4.0);
        QCOMPARE(preview.mix(), 1.0);
        QCOMPARE(preview.operation().effectId, QStringLiteral("posterize"));
    }

    void aNewChangeMakesTheOlderCalculationStale()
    {
        EffectPreview preview(base());
        QSignalSpy ready(&preview, &EffectPreview::pictureReady);
        preview.select(QStringLiteral("posterize"));
        preview.setValue(0, 2); // before anything was calculated
        QTRY_VERIFY_WITH_TIMEOUT(ready.count() >= 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!preview.busy(), 5000);
        // two levels: only a handful of distinct colours; four levels would give many more
        QSet<QRgb> colours;
        const QImage shown = ready.last().at(0).value<QImage>();
        for (int y = 0; y < shown.height(); ++y)
            for (int x = 0; x < shown.width(); ++x)
                colours.insert(shown.pixel(x, y));
        QVERIFY2(colours.size() <= 8, qPrintable(QString::number(colours.size())));
    }

    void droppingBeforeAnythingIsShownShowsNothing()
    {
        EffectPreview preview(base());
        QSignalSpy ready(&preview, &EffectPreview::pictureReady);
        QSignalSpy active(&preview, &EffectPreview::activeChanged);
        preview.select(QStringLiteral("posterize"));
        QVERIFY(!preview.drop()); // nothing had reached the canvas
        QVERIFY(!preview.active());
        QCOMPARE(active.count(), 2); // started, stopped
        QTest::qWait(400);
        QCOMPARE(ready.count(), 0); // the calculation that was queued is stale
        QVERIFY(!preview.exactReady());
        QCOMPARE(preview.applyTo(m_picture), m_picture); // no effect: the picture as it is
    }

    void droppingAfterAPreviewWasShownSaysSoToTheCaller()
    {
        EffectPreview preview(base());
        QSignalSpy ready(&preview, &EffectPreview::pictureReady);
        preview.select(QStringLiteral("posterize"));
        QTRY_VERIFY_WITH_TIMEOUT(ready.count() >= 1, 5000);
        QVERIFY(preview.drop()); // the canvas shows a preview: the caller must put the real picture back
        QVERIFY(!preview.drop());
    }

    void aMixOfZeroChangesNothing()
    {
        EffectPreview preview(base());
        preview.select(QStringLiteral("posterize"));
        preview.setMix(0.0);
        QCOMPARE(preview.mix(), 0.0);
        QCOMPARE(preview.applyTo(m_picture), m_picture);
        preview.setMix(5.0);
        QCOMPARE(preview.mix(), 1.0); // clamped
    }

    void destroyingItWhileItCalculatesIsSafe()
    {
        {
            EffectPreview preview(base());
            preview.select(QStringLiteral("blur"));
            preview.setValue(0, 90);
        } // ~EffectPreview: cancel, then wait for the workers
        QTest::qWait(300);
    }
};

QTEST_MAIN(TestEffectPreview)
#include "test_effect_preview.moc"
