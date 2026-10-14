#include "edit/Cas.h"

#include <QtTest>

// FidelityFX CAS (core/edit/Cas.h): what the sharpening promises, one pixel at a time.
using core::edit::casChannel;

class TestCas : public QObject {
    Q_OBJECT

private slots:
    void strengthZeroAndFlatAreasLeaveThePixelAlone()
    {
        const float edge[9] = {0.2f, 0.2f, 0.8f, 0.2f, 0.4f, 0.8f, 0.2f, 0.2f, 0.8f};
        QCOMPARE(casChannel(edge, 0.0f), 0.4f); // off
        const float flat[9] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        QCOMPARE(casChannel(flat, 1.0f), 0.5f); // nothing to sharpen
        const float black[9] = {};
        QCOMPARE(casChannel(black, 1.0f), 0.0f); // no division by zero in the dark
        const float white[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
        QCOMPARE(casChannel(white, 1.0f), 1.0f);
    }

    void anEdgeGetsMoreContrastAndMoreAsTheStrengthGrows()
    {
        // the pixel on the dark side of the edge gets darker, the one on the light side lighter
        const float dark[9] = {0.3f, 0.35f, 0.6f, 0.3f, 0.35f, 0.6f, 0.3f, 0.35f, 0.6f};
        const float light[9] = {0.3f, 0.55f, 0.6f, 0.3f, 0.55f, 0.6f, 0.3f, 0.55f, 0.6f};
        float lastDark = dark[4], lastLight = light[4];
        for (float s : {0.25f, 0.5f, 0.75f, 1.0f}) {
            const float d = casChannel(dark, s), l = casChannel(light, s);
            QVERIFY2(d < lastDark, "the dark side goes darker, more with more strength");
            QVERIFY2(l > lastLight, "the light side goes lighter, more with more strength");
            lastDark = d;
            lastLight = l;
        }
    }

    void theResultNeverLeavesZeroToOneAndNeverIsNotANumber()
    {
        // every combination of extreme neighbours
        for (int mask = 0; mask < 512; ++mask) {
            float n[9];
            for (int k = 0; k < 9; ++k)
                n[k] = (mask >> k) & 1 ? 1.0f : 0.0f;
            for (float s : {0.1f, 0.6f, 1.0f}) {
                const float out = casChannel(n, s);
                QVERIFY(out >= 0.0f && out <= 1.0f);
                QVERIFY(!std::isnan(out));
            }
        }
    }

    void whereTheNeighbourhoodAlreadyTouchesBlackOrWhiteThereIsNoRoomToAdd()
    {
        // the "adaptive" part: the smooth minimum is 0 (it touches black), so the amount is 0 and the pixel stays
        const float touchesBlack[9] = {0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 1.0f, 0.0f, 0.0f, 1.0f};
        QCOMPARE(casChannel(touchesBlack, 1.0f), 0.1f);
        // the same step in the middle of the range has room, and does change
        const float midGrey[9] = {0.4f, 0.4f, 0.6f, 0.4f, 0.45f, 0.6f, 0.4f, 0.4f, 0.6f};
        QVERIFY(casChannel(midGrey, 1.0f) != 0.45f);
    }
};

QTEST_MAIN(TestCas)
#include "test_cas.moc"
