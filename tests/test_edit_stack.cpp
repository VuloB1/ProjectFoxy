#include <QtTest>
#include <QPainter>
#include "edit/AdjustMath.h"
#include "edit/EditStack.h"
#include "edit/Effects.h"
#include "edit/Looks.h"
#include "edit/Resample.h"
#include <set>

using namespace core::edit;

namespace {

QImage solid(int r, int g, int b, int w = 8, int h = 8)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(r, g, b));
    return img;
}

QRgb firstPixel(const QImage &img)
{
    return img.pixel(0, 0);
}

// Statistics of the red channel of a flat gray picture after adding noise.
struct NoiseStats {
    double mean = 0.0, sigma = 0.0, kurtosis = 0.0;
};
NoiseStats noiseStats(const QImage &img)
{
    NoiseStats st;
    const double n = double(img.width()) * img.height();
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            st.mean += qRed(img.pixel(x, y));
    st.mean /= n;
    double m2 = 0.0, m4 = 0.0;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const double d = qRed(img.pixel(x, y)) - st.mean;
            m2 += d * d;
            m4 += d * d * d * d;
        }
    m2 /= n;
    m4 /= n;
    st.sigma = std::sqrt(m2);
    st.kurtosis = m4 / (m2 * m2);
    return st;
}

// High-frequency, fully opaque test picture: every neighborhood effect has
// something to chew on (a smooth gradient would leave a median untouched).
QImage busyPicture(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int r = (x * 37 + y * 91 + (x * y) % 17 * 13) % 256;
            const int g = (x * 53 + y * 29 + (x ^ y) * 7) % 256;
            const int b = (x * 11 + y * 71 + (x * x + y) % 23 * 9) % 256;
            img.setPixelColor(x, y, QColor(r, g, b));
        }
    return img;
}

// Soft shapes on a gradient: survives being scaled down and up, so two renders
// at different resolutions can be compared pixel by pixel.
QImage smoothPicture(int w, int h)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0, 0, w, h);
    grad.setColorAt(0.0, QColor(30, 60, 140));
    grad.setColorAt(1.0, QColor(240, 200, 90));
    p.fillRect(img.rect(), grad);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 60, 60));
    p.drawEllipse(QPointF(w * 0.3, h * 0.4), w * 0.12, w * 0.12);
    p.setBrush(QColor(40, 170, 90));
    p.drawRoundedRect(QRectF(w * 0.55, h * 0.2, w * 0.25, h * 0.5), w * 0.03, w * 0.03);
    return img;
}


} // namespace

class TestEditStack : public QObject {
    Q_OBJECT
private slots:
    void pushThenUndoRemovesOperation()
    {
        EditStack stack;
        stack.push(RotateOp{90});
        QCOMPARE(stack.operations().size(), size_t(1));
        QVERIFY(stack.undo());
        QVERIFY(!stack.undo());
    }

    void pushAfterUndoDropsRedoTail()
    {
        EditStack stack;
        stack.push(RotateOp{90});
        stack.push(RotateOp{180});
        stack.undo();
        stack.push(FlipOp{true, false});
        QCOMPARE(stack.operations().size(), size_t(2));
    }

    void defaultAdjustIsIdentity()
    {
        AdjustOp op;
        QVERIFY(op.isIdentity());

        const AdjustLut lut = buildAdjustLut(op);
        for (int i = 0; i < 256; ++i) {
            QCOMPARE(int(lut.r[i]), i);
            QCOMPARE(int(lut.g[i]), i);
            QCOMPARE(int(lut.b[i]), i);
        }

        const QImage src = solid(40, 120, 200);
        QCOMPARE(applyAdjustOp(src, op), src);
    }

    void anyChangedSliderIsNotIdentity()
    {
        AdjustOp op;
        op.vibrance = 0.1;
        QVERIFY(!op.isIdentity());

        AdjustOp leveled;
        leveled.levels[2].inBlack = 0.1;
        QVERIFY(!leveled.isIdentity());

        AdjustOp curved;
        curved.curves[0] = {QPointF(0, 0), QPointF(0.5, 0.7), QPointF(1, 1)};
        QVERIFY(!curved.isIdentity());
    }

    void exposureBrightensAndDarkens()
    {
        AdjustOp up;
        up.exposure = 0.5;
        AdjustOp down;
        down.exposure = -0.5;
        const AdjustLut lutUp = buildAdjustLut(up);
        const AdjustLut lutDown = buildAdjustLut(down);
        QVERIFY(lutUp.g[100] > 100);
        QVERIFY(lutDown.g[100] < 100);
        QCOMPARE(int(lutUp.g[0]), 0);   // black stays black
        QCOMPARE(int(lutUp.g[255]), 255);
    }

    void toneLutStaysMonotonic()
    {
        AdjustOp op;
        op.exposure = 0.3;
        op.gamma = -0.4;
        op.contrast = 0.5;
        op.blacks = 0.6;
        op.whites = -0.6;
        op.curves[0] = {QPointF(0, 0), QPointF(0.3, 0.2), QPointF(0.7, 0.85), QPointF(1, 1)};
        const AdjustLut lut = buildAdjustLut(op);
        for (int i = 1; i < 256; ++i)
            QVERIFY2(lut.r[i] >= lut.r[i - 1], qPrintable(QString("not monotonic at %1").arg(i)));
    }

    void negativeInverts()
    {
        AdjustOp op;
        op.negative = true;
        const QImage out = applyAdjustOp(solid(10, 100, 250), op);
        QCOMPARE(qRed(firstPixel(out)), 245);
        QCOMPARE(qGreen(firstPixel(out)), 155);
        QCOMPARE(qBlue(firstPixel(out)), 5);
    }

    void fullDesaturationGivesGray()
    {
        AdjustOp op;
        op.saturation = -1.0;
        const QRgb p = firstPixel(applyAdjustOp(solid(200, 60, 30), op));
        QVERIFY(qAbs(qRed(p) - qGreen(p)) <= 1);
        QVERIFY(qAbs(qGreen(p) - qBlue(p)) <= 1);
    }

    void warmTemperatureShiftsRedUpBlueDown()
    {
        AdjustOp op;
        op.temperature = 0.5;
        const QRgb p = firstPixel(applyAdjustOp(solid(128, 128, 128), op));
        QVERIFY(qRed(p) > 128);
        QCOMPARE(qGreen(p), 128);
        QVERIFY(qBlue(p) < 128);
    }

    void levelsClipAndStretch()
    {
        AdjustOp op;
        op.levels[0] = LevelsChannel{0.2, 1.0, 0.8};
        const AdjustLut lut = buildAdjustLut(op);
        QCOMPARE(int(lut.g[int(0.2 * 255)]), 0);
        QCOMPARE(int(lut.g[int(0.8 * 255) + 1]), 255);
        QVERIFY(lut.g[128] > 100 && lut.g[128] < 156); // midpoint stays near the middle
    }

    void curvePassesThroughItsPoints()
    {
        const auto curve = sampleCurve({QPointF(0, 0), QPointF(0.5, 0.75), QPointF(1, 1)});
        QVERIFY(qAbs(curve[0] - 0.0) < 1e-6);
        QVERIFY(qAbs(curve[255] - 1.0) < 1e-6);
        QVERIFY(qAbs(curve[128] - 0.75) < 0.02);
        for (size_t i = 1; i < curve.size(); ++i)
            QVERIFY(curve[i] >= curve[i - 1] - 1e-9); // never dips
    }

    void identityCurveSamplesAsDiagonal()
    {
        const auto curve = sampleCurve({});
        for (int i = 0; i < 256; ++i)
            QVERIFY(qAbs(curve[i] - i / 255.0) < 1e-9);
    }

    void shadowsLiftDarkPixelsOnly()
    {
        AdjustOp op;
        op.shadows = 1.0;
        const QRgb dark = firstPixel(applyAdjustOp(solid(30, 30, 30), op));
        const QRgb bright = firstPixel(applyAdjustOp(solid(230, 230, 230), op));
        QVERIFY(qRed(dark) > 30 + 20);
        QCOMPARE(qRed(bright), 230);
    }

    void autoContrastStretchesFlatImage()
    {
        // Values only between 90 and 160: auto contrast must pull the white
        // point in and the black point up.
        QImage img(64, 64, QImage::Format_RGBA8888);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const int v = 90 + (x + y) * 70 / 126;
                img.setPixelColor(x, y, QColor(v, v, v));
            }
        const AdjustOp op = autoAdjusted(AdjustOp(), img, AutoAdjustKind::Contrast);
        QVERIFY(op.levels[0].inBlack > 0.3);
        QVERIFY(op.levels[0].inWhite < 0.7);
        QVERIFY(!op.isIdentity());
    }

    void lookCatalogueIsWellFormed()
    {
        std::set<QString> ids;
        std::set<QString> groups;
        for (const LookGroup &g : lookGroups())
            groups.insert(g.id);
        QVERIFY(!allLooks().empty());
        for (const LookSpec &look : allLooks()) {
            QVERIFY2(!look.id.isEmpty() && !look.name.isEmpty(), qPrintable(look.id));
            QVERIFY2(ids.insert(look.id).second, qPrintable("duplicate id " + look.id));
            QVERIFY2(groups.count(look.group) == 1, qPrintable("unknown group for " + look.id));
            QVERIFY2(findLook(look.id) == &look, qPrintable(look.id));
        }
        QVERIFY(findLook(QString()) == nullptr);
        QVERIFY(findLook(QStringLiteral("no-such-look")) == nullptr);
    }

    void noLookAndZeroAmountLeaveTheImageAlone()
    {
        const QImage src = solid(40, 120, 200);
        QCOMPARE(applyLook(src, QString(), 1.0), src);
        QCOMPARE(applyLook(src, QStringLiteral("no-such-look"), 1.0), src);
        QCOMPARE(applyLook(src, QStringLiteral("cine"), 0.0), src);
    }

    void blackAndWhiteLookGivesGray()
    {
        const QRgb p = firstPixel(applyLook(solid(200, 60, 30), QStringLiteral("bw"), 1.0));
        QVERIFY(qAbs(qRed(p) - qGreen(p)) <= 1);
        QVERIFY(qAbs(qGreen(p) - qBlue(p)) <= 1);
    }

    void halfAmountBlendsHalfway()
    {
        const QImage src = solid(200, 60, 30);
        const QRgb full = firstPixel(applyLook(src, QStringLiteral("bw"), 1.0));
        const QRgb half = firstPixel(applyLook(src, QStringLiteral("bw"), 0.5));
        QVERIFY(qAbs(qRed(half) - (200 + qRed(full)) / 2) <= 1);
        QVERIFY(qAbs(qGreen(half) - (60 + qGreen(full)) / 2) <= 1);
        QVERIFY(qAbs(qBlue(half) - (30 + qBlue(full)) / 2) <= 1);
    }

    void gradientLookMapsBrightnessToItsColors()
    {
        // Sepia: black takes the dark stop, white the light one, and the result
        // stays warm (red above blue) in between.
        const QRgb dark = firstPixel(applyLook(solid(0, 0, 0), QStringLiteral("sepia"), 1.0));
        const QRgb light = firstPixel(applyLook(solid(255, 255, 255), QStringLiteral("sepia"), 1.0));
        const QRgb mid = firstPixel(applyLook(solid(128, 128, 128), QStringLiteral("sepia"), 1.0));
        QVERIFY(qRed(dark) < 60 && qRed(light) > 220);
        QVERIFY(qRed(mid) > qBlue(mid) + 20);
    }

    void splitToneTintsShadowsAndHighlightsDifferently()
    {
        // "cine" pushes shadows toward teal and highlights toward orange.
        const QRgb shadow = firstPixel(applyLook(solid(40, 40, 40), QStringLiteral("cine"), 1.0));
        const QRgb highlight = firstPixel(applyLook(solid(215, 215, 215), QStringLiteral("cine"), 1.0));
        QVERIFY(qBlue(shadow) > qRed(shadow));
        QVERIFY(qRed(highlight) > qBlue(highlight));
    }

    void vignetteDarkensCornersAndLightensWhenNegative()
    {
        const QImage src = solid(128, 128, 128, 64, 64);
        AdjustOp dark;
        dark.vignette = 1.0;
        const QImage d = applyAdjustOp(src, dark);
        QVERIFY(qRed(d.pixel(0, 0)) < 100);
        QVERIFY(qAbs(qRed(d.pixel(32, 32)) - 128) <= 2);

        AdjustOp light;
        light.vignette = -1.0;
        const QImage l = applyAdjustOp(src, light);
        QVERIFY(qRed(l.pixel(0, 0)) > 150);
        QVERIFY(qAbs(qRed(l.pixel(32, 32)) - 128) <= 2);
    }

    void grainIsRepeatableAndCentered()
    {
        const QImage src = solid(128, 128, 128, 64, 64);
        AdjustOp op;
        op.grain = 0.8;
        const QImage a = applyAdjustOp(src, op);
        const QImage b = applyAdjustOp(src, op);
        QCOMPARE(a, b); // the same photo always gets the same grain

        double sum = 0.0;
        int distinct = 0;
        QRgb first = a.pixel(0, 0);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                sum += qRed(a.pixel(x, y));
                if (a.pixel(x, y) != first)
                    ++distinct;
            }
        QVERIFY(distinct > 500);                          // it really is noise
        QVERIFY(qAbs(sum / (64 * 64) - 128.0) < 4.0);     // and does not shift the brightness
    }

    void noiseTypesFollowTheirDistributions()
    {
        // amount 0.5 on mid-gray: the deviation is 0.5 * 0.2 of full scale, ~25 levels.
        const QImage src = solid(128, 128, 128, 200, 200);
        AdjustOp op;
        op.grain = 0.5;

        op.grainType = int(NoiseType::Uniform);
        const NoiseStats uni = noiseStats(applyAdjustOp(src, op));
        op.grainType = int(NoiseType::Gaussian);
        const NoiseStats gau = noiseStats(applyAdjustOp(src, op));
        op.grainType = int(NoiseType::Laplacian);
        const NoiseStats lap = noiseStats(applyAdjustOp(src, op));

        for (const NoiseStats &st : {uni, gau, lap}) {
            QVERIFY(qAbs(st.mean - 128.0) < 1.5);          // zero-mean: brightness unchanged
            QVERIFY(st.sigma > 22.0 && st.sigma < 29.0);   // same strength for every type
        }
        QVERIFY(uni.kurtosis < 2.3);                       // flat (ideal 1.8)
        QVERIFY(gau.kurtosis > 2.6 && gau.kurtosis < 3.5); // bell curve (ideal 3)
        QVERIFY(lap.kurtosis > 4.5);                       // heavy tails (ideal 6)
    }

    void monochromeNoiseIsTheSameOnEveryChannel()
    {
        const QImage src = solid(128, 128, 128, 64, 64);
        AdjustOp op;
        op.grain = 0.6;
        for (int type : {0, 1, 2, 3}) {
            op.grainType = type;
            op.grainMono = true;
            const QImage mono = applyAdjustOp(src, op);
            op.grainMono = false;
            const QImage color = applyAdjustOp(src, op);
            int differing = 0;
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const QRgb m = mono.pixel(x, y);
                    QVERIFY(qRed(m) == qGreen(m) && qGreen(m) == qBlue(m));
                    const QRgb c = color.pixel(x, y);
                    if (qRed(c) != qGreen(c) || qGreen(c) != qBlue(c))
                        ++differing;
                }
            QVERIFY2(differing > 64 * 64 / 10, qPrintable(QString("type %1").arg(type)));
        }
    }

    void impulseNoiseOnlyHitsSomePixelsWithBlackAndWhite()
    {
        const QImage src = solid(128, 128, 128, 200, 200);
        AdjustOp op;
        op.grain = 0.5;
        op.grainType = int(NoiseType::Impulse);
        op.grainMono = true;
        const QImage out = applyAdjustOp(src, op);
        int black = 0, white = 0, untouched = 0;
        for (int y = 0; y < 200; ++y)
            for (int x = 0; x < 200; ++x) {
                const QRgb p = out.pixel(x, y);
                if (p == qRgb(0, 0, 0)) ++black;
                else if (p == qRgb(255, 255, 255)) ++white;
                else if (p == qRgb(128, 128, 128)) ++untouched;
            }
        QCOMPARE(black + white + untouched, 200 * 200); // nothing in between
        const double hit = double(black + white) / (200 * 200);
        QVERIFY(hit > 0.045 && hit < 0.075);            // 0.5 * 12% = 6%
        QVERIFY(black > 100 && white > 100);            // both kinds appear
    }

    void noiseNeedsAnAmount()
    {
        const QImage src = solid(90, 140, 200, 32, 32);
        AdjustOp op;
        for (int type : {0, 1, 2, 3}) {
            op.grainType = type;
            QCOMPARE(applyAdjustOp(src, op), src); // amount 0: untouched
        }
    }

    void resizeReturnsRequestedSizeAndKeepsFlatColor()
    {
        const QImage src = solid(90, 140, 200, 40, 30);
        const QImage up = resizeHighQuality(src, QSize(100, 100), true, true);
        QCOMPARE(up.size(), QSize(100, 75)); // fitted, aspect kept
        const QImage stretched = resizeHighQuality(src, QSize(100, 100), false, true);
        QCOMPARE(stretched.size(), QSize(100, 100));
        const QImage down = resizeHighQuality(src, QSize(10, 8), false, false);
        for (const QImage &img : {up, stretched, down})
            for (int y = 0; y < img.height(); ++y)
                for (int x = 0; x < img.width(); ++x) {
                    const QRgb p = img.pixel(x, y);
                    QVERIFY(qAbs(qRed(p) - 90) <= 1 && qAbs(qGreen(p) - 140) <= 1 && qAbs(qBlue(p) - 200) <= 1);
                }
    }

    void resizeKeepsTransparency()
    {
        QImage src(20, 20, QImage::Format_RGBA8888);
        src.fill(QColor(0, 0, 0, 0));
        for (int y = 5; y < 15; ++y)
            for (int x = 5; x < 15; ++x)
                src.setPixelColor(x, y, QColor(255, 0, 0, 255));
        const QImage up = resizeHighQuality(src, QSize(60, 60), false, true);
        QCOMPARE(qAlpha(up.pixel(30, 30)), 255);
        QCOMPARE(qAlpha(up.pixel(2, 2)), 0);
        // no dark fringe from the transparent pixels' color leaking in
        for (int x = 0; x < up.width(); ++x) {
            const QRgb p = up.pixel(x, 30);
            if (qAlpha(p) > 200)
                QVERIFY(qRed(p) > 240);
        }
    }

    void enlargingBeatsPlainStretchingAndDoesNotRing()
    {
        // A busy test picture: enlarge a half-size copy back and compare with
        // the original. Lanczos + edge enhancement must land closer to it than
        // Qt's smooth scaling, and the enhancement must not overshoot.
        QImage ref(240, 160, QImage::Format_RGBA8888);
        ref.fill(QColor(235, 228, 210));
        {
            QPainter p(&ref);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(QColor(180, 40, 40), 2));
            p.drawEllipse(QPoint(60, 80), 45, 45);
            p.setPen(QPen(QColor(20, 120, 60), 1));
            for (int i = 0; i < 20; ++i)
                p.drawLine(130 + i * 4, 20, 130 + i * 4 + 12, 140);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(60, 90, 200));
            p.drawRoundedRect(150, 60, 60, 60, 10, 10);
        }
        const QImage low = resizeHighQuality(ref, QSize(120, 80), false, false);
        const QImage qt = low.scaled(ref.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_RGBA8888);
        const QImage plain = resizeHighQuality(low, ref.size(), false, false);
        const QImage better = resizeHighQuality(low, ref.size(), false, true);

        auto psnr = [&](const QImage &a) {
            double se = 0.0;
            for (int y = 0; y < ref.height(); ++y)
                for (int x = 0; x < ref.width(); ++x)
                    for (int c = 0; c < 3; ++c) {
                        const double d = ref.constScanLine(y)[x * 4 + c] - a.constScanLine(y)[x * 4 + c];
                        se += d * d;
                    }
            return 10.0 * std::log10(255.0 * 255.0 / (se / (ref.width() * ref.height() * 3)));
        };
        QVERIFY(psnr(plain) > psnr(qt));
        QVERIFY(psnr(better) > psnr(qt) + 0.3);

        // No new halos: the enhancement never pushes a pixel meaningfully
        // beyond the extremes the plain Lanczos result already had.
        int lo = 255, hi = 0;
        for (int y = 0; y < plain.height(); ++y)
            for (int x = 0; x < plain.width(); ++x)
                for (int c = 0; c < 3; ++c) {
                    lo = std::min<int>(lo, plain.constScanLine(y)[x * 4 + c]);
                    hi = std::max<int>(hi, plain.constScanLine(y)[x * 4 + c]);
                }
        for (int y = 0; y < better.height(); ++y)
            for (int x = 0; x < better.width(); ++x)
                for (int c = 0; c < 3; ++c) {
                    QVERIFY(better.constScanLine(y)[x * 4 + c] >= lo);
                    QVERIFY(better.constScanLine(y)[x * 4 + c] <= hi);
                }
    }

    void effectCatalogueIsWellFormed()
    {
        QVERIFY(!allEffects().empty());
        std::set<QString> ids;
        std::set<QString> groups;
        for (const EffectGroup &g : effectGroups())
            groups.insert(g.id);
        for (const EffectSpec &fx : allEffects()) {
            QVERIFY2(ids.insert(fx.id).second, qPrintable("duplicate id " + fx.id));
            QVERIFY2(!fx.name.isEmpty(), qPrintable(fx.id));
            QVERIFY2(groups.count(fx.group) == 1, qPrintable(fx.id + " has an unknown group"));
            QVERIFY2(fx.params.size() <= kMaxEffectParams, qPrintable(fx.id));
            for (const EffectParam &p : fx.params) {
                QVERIFY2(p.min < p.max, qPrintable(fx.id + "/" + p.label));
                QVERIFY2(p.def >= p.min && p.def <= p.max, qPrintable(fx.id + "/" + p.label));
                if (p.toggle)
                    QVERIFY(p.min == 0.0 && p.max == 1.0);
                if (!p.options.isEmpty())
                    QVERIFY2(p.min == 0.0 && p.max == p.options.size() - 1 && p.integer, qPrintable(fx.id));
            }
            QCOMPARE(findEffect(fx.id), &fx);
            const auto sample = sampleEffectValues(fx);
            for (size_t i = 0; i < fx.params.size(); ++i)
                QVERIFY2(sample[i] >= fx.params[i].min && sample[i] <= fx.params[i].max, qPrintable(fx.id));
        }
        QVERIFY(findEffect(QStringLiteral("nope")) == nullptr);
        QVERIFY(ids.count(QStringLiteral("blur")) == 1);
    }

    void everyEffectRunsKeepsTheSizeAndChangesTheImage()
    {
        const QImage src = busyPicture(96, 64);
        for (const EffectSpec &fx : allEffects()) {
            const QString text = fx.usesText ? QStringLiteral("Texto") : QString(); // the effects that carry a text need one
            const QImage out = applyEffect(src, fx.id, defaultEffectValues(fx), 1.0, nullptr, text);
            // the same size - unless the effect says it changes it, and then exactly the size it announces
            const QSize expected = effectOutputSize(fx.id, defaultEffectValues(fx), src.size(), text);
            QVERIFY2(fx.changesSize || expected == src.size(), qPrintable(fx.id));
            QVERIFY2(out.size() == expected, qPrintable(fx.id));
            QVERIFY2(out.format() == QImage::Format_RGBA8888, qPrintable(fx.id));
            QVERIFY2(out != src, qPrintable(fx.id + " changed nothing"));
        }
    }

    // A clean test picture (smooth colour gradient with a hard-edged bright square) and the same with
    // Gaussian noise of the given deviation added to each channel.
    static QImage cleanPicture(int w, int h)
    {
        QImage img(w, h, QImage::Format_RGBA8888);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const bool square = x > w / 3 && x < 2 * w / 3 && y > h / 3 && y < 2 * h / 3;
                img.setPixelColor(x, y, square ? QColor(230, 210, 60) : QColor(40 + x * 100 / w, 80 + y * 90 / h, 140));
            }
        return img;
    }
    static QImage addNoise(const QImage &clean, double sigma)
    {
        QImage img = clean;
        quint32 seed = 987654321u;
        auto uniform = [&seed] {
            seed = seed * 1664525u + 1013904223u;
            return (seed >> 8) / 16777216.0;
        };
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                QColor c = img.pixelColor(x, y);
                auto gauss = [&] { return (uniform() + uniform() + uniform() + uniform() - 2.0) * sigma * std::sqrt(3.0); };
                c.setRgb(qBound(0, int(c.red() + gauss() + 0.5), 255), qBound(0, int(c.green() + gauss() + 0.5), 255),
                         qBound(0, int(c.blue() + gauss() + 0.5), 255), c.alpha());
                img.setPixelColor(x, y, c);
            }
        return img;
    }
    static double meanError(const QImage &a, const QImage &b)
    {
        double sum = 0;
        for (int y = 0; y < a.height(); ++y)
            for (int x = 0; x < a.width(); ++x) {
                const QColor p = a.pixelColor(x, y), q = b.pixelColor(x, y);
                sum += qAbs(p.red() - q.red()) + qAbs(p.green() - q.green()) + qAbs(p.blue() - q.blue());
            }
        return sum / (3.0 * a.width() * a.height());
    }

    // "Quitar ruido" (non-local means): on a noisy picture it gets much closer to the clean one than
    // both the noisy picture and the old median do, and it does not soften the hard edge.
    void denoiseBeatsTheMedianAndKeepsEdges()
    {
        const QImage clean = cleanPicture(96, 96);
        const QImage noisy = addNoise(clean, 14.0);
        const QImage out = applyEffect(noisy, QStringLiteral("denoise"), {50, 60});
        const QImage median = applyEffect(noisy, QStringLiteral("median"), {1});
        const double noisyErr = meanError(noisy, clean), denoisedErr = meanError(out, clean), medianErr = meanError(median, clean);
        qInfo() << "mean error: noisy" << noisyErr << "median" << medianErr << "denoise" << denoisedErr;
        QVERIFY(denoisedErr < noisyErr * 0.5);
        QVERIFY(denoisedErr < medianErr * 0.85);
        // across the square's left edge (x = 32|33) the step is still there
        const int step = out.pixelColor(36, 48).blue() - out.pixelColor(28, 48).blue(); // clean: 60 - 140 = -80
        QVERIFY2(step < -55, qPrintable(QStringLiteral("edge step %1").arg(step)));
    }

    // Nothing to remove: both sliders at 0 gives the picture back; alpha is never touched.
    void denoiseWithZeroStrengthIsANoOpAndKeepsAlpha()
    {
        QImage src = addNoise(cleanPicture(40, 30), 10.0);
        QCOMPARE(applyEffect(src, QStringLiteral("denoise"), {0, 0}), src);
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x) {
                QColor c = src.pixelColor(x, y);
                c.setAlpha(100 + (x * 3 + y) % 100);
                src.setPixelColor(x, y, c);
            }
        const QImage out = applyEffect(src, QStringLiteral("denoise"), {70, 70});
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x)
                QCOMPARE(out.pixelColor(x, y).alpha(), src.pixelColor(x, y).alpha());
    }

    // "Quitar ruido" radius 1 uses a fixed sorting network; it must give the median of the 3x3
    // neighbourhood (edges clamped). Opaque pixels: every colour channel; translucent ones: the
    // alpha channel (colour is worked on premultiplied, so only alpha compares exactly).
    void medianRadiusOneIsTheTrueMedian()
    {
        for (const bool opaque : {true, false}) {
            QImage src(23, 17, QImage::Format_RGBA8888);
            quint32 seed = 12345;
            for (int y = 0; y < src.height(); ++y)
                for (int x = 0; x < src.width(); ++x) {
                    seed = seed * 1664525u + 1013904223u;
                    src.setPixelColor(x, y, QColor((seed >> 8) & 255, (seed >> 16) & 255, (seed >> 24) & 255,
                                                   opaque ? 255 : 255 - int((seed >> 4) & 63)));
                }
            const QImage out = applyEffect(src, QStringLiteral("median"), {1});
            for (int y = 0; y < src.height(); ++y)
                for (int x = 0; x < src.width(); ++x)
                    for (int c = opaque ? 0 : 3; c < (opaque ? 3 : 4); ++c) {
                        std::vector<int> v;
                        for (int j = -1; j <= 1; ++j)
                            for (int i = -1; i <= 1; ++i) {
                                const QColor p = src.pixelColor(qBound(0, x + i, src.width() - 1), qBound(0, y + j, src.height() - 1));
                                v.push_back(c == 0 ? p.red() : c == 1 ? p.green() : c == 2 ? p.blue() : p.alpha());
                            }
                        std::sort(v.begin(), v.end());
                        const QColor o = out.pixelColor(x, y);
                        const int got = c == 0 ? o.red() : c == 1 ? o.green() : c == 2 ? o.blue() : o.alpha();
                        QVERIFY2(got == v[4], qPrintable(QStringLiteral("channel %1 at %2,%3").arg(c).arg(x).arg(y)));
                    }
        }
    }

    void negativeInvertsTheColoursAndKeepsTheAlpha()
    {
        QImage src(4, 4, QImage::Format_RGBA8888);
        src.fill(QColor(10, 100, 250, 77));
        const QImage out = applyEffect(src, QStringLiteral("negative"), {});
        const QColor c = out.pixelColor(1, 1);
        QCOMPARE(c.red(), 245);
        QCOMPARE(c.green(), 155);
        QCOMPARE(c.blue(), 5);
        QCOMPARE(c.alpha(), 77);
        QCOMPARE(applyEffect(out, QStringLiteral("negative"), {}), src); // twice = the original
    }

    // The vignette leaves the middle alone and works on the edges; a negative amount
    // lightens them instead; amount 0 changes nothing; the alpha is never touched.
    void vignetteWorksOnTheEdgesOnly()
    {
        const QImage grey = solid(128, 128, 128, 101, 81);
        EffectValues v = defaultEffectValues(*findEffect(QStringLiteral("vignette")));
        const QImage dark = applyEffect(grey, QStringLiteral("vignette"), v);
        QCOMPARE(dark.pixelColor(50, 40).red(), 128);        // the middle
        QVERIFY(dark.pixelColor(0, 0).red() < 90);           // the corner
        QVERIFY(dark.pixelColor(0, 0).red() <= dark.pixelColor(20, 10).red());

        v[0] = -60;
        const QImage light = applyEffect(grey, QStringLiteral("vignette"), v);
        QCOMPARE(light.pixelColor(50, 40).red(), 128);
        QVERIFY(light.pixelColor(0, 0).red() > 180);

        v[0] = 0;
        QCOMPARE(applyEffect(grey, QStringLiteral("vignette"), v), grey);

        QImage translucent(60, 40, QImage::Format_RGBA8888);
        translucent.fill(QColor(200, 100, 50, 90));
        v[0] = 80;
        QCOMPARE(applyEffect(translucent, QStringLiteral("vignette"), v).pixelColor(0, 0).alpha(), 90);
    }

    void vignetteSizeSoftnessAndCentreMoveTheDarkness()
    {
        const QImage grey = solid(128, 128, 128, 120, 80);
        const auto spec = *findEffect(QStringLiteral("vignette"));
        EffectValues v = defaultEffectValues(spec);

        // a bigger clear area leaves a corner lighter
        EffectValues small = v; small[1] = 20;
        EffectValues big = v; big[1] = 90;
        QVERIFY(applyEffect(grey, QStringLiteral("vignette"), small).pixelColor(5, 5).red()
                < applyEffect(grey, QStringLiteral("vignette"), big).pixelColor(5, 5).red());

        // moving the centre to the left makes the left side clear and the right side dark
        EffectValues left = v; left[4] = -100; left[1] = 30;
        const QImage moved = applyEffect(grey, QStringLiteral("vignette"), left);
        QVERIFY(moved.pixelColor(2, 40).red() > moved.pixelColor(117, 40).red());

        // a rectangle (roundness -100) darkens the middle of an edge more than a circle does
        EffectValues box = v; box[3] = -100; box[1] = 40;
        EffectValues circle = v; circle[3] = 100; circle[1] = 40;
        QVERIFY(applyEffect(grey, QStringLiteral("vignette"), box).pixelColor(60, 2).red()
                < applyEffect(grey, QStringLiteral("vignette"), circle).pixelColor(60, 2).red());
    }

    void grainIsRepeatableAndRespectsMonochromeAndAmount()
    {
        const QImage grey = solid(128, 128, 128, 64, 64);
        EffectValues v = defaultEffectValues(*findEffect(QStringLiteral("grain")));
        const QImage a = applyEffect(grey, QStringLiteral("grain"), v);
        QCOMPARE(applyEffect(grey, QStringLiteral("grain"), v), a); // deterministic
        QVERIFY(a != grey);

        // monochrome: red, green and blue move together
        v[3] = 1;
        const QImage mono = applyEffect(grey, QStringLiteral("grain"), v);
        for (int y = 0; y < mono.height(); y += 7)
            for (int x = 0; x < mono.width(); x += 5) {
                const QColor c = mono.pixelColor(x, y);
                QCOMPARE(c.red(), c.green());
                QCOMPARE(c.green(), c.blue());
            }

        v[0] = 0;
        QCOMPARE(applyEffect(grey, QStringLiteral("grain"), v), grey);

        // a coarser grain repeats the same value over a block of pixels
        v[0] = 60; v[1] = 100; v[3] = 1;
        const QImage coarse = applyEffect(solid(128, 128, 128, 400, 400), QStringLiteral("grain"), v);
        QCOMPARE(coarse.pixelColor(0, 0), coarse.pixelColor(1, 1));
    }

    // ---- modo pixel: nearest-neighbour resize and rotation ----

    // Enlarging by a whole number repeats every pixel exactly that many times: no new colours,
    // no blended edges.
    void nearestEnlargementRepeatsEveryPixel()
    {
        QImage src(3, 2, QImage::Format_RGBA8888);
        const QColor colors[] = {QColor(255, 0, 0), QColor(0, 255, 0), QColor(0, 0, 255),
                                 QColor(255, 255, 0), QColor(0, 255, 255), QColor(255, 0, 255, 90)};
        for (int i = 0; i < 6; ++i)
            src.setPixelColor(i % 3, i / 3, colors[i]);

        const QImage big = resizeNearest(src, QSize(12, 8), false); // x4
        QCOMPARE(big.size(), QSize(12, 8));
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 12; ++x)
                QCOMPARE(big.pixelColor(x, y), src.pixelColor(x / 4, y / 4));
    }

    void nearestShrinkPicksPixelsThatExistAndKeepsTheAspect()
    {
        const QImage src = busyPicture(40, 20);
        const QImage small = resizeNearest(src, QSize(10, 10), true); // 10 x 5 keeping the shape
        QCOMPARE(small.size(), QSize(10, 5));
        QSet<QRgb> seen;
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x)
                seen.insert(src.pixel(x, y));
        for (int y = 0; y < small.height(); ++y)
            for (int x = 0; x < small.width(); ++x)
                QVERIFY2(seen.contains(small.pixel(x, y)), "a colour that is not in the picture appeared");
    }

    void aNearestResizeOpDoesNotBlendAndLanczosStillDoes()
    {
        QImage src(2, 1, QImage::Format_RGBA8888);
        src.setPixelColor(0, 0, QColor(0, 0, 0));
        src.setPixelColor(1, 0, QColor(255, 255, 255));

        ResizeOp nearest{QSize(8, 4), false, QStringLiteral("nearest"), false};
        const QImage hard = applyOperation(src, nearest);
        for (int x = 0; x < 8; ++x)
            QVERIFY(hard.pixelColor(x, 0).red() == 0 || hard.pixelColor(x, 0).red() == 255);

        ResizeOp smooth{QSize(8, 4), false, QStringLiteral("lanczos3"), false};
        const QImage soft = applyOperation(src, smooth);
        bool blended = false;
        for (int x = 0; x < 8; ++x)
            blended = blended || (soft.pixelColor(x, 0).red() > 0 && soft.pixelColor(x, 0).red() < 255);
        QVERIFY(blended);
    }

    // A tilted picture rotated without smoothing only holds colours it already had (or see-through).
    void nearestRotationAddsNoColours()
    {
        const QImage src = busyPicture(30, 30);
        QSet<QRgb> seen;
        for (int y = 0; y < 30; ++y)
            for (int x = 0; x < 30; ++x)
                seen.insert(src.pixel(x, y));
        const QImage turned = applyOperation(src, RotateOp{23.0, false});
        for (int y = 0; y < turned.height(); ++y)
            for (int x = 0; x < turned.width(); ++x)
                QVERIFY2(turned.pixelColor(x, y).alpha() == 0 || seen.contains(turned.pixel(x, y)),
                         "rotation without smoothing invented a colour");
    }

    void unknownEffectOrZeroMixLeavesThePicture()
    {
        const QImage src = busyPicture(40, 30);
        QCOMPARE(applyEffect(src, QStringLiteral("nope"), {50, 50, 50}), src);
        QCOMPARE(applyEffect(src, QStringLiteral("blur"), {80, 0, 0}, 0.0), src);
    }

    void effectsAtZeroStrengthKeepThePicture()
    {
        const QImage src = busyPicture(60, 40);
        const struct { const char *id; EffectValues v; } neutral[] = {
            {"blur", {0, 0, 0}},   {"motion", {0, 0, 0}}, {"zoom", {0, 0, 0}},  {"spin", {0, 0, 0}},
            {"fisheye", {0, 0, 0}}, {"swirl", {0, 70, 0}}, {"wave", {0, 30, 0}}, {"frosted", {0, 0, 0}},
        };
        for (const auto &n : neutral)
            QVERIFY2(applyEffect(src, QString::fromLatin1(n.id), n.v) == src, n.id);
    }

    void blurKeepsFlatColorAndSoftensAnEdge()
    {
        const QImage flat = solid(90, 140, 200, 80, 60);
        const QImage flatOut = applyEffect(flat, QStringLiteral("blur"), {100, 0, 0});
        for (int y = 0; y < flatOut.height(); ++y)
            for (int x = 0; x < flatOut.width(); ++x) {
                const QRgb p = flatOut.pixel(x, y);
                QVERIFY(qAbs(qRed(p) - 90) <= 1 && qAbs(qGreen(p) - 140) <= 1 && qAbs(qBlue(p) - 200) <= 1);
            }

        QImage edge(100, 40, QImage::Format_RGBA8888);
        for (int y = 0; y < edge.height(); ++y)
            for (int x = 0; x < edge.width(); ++x)
                edge.setPixelColor(x, y, x < 50 ? QColor(0, 0, 0) : QColor(255, 255, 255));
        const QImage out = applyEffect(edge, QStringLiteral("blur"), {60, 0, 0});
        const int left = qRed(out.pixel(49, 20));
        const int right = qRed(out.pixel(50, 20));
        QVERIFY(left > 20 && left < 140);      // the step became a ramp: both sides of
        QVERIFY(right > 115 && right < 235);   // the edge now hold a mix of black and white
        QVERIFY(left < right);
        QVERIFY(qRed(out.pixel(2, 20)) < 20);  // far from the edge: untouched
        QVERIFY(qRed(out.pixel(97, 20)) > 235);
        // monotonic across the edge - a blur must not ring
        for (int x = 1; x < out.width(); ++x)
            QVERIFY(qRed(out.pixel(x, 20)) >= qRed(out.pixel(x - 1, 20)) - 1);
    }

    void blurDoesNotBleedColorOutOfTransparentPixels()
    {
        QImage src(60, 60, QImage::Format_RGBA8888);
        src.fill(QColor(0, 0, 0, 0)); // transparent, hidden color black
        for (int y = 20; y < 40; ++y)
            for (int x = 20; x < 40; ++x)
                src.setPixelColor(x, y, QColor(255, 0, 0, 255));
        for (const char *id : {"blur", "motion", "zoom", "frosted"}) {
            const QImage out = applyEffect(src, QString::fromLatin1(id), {80, 0, 0});
            for (int y = 0; y < out.height(); ++y)
                for (int x = 0; x < out.width(); ++x) {
                    const QRgb p = out.pixel(x, y);
                    if (qAlpha(p) > 200)
                        QVERIFY2(qRed(p) > 235, id);
                }
        }
    }

    void posterizeAndThresholdUseOnlyTheirLevels()
    {
        QImage ramp(256, 4, QImage::Format_RGBA8888);
        for (int y = 0; y < ramp.height(); ++y)
            for (int x = 0; x < 256; ++x)
                ramp.setPixelColor(x, y, QColor(x, x, x));
        for (int levels : {2, 4, 7, 16}) {
            const QImage out = applyEffect(ramp, QStringLiteral("posterize"), {double(levels), 0, 0});
            std::set<int> seen;
            for (int x = 0; x < 256; ++x)
                seen.insert(qRed(out.pixel(x, 1)));
            QCOMPARE(int(seen.size()), levels);
            QCOMPARE(*seen.begin(), 0);
            QCOMPARE(*seen.rbegin(), 255);
        }
        const QImage bw = applyEffect(ramp, QStringLiteral("threshold"), {100, 0, 0});
        for (int x = 0; x < 256; ++x) {
            const int v = qRed(bw.pixel(x, 0));
            QVERIFY(v == 0 || v == 255);
            if (qAbs(x - 100) > 1) // right at the limit, floating-point luma may land either side
                QCOMPARE(v == 255, x > 100);
        }
    }

    void edgesAndEmbossOnAFlatPicture()
    {
        const QImage flat = solid(120, 60, 200, 40, 40);
        const QImage dark = applyEffect(flat, QStringLiteral("edges"), {50, 0, 0});
        const QImage light = applyEffect(flat, QStringLiteral("edges"), {50, 1, 0});
        const QImage relief = applyEffect(flat, QStringLiteral("emboss"), {50, 135, 0});
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 40; ++x) {
                QCOMPARE(qRed(dark.pixel(x, y)), 0);
                QCOMPARE(qRed(light.pixel(x, y)), 255);
                QVERIFY(qAbs(qRed(relief.pixel(x, y)) - 128) <= 1);
            }
        // ...and a real edge lights the dark version up
        QImage step(40, 20, QImage::Format_RGBA8888);
        for (int y = 0; y < 20; ++y)
            for (int x = 0; x < 40; ++x)
                step.setPixelColor(x, y, x < 20 ? QColor(10, 10, 10) : QColor(240, 240, 240));
        const QImage edge = applyEffect(step, QStringLiteral("edges"), {50, 0, 0});
        QVERIFY(qRed(edge.pixel(20, 10)) > 200);
        QCOMPARE(qRed(edge.pixel(3, 10)), 0);
    }

    void effectsAreDeterministic()
    {
        const QImage src = busyPicture(64, 48);
        for (const char *id : {"crystallize", "frosted", "halftone"}) {
            const EffectSpec *fx = findEffect(QString::fromLatin1(id));
            QVERIFY(fx);
            const auto v = defaultEffectValues(*fx);
            QCOMPARE(applyEffect(src, fx->id, v), applyEffect(src, fx->id, v));
        }
    }

    void mixBlendsHalfwayToTheOriginal()
    {
        const QImage src = busyPicture(50, 40);
        const EffectValues v = {1, 0, 0};
        const QImage full = applyEffect(src, QStringLiteral("threshold"), v);
        const QImage half = applyEffect(src, QStringLiteral("threshold"), v, 0.5);
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x)
                for (int c = 0; c < 3; ++c) {
                    const int s = src.constScanLine(y)[x * 4 + c];
                    const int f = full.constScanLine(y)[x * 4 + c];
                    QVERIFY(qAbs(half.constScanLine(y)[x * 4 + c] - (s + f) / 2.0) <= 1.01);
                }
    }

    // The point of expressing distances as a percentage of the long side: the
    // same settings give the same look on a small preview and the full photo.
    void effectLooksTheSameAtAnyResolution()
    {
        const QImage big = smoothPicture(480, 320);
        const QImage small = big.scaled(120, 80, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_RGBA8888);
        for (const char *id : {"blur", "motion", "zoom", "spin", "fisheye", "swirl", "wave"}) {
            const EffectSpec *fx = findEffect(QString::fromLatin1(id));
            QVERIFY(fx);
            const auto v = defaultEffectValues(*fx);
            const QImage fromBig = applyEffect(big, fx->id, v)
                                       .scaled(120, 80, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                       .convertToFormat(QImage::Format_RGBA8888);
            const QImage fromSmall = applyEffect(small, fx->id, v);
            double sum = 0.0;
            for (int y = 0; y < 80; ++y)
                for (int x = 0; x < 120; ++x)
                    for (int c = 0; c < 3; ++c)
                        sum += qAbs(int(fromBig.constScanLine(y)[x * 4 + c]) - int(fromSmall.constScanLine(y)[x * 4 + c]));
            const double mean = sum / (120.0 * 80.0 * 3.0);
            qInfo().noquote() << "resolution parity" << id << "mean abs diff" << QString::number(mean, 'f', 2);
            QVERIFY2(mean < 6.0, qPrintable(QString("%1: %2").arg(id).arg(mean)));
        }
    }

    void aCancelledEffectStillReturnsAPictureOfTheRightSize()
    {
        const QImage src = busyPicture(300, 200);
        std::atomic<bool> cancel{true};
        for (const char *id : {"blur", "median", "crystallize", "emboss", "wave"}) {
            const EffectSpec *fx = findEffect(QString::fromLatin1(id));
            const QImage out = applyEffect(src, fx->id, defaultEffectValues(*fx), 1.0, &cancel);
            QCOMPARE(out.size(), src.size());
        }
    }

    void effectsLiveInTheUndoHistory()
    {
        const QImage src = busyPicture(48, 32);
        EditStack stack;
        stack.push(EffectOp{QStringLiteral("posterize"), {3, 0, 0}, 1.0});
        QVERIFY(stack.bake(src) != src);
        QVERIFY(stack.undo());
        QCOMPARE(stack.bake(src), src);
        QVERIFY(stack.redo());
        QCOMPARE(stack.bake(src), applyEffect(src, QStringLiteral("posterize"), {3, 0, 0}));
    }

    // The Ajustes sliders and the Filtros look share the undo history with the
    // structural edits, as LiveOp snapshots that change no pixels.
    void liveOpsShareTheHistoryButChangeNoPixels()
    {
        const QImage src = busyPicture(40, 30);
        LiveOp a;
        a.adjust.exposure = 0.4;
        LiveOp b;
        b.adjust.exposure = 0.8;
        b.lookId = QStringLiteral("sepia");
        b.lookAmount = 0.5;

        EditStack stack;
        QVERIFY(stack.activeLiveState() == nullptr);
        QVERIFY(!stack.hasActivePixelOps());
        stack.push(a);
        QVERIFY(!stack.hasActivePixelOps()); // a slider alone is not a pixel edit
        QCOMPARE(stack.bake(src), src);
        stack.push(FlipOp{true, false});
        stack.push(b);
        QVERIFY(stack.hasActivePixelOps());
        QCOMPARE(stack.bake(src), src.mirrored(true, false));

        QCOMPARE(stack.activeLiveState()->adjust.exposure, 0.8);
        QCOMPARE(stack.activeLiveState()->lookId, QStringLiteral("sepia"));
        QVERIFY(stack.undoTargetIsLive());
        QVERIFY(stack.undo());                               // undo the look/exposure gesture
        QCOMPARE(stack.activeLiveState()->adjust.exposure, 0.4);
        QVERIFY(stack.redoTargetIsLive());
        QVERIFY(!stack.undoTargetIsLive());                  // next one back is the flip
        QVERIFY(stack.undo());                               // undo the flip
        QCOMPARE(stack.bake(src), src);
        QCOMPARE(stack.activeLiveState()->adjust.exposure, 0.4); // the overlay is untouched by it
        QVERIFY(stack.undo());
        QVERIFY(stack.activeLiveState() == nullptr);         // back to neutral
        QVERIFY(stack.redo());
        QCOMPARE(stack.activeLiveState()->adjust.exposure, 0.4);
    }

    void mergingTheTicksOfOneGestureIntoOneHistoryEntry()
    {
        LiveOp tick;
        EditStack stack;
        stack.push(RotateOp{90});
        QVERIFY(!stack.replaceLastLive(tick)); // the newest entry is not a LiveOp

        tick.adjust.exposure = 0.1;
        stack.push(tick);
        for (double e : {0.2, 0.3, 0.55}) {
            tick.adjust.exposure = e;
            QVERIFY(stack.replaceLastLive(tick));
        }
        QCOMPARE(stack.operations().size(), size_t(2)); // rotate + ONE live entry
        QCOMPARE(stack.activeLiveState()->adjust.exposure, 0.55);
        QVERIFY(stack.undo());                              // a single undo removes the whole drag
        QVERIFY(stack.activeLiveState() == nullptr);

        // once something has been undone past the newest entry, nothing is merged into it
        QVERIFY(stack.redo());
        QVERIFY(stack.undo());
        QVERIFY(!stack.replaceLastLive(tick));
    }

    // The remembered results must never change what the stack produces.
    void checkpointsNeverChangeTheResult()
    {
        const QImage src = busyPicture(48, 36);
        const EffectOp posterize{QStringLiteral("posterize"), {3, 0, 0}, 1.0};
        const EffectOp threshold{QStringLiteral("threshold"), {90, 0, 0}, 1.0};

        auto fresh = [&](std::initializer_list<Operation> ops) {
            EditStack s;
            for (const Operation &op : ops)
                s.push(op);
            return s.bake(src);
        };

        EditStack stack;
        stack.push(RotateOp{90});
        stack.push(posterize);
        stack.push(FlipOp{true, false});
        QCOMPARE(stack.bake(src), fresh({RotateOp{90}, posterize, FlipOp{true, false}}));
        QVERIFY(stack.undo());
        QCOMPARE(stack.bake(src), fresh({RotateOp{90}, posterize}));
        QVERIFY(stack.undo());
        QCOMPARE(stack.bake(src), fresh({RotateOp{90}}));
        QVERIFY(stack.redo());
        QVERIFY(stack.redo());
        QCOMPARE(stack.bake(src), fresh({RotateOp{90}, posterize, FlipOp{true, false}}));

        // A new operation after stepping back: results remembered for the
        // discarded ones must not leak into it.
        QVERIFY(stack.undo());
        QVERIFY(stack.undo());
        stack.push(threshold);
        QCOMPARE(stack.bake(src), fresh({RotateOp{90}, threshold}));
        QVERIFY(stack.bake(src) != fresh({RotateOp{90}, posterize}));

        // Handing the stack a result it did not calculate itself.
        EditStack handed;
        handed.push(posterize);
        handed.storeCheckpoint(src, applyEffect(src, QStringLiteral("posterize"), {3, 0, 0}));
        QCOMPARE(handed.bake(src), fresh({posterize}));

        // A different source picture invalidates everything remembered.
        const QImage other = busyPicture(48, 36).mirrored(true, false);
        QCOMPARE(handed.bake(other), applyEffect(other, QStringLiteral("posterize"), {3, 0, 0}));
    }

    // However little (or much) memory the history may use for remembered results,
    // what it produces must be identical.
    void resultsDoNotDependOnTheCheckpointBudget()
    {
        const QImage src = busyPicture(48, 36);
        const EffectOp posterize{QStringLiteral("posterize"), {3, 0, 0}, 1.0};
        const EffectOp threshold{QStringLiteral("threshold"), {90, 0, 0}, 1.0};
        EditStack reference;
        for (const Operation &op : {Operation(RotateOp{90}), Operation(posterize), Operation(FlipOp{true, false}), Operation(threshold)})
            reference.push(op);
        const QImage expected = reference.bake(src);

        for (qint64 budget : {qint64(0), qint64(1000), qint64(100000), qint64(600) * 1024 * 1024}) {
            EditStack s;
            s.setCheckpointBudget(budget);
            for (const Operation &op : {Operation(RotateOp{90}), Operation(posterize), Operation(FlipOp{true, false}), Operation(threshold)}) {
                s.push(op);
                s.bake(src); // fills (and evicts from) the remembered results
            }
            QCOMPARE(s.bake(src), expected);
            for (int i = 0; i < 4; ++i)
                QVERIFY(s.undo());
            QCOMPARE(s.bake(src), src);
            for (int i = 0; i < 4; ++i)
                QVERIFY(s.redo());
            QCOMPARE(s.bake(src), expected);
        }
    }

    void everyLookRunsAndKeepsTheImageSize()
    {
        QImage src(90, 60, QImage::Format_RGBA8888);
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x)
                src.setPixelColor(x, y, QColor(x * 255 / 89, y * 255 / 59, (x + y) * 255 / 149));
        for (const LookSpec &look : allLooks()) {
            const QImage out = applyLook(src, look.id, 1.0);
            QVERIFY2(out.size() == src.size(), qPrintable(look.id));
            QVERIFY2(out != src, qPrintable(look.id + " changed nothing"));
        }
    }

    void clarityAndSharpenKeepImageSize()
    {
        AdjustOp op;
        op.clarity = 0.6;
        op.sharpness = 0.4;
        const QImage src = solid(100, 110, 120, 50, 40);
        const QImage out = applyAdjustOp(src, op);
        QCOMPARE(out.size(), src.size());
        // A flat image has no detail to boost - clarity/sharpen leave it be.
        QVERIFY(qAbs(qRed(out.pixel(25, 20)) - 100) <= 1);
    }

    // The safe-save worker replays the history with bakeOperations() (no checkpoints, no
    // shared state, any thread): it must produce exactly what the stack itself bakes.
    void bakeOperationsGivesWhatTheStackBakes()
    {
        QImage src(64, 48, QImage::Format_RGBA8888);
        for (int y = 0; y < src.height(); ++y)
            for (int x = 0; x < src.width(); ++x)
                src.setPixelColor(x, y, QColor((x * 5) % 256, (y * 7) % 256, (x * y) % 256));

        EditStack stack;
        stack.push(CropOp{QRectF(0.1, 0.1, 0.8, 0.8)});
        stack.push(LiveOp{}); // changes no pixels
        stack.push(RotateOp{90});
        stack.push(FlipOp{true, false});
        stack.push(EffectOp{QStringLiteral("posterize"), {4.0, 0.0, 0.0}, 1.0});
        const QImage viaStack = stack.bake(src);

        QCOMPARE(bakeOperations(src, stack.activePixelOps()), viaStack);
        QCOMPARE(stack.activePixelOps().size(), size_t(4)); // the LiveOp is not one of them

        stack.undo();
        QCOMPARE(bakeOperations(src, stack.activePixelOps()), stack.bake(src)); // after an undo too
    }

    // "What the file holds" = the look, then the sliders: one definition for save, copy
    // and wallpaper.
    void theLiveOverlayIsTheLookThenTheSliders()
    {
        const QImage src = solid(90, 120, 150, 24, 16);
        AdjustOp adjust;
        adjust.exposure = 0.3;
        adjust.saturation = 0.4;

        QCOMPARE(renderLiveOverlay(src, QString(), 1.0, AdjustOp()), src); // nothing to do: the same picture
        QCOMPARE(renderLiveOverlay(src, QString(), 1.0, adjust), applyAdjustOp(src, adjust));
        QCOMPARE(renderLiveOverlay(src, QStringLiteral("sepia"), 0.7, adjust),
                 applyAdjustOp(applyLook(src, QStringLiteral("sepia"), 0.7), adjust));
    }
};

QTEST_MAIN(TestEditStack)
#include "test_edit_stack.moc"
