#include <QtTest>
#include "edit/Blend.h"
#include "edit/Effects.h"

#include <cmath>

using namespace core::edit;

namespace {

QImage solid(int r, int g, int b, int w = 8, int h = 8, int a = 255)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(r, g, b, a));
    return img;
}

double packRgb(int r, int g, int b) { return double((r << 16) | (g << 8) | b); }

QRgb at(const QImage &img, int x, int y) { return img.pixel(x, y); }

// A black picture with white squares of 40x40 whose top-left corners are given.
QImage squares(int w, int h, std::initializer_list<QPoint> corners)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(Qt::black);
    for (const QPoint &c : corners)
        for (int y = c.y(); y < c.y() + 40; ++y)
            for (int x = c.x(); x < c.x() + 40; ++x)
                img.setPixelColor(x, y, Qt::white);
    return img;
}

// The bounding box of the pixels whose channel `ch` (0 r, 1 g, 2 b) is above 128 inside `area`.
QRect channelBox(const QImage &img, int ch, QRect area)
{
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            if (img.constScanLine(y)[x * 4 + ch] > 128) {
                x0 = std::min(x0, x); y0 = std::min(y0, y);
                x1 = std::max(x1, x); y1 = std::max(y1, y);
            }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
}

double meanChannel(const QImage &img, QRect area, int ch)
{
    double sum = 0;
    for (int y = area.top(); y <= area.bottom(); ++y)
        for (int x = area.left(); x <= area.right(); ++x)
            sum += img.constScanLine(y)[x * 4 + ch];
    return sum / (double(area.width()) * area.height());
}

} // namespace

class TestEffectFamilies : public QObject {
    Q_OBJECT

private slots:
    // --- blend modes -------------------------------------------------------------------------

    void blendModesFollowTheirDefinitions()
    {
        QCOMPARE(blendModeNames().size(), int(BlendMode::Count));
        const float base[3] = {0.8f, 0.4f, 0.2f}, top[3] = {0.5f, 0.5f, 0.5f};
        float out[3];
        blendRgb(BlendMode::Normal, base, top, out);
        QCOMPARE(out[0], 0.5f);
        blendRgb(BlendMode::Multiply, base, top, out);
        QVERIFY(qAbs(out[0] - 0.4f) < 1e-5f);
        blendRgb(BlendMode::Screen, base, top, out);
        QVERIFY(qAbs(out[0] - (0.8f + 0.5f - 0.4f)) < 1e-5f);
        blendRgb(BlendMode::LinearDodge, base, top, out);
        QCOMPARE(out[0], 1.0f);
        blendRgb(BlendMode::Difference, base, top, out);
        QVERIFY(qAbs(out[0] - 0.3f) < 1e-5f);
        blendRgb(BlendMode::Darken, base, top, out);
        QCOMPARE(out[0], 0.5f);
        blendRgb(BlendMode::Lighten, base, top, out);
        QCOMPARE(out[0], 0.8f);
        // multiplying by white and screening black change nothing; screening white gives white
        const float white[3] = {1, 1, 1}, black[3] = {0, 0, 0};
        blendRgb(BlendMode::Multiply, base, white, out);
        QCOMPARE(out[1], 0.4f);
        blendRgb(BlendMode::Screen, base, black, out);
        QVERIFY(qAbs(out[1] - 0.4f) < 1e-6f);
        blendRgb(BlendMode::Screen, base, white, out);
        QCOMPARE(out[2], 1.0f);
        // Luminosity keeps the base's hue but takes the top's brightness: a grey top over a colour gives a colour of that luma
        const float lumTop[3] = {0.9f, 0.9f, 0.9f};
        blendRgb(BlendMode::Luminosity, base, lumTop, out);
        const float l = 0.3f * out[0] + 0.59f * out[1] + 0.11f * out[2];
        QVERIFY(qAbs(l - 0.9f) < 0.02f);
        // every mode stays inside 0..1
        for (int m = 0; m < int(BlendMode::Count); ++m) {
            blendRgb(BlendMode(m), base, top, out);
            for (int i = 0; i < 3; ++i)
                QVERIFY2(out[i] >= -1e-6f && out[i] <= 1.0f + 1e-6f, qPrintable(blendModeNames().at(m)));
        }
    }

    void compositingAUnitOfLayerOverATransparentPixelTakesTheLayerColour()
    {
        uint8_t px[4] = {0, 0, 0, 0};
        compositeOver(px, 200, 100, 50, 1.0, BlendMode::Multiply);
        QCOMPARE(int(px[0]), 200);
        QCOMPARE(int(px[3]), 255);
        uint8_t half[4] = {100, 100, 100, 255};
        compositeOver(half, 200, 200, 200, 0.5, BlendMode::Normal);
        QVERIFY(qAbs(int(half[0]) - 150) <= 1);
        QCOMPARE(int(half[3]), 255);
    }

    // --- the catalogue's new fields ----------------------------------------------------------

    void newCatalogueFieldsAreConsistent()
    {
        for (const EffectSpec &fx : allEffects()) {
            const int n = int(fx.params.size());
            for (int i = 0; i < n; ++i) {
                const EffectParam &p = fx.params[size_t(i)];
                if (p.color) {
                    QVERIFY2(p.min == 0.0 && p.max == 16777215.0, qPrintable(fx.id));
                    QVERIFY2(p.integer, qPrintable(fx.id));
                }
                if (p.dependsOn >= 0) {
                    QVERIFY2(p.dependsOn < n && p.dependsOn != i, qPrintable(fx.id + "/" + p.label));
                    const EffectParam &c = fx.params[size_t(p.dependsOn)];
                    QVERIFY2(c.toggle || !c.options.isEmpty(), qPrintable(fx.id + " depends on a slider"));
                    QVERIFY2(p.dependsMask != 0, qPrintable(fx.id));
                }
                if (p.options.size() > 8 && p.label == QLatin1String("Fusión"))
                    QCOMPARE(int(p.options.size()), int(BlendMode::Count));
            }
            for (const EffectPreset &pr : fx.presets) {
                QVERIFY2(!pr.name.isEmpty(), qPrintable(fx.id));
                for (const auto &[index, value] : pr.sets) {
                    QVERIFY2(index >= 0 && index < n, qPrintable(fx.id + "/" + pr.name));
                    QVERIFY2(value >= fx.params[size_t(index)].min && value <= fx.params[size_t(index)].max,
                             qPrintable(fx.id + "/" + pr.name));
                }
            }
            for (const EffectOverlay &o : fx.overlays) {
                if (o.kind == EffectOverlay::Point)
                    QVERIFY2(o.x >= 0 && o.x < n && o.y >= 0 && o.y < n, qPrintable(fx.id));
                else if (o.kind == EffectOverlay::VLine)
                    QVERIFY2(o.x >= 0 && o.x < n, qPrintable(fx.id));
                else
                    QVERIFY2(o.y >= 0 && o.y < n, qPrintable(fx.id));
            }
        }
    }

    void presetsSetOnlyTheirOwnControls()
    {
        const EffectSpec *bw = findEffect(QStringLiteral("bw"));
        QVERIFY(bw);
        QVERIFY(bw->presets.size() >= 3);
        EffectValues v = defaultEffectValues(*bw);
        v[6] = 1; // tint on
        const EffectValues red = applyEffectPreset(*bw, bw->presets[2], v); // "Filtro rojo"
        QCOMPARE(red[0], 120.0);
        QCOMPARE(red[3], -50.0);
        QCOMPARE(red[6], 1.0); // not touched
    }

    // --- Blanco y negro (PhotoScape's channel-mixer weights) -----------------------------------

    void blackAndWhiteUsesTheSixColourWeights()
    {
        const EffectSpec *bw = findEffect(QStringLiteral("bw"));
        const EffectValues v = defaultEffectValues(*bw); // R40 Y60 G40 C60 B20 M80
        auto grey = [&](int r, int g, int b) { return qRed(at(applyEffect(solid(r, g, b), QStringLiteral("bw"), v), 3, 3)); };
        QCOMPARE(grey(255, 0, 0), 102);
        QCOMPARE(grey(255, 255, 0), 153);
        QCOMPARE(grey(0, 255, 0), 102);
        QCOMPARE(grey(0, 255, 255), 153);
        QCOMPARE(grey(0, 0, 255), 51);
        QCOMPARE(grey(255, 0, 255), 204);
        QCOMPARE(grey(128, 128, 128), 128);
        QVERIFY(qAbs(grey(255, 128, 128) - 178) <= 1); // red with some white: white + (127 * 0.4)
    }

    void theNeutralPresetReproducesLuminance()
    {
        const EffectSpec *bw = findEffect(QStringLiteral("bw"));
        const EffectValues v = applyEffectPreset(*bw, bw->presets[1], defaultEffectValues(*bw)); // "Neutro"
        for (const QColor &c : {QColor(200, 90, 30), QColor(20, 180, 220), QColor(240, 240, 40), QColor(90, 20, 150)}) {
            const int got = qRed(at(applyEffect(solid(c.red(), c.green(), c.blue()), QStringLiteral("bw"), v), 2, 2));
            const double luma = 0.299 * c.red() + 0.587 * c.green() + 0.114 * c.blue();
            QVERIFY2(qAbs(got - luma) < 2.5, qPrintable(c.name()));
        }
    }

    void theTintKeepsTheBrightnessAndStaysInRange()
    {
        const EffectSpec *bw = findEffect(QStringLiteral("bw"));
        EffectValues v = defaultEffectValues(*bw);
        v[6] = 1;
        v[7] = packRgb(150, 115, 75);
        v[8] = 100;
        for (int g : {0, 3, 60, 128, 200, 252, 255}) {
            const QRgb p = at(applyEffect(solid(g, g, g), QStringLiteral("bw"), v), 1, 1);
            const double luma = 0.299 * qRed(p) + 0.587 * qGreen(p) + 0.114 * qBlue(p);
            QVERIFY2(qAbs(luma - g) < 2.0, qPrintable(QString::number(g)));
            if (g >= 60 && g <= 200)
                QVERIFY2(qRed(p) > qBlue(p) + 20, "warm tint");
        }
    }

    // --- Celofán and Aberración cromática ---------------------------------------------------------

    void cellophaneKeepsRedAndMovesGreenAndBlue()
    {
        const int w = 300, h = 240; // min side 240: green moves by 240/30 = 8 px, blue by (240/46, 240/20.5) = (5.2, 11.7)
        const QImage src = squares(w, h, {QPoint(130, 100)});
        const QImage out = applyEffect(src, QStringLiteral("cellophane"), {100, 0});
        const QRect area(100, 70, 120, 120);
        const QRect red = channelBox(out, 0, area), green = channelBox(out, 1, area), blue = channelBox(out, 2, area);
        QCOMPARE(red, QRect(130, 100, 40, 40));                  // red does not move
        QVERIFY(qAbs(green.x() - (130 - 8)) <= 1 && qAbs(green.y() - (100 - 8)) <= 1);
        QVERIFY(qAbs(blue.x() - (130 - 5)) <= 1 && qAbs(blue.y() - (100 - 12)) <= 1);
        QCOMPARE(green.width(), 40);
        // amount 0 changes nothing
        QCOMPARE(applyEffect(src, QStringLiteral("cellophane"), {0, 0}), src);
    }

    void cellophaneAngleTurnsTheFringes()
    {
        const QImage src = squares(300, 240, {QPoint(130, 100)});
        const QImage out = applyEffect(src, QStringLiteral("cellophane"), {100, 180});
        const QRect green = channelBox(out, 1, QRect(100, 70, 120, 120));
        QVERIFY(green.x() > 130 + 5 && green.y() > 100 + 5); // turned half a revolution: green now moves down-right
    }

    void chromaticAberrationLeavesTheCentreAndRedAlone()
    {
        const int w = 400, h = 300;
        QImage src(w, h, QImage::Format_RGBA8888);
        src.fill(Qt::black);
        for (int x = 0; x < w; ++x) // thin vertical bars every 40 px
            if (x % 40 < 3)
                for (int y = 0; y < h; ++y)
                    src.setPixelColor(x, y, Qt::white);
        const QImage out = applyEffect(src, QStringLiteral("chromatic"), {100, 0, 0, 0});
        // red untouched everywhere
        for (int y = 0; y < h; y += 17)
            for (int x = 0; x < w; ++x)
                QCOMPARE(int(out.constScanLine(y)[x * 4]), int(src.constScanLine(y)[x * 4]));
        // near the middle green sits where it was; further out it moves outward (toward smaller x on the left half)
        auto barCentre = [&](int nominalX, int ch) {
            double sum = 0, wsum = 0;
            for (int x = std::max(0, nominalX - 12); x < std::min(w, nominalX + 15); ++x) {
                const double v = out.constScanLine(150)[x * 4 + ch];
                sum += v * x;
                wsum += v;
            }
            return wsum > 0 ? sum / wsum : -1.0;
        };
        const double middle = barCentre(200, 1) - barCentre(200, 0);
        QVERIFY2(qAbs(middle) < 0.6, qPrintable(QString::number(middle)));
        const double leftShift = barCentre(80, 1) - barCentre(80, 0);
        QVERIFY2(leftShift < -0.8, qPrintable(QString::number(leftShift))); // green outward = left of red on the left side
        const double rightShift = barCentre(320, 1) - barCentre(320, 0);
        QVERIFY2(rightShift > 0.8, qPrintable(QString::number(rightShift)));
        // the opposite sign moves it the other way
        const QImage back = applyEffect(src, QStringLiteral("chromatic"), {-100, 0, 0, 0});
        double sum = 0, wsum = 0;
        for (int x = 68; x < 95; ++x) {
            const double v = back.constScanLine(150)[x * 4 + 1];
            sum += v * x;
            wsum += v;
        }
        QVERIFY(sum / wsum > barCentre(80, 0) + 0.8);
    }

    // --- Borrar niebla ----------------------------------------------------------------------------

    void dehazeFollowsTheDarkChannelModel()
    {
        // A flat grey patch on white: with the air light white, J = (I - 255) / t + 255 and t = 1 - omega * I / 255.
        // At "Cantidad" 50 PhotoScape's omega is 0.41, so a grey of 128 becomes about 96.
        QImage img(200, 200, QImage::Format_RGBA8888);
        img.fill(Qt::white);
        for (int y = 60; y < 140; ++y)
            for (int x = 60; x < 140; ++x)
                img.setPixelColor(x, y, QColor(128, 128, 128));
        const QImage out = applyEffect(img, QStringLiteral("dehaze"), {50, 0, 40, 1});
        QVERIFY2(qAbs(qRed(at(out, 100, 100)) - 96) <= 4, qPrintable(QString::number(qRed(at(out, 100, 100)))));
        QVERIFY(qRed(at(out, 5, 5)) >= 250); // white stays white
        // saturated colours have a dark channel of 0: left as they are
        QImage colours(120, 120, QImage::Format_RGBA8888);
        colours.fill(QColor(0, 255, 255));
        const QImage cyan = applyEffect(colours, QStringLiteral("dehaze"), {100, 0, 40, 1});
        QVERIFY(qAbs(qRed(at(cyan, 60, 60)) - 0) <= 2 && qAbs(qGreen(at(cyan, 60, 60)) - 255) <= 2);
        // lifting the shadows brightens the result
        const QImage lifted = applyEffect(img, QStringLiteral("dehaze"), {50, 80, 40, 1});
        QVERIFY(qRed(at(lifted, 100, 100)) > qRed(at(out, 100, 100)) + 5);
    }

    void dehazeAtZeroKeepsThePicture()
    {
        const QImage src = solid(90, 140, 200, 40, 30);
        QCOMPARE(applyEffect(src, QStringLiteral("dehaze"), {0, 0, 40, 0}), src);
    }

    // --- Mejorar documento -----------------------------------------------------------------------

    void documentFlattensUnevenLightingAndKeepsTheInk()
    {
        // Paper that is darker on the left than on the right, with a black stroke on it.
        const int w = 320, h = 200;
        QImage page(w, h, QImage::Format_RGBA8888);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int v = 120 + x * 120 / w; // 120..240
                page.setPixelColor(x, y, QColor(v, v, v));
            }
        for (int x = 40; x < 280; ++x)
            for (int y = 95; y < 99; ++y)
                page.setPixelColor(x, y, QColor(20, 20, 20));
        const EffectSpec *doc = findEffect(QStringLiteral("document"));
        EffectValues v = defaultEffectValues(*doc);
        v[0] = 1; // black and white page
        v[1] = 50;
        const QImage out = applyEffect(page, QStringLiteral("document"), v);
        const double left = meanChannel(out, QRect(10, 10, 20, 60), 0);
        const double right = meanChannel(out, QRect(290, 10, 20, 60), 0);
        QVERIFY2(left > 215 && right > 215, qPrintable(QString("%1 %2").arg(left).arg(right))); // both ends are white paper now
        QVERIFY(qAbs(left - right) < 20);
        QVERIFY(qRed(at(out, 160, 96)) < 70); // the stroke is still dark
    }

    void documentTextModeGivesPureBlackOnWhite()
    {
        const int w = 300, h = 160;
        QImage page(w, h, QImage::Format_RGBA8888);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const int v = 150 + y * 80 / h;
                page.setPixelColor(x, y, QColor(v, v, v));
            }
        for (int y = 70; y < 76; ++y)
            for (int x = 30; x < 270; ++x)
                page.setPixelColor(x, y, QColor(60, 60, 60));
        const EffectSpec *doc = findEffect(QStringLiteral("document"));
        EffectValues v = defaultEffectValues(*doc);
        v[0] = 3;
        const QImage out = applyEffect(page, QStringLiteral("document"), v);
        QVERIFY(qRed(at(out, 150, 73)) < 40);
        QVERIFY(qRed(at(out, 150, 20)) > 245);
        QVERIFY(qRed(at(out, 150, 140)) > 245);
    }

    // --- Semitono (print halftone) --------------------------------------------------------------

    void printHalftoneKeepsTheTone()
    {
        const EffectSpec *ht = findEffect(QStringLiteral("halftoneprint"));
        QVERIFY(ht);
        for (int tone : {64, 128, 192}) {
            QImage grey(400, 300, QImage::Format_RGBA8888);
            grey.fill(QColor(tone, tone, tone));
            const QImage out = applyEffect(grey, QStringLiteral("halftoneprint"), defaultEffectValues(*ht));
            const double mean = meanChannel(out, QRect(60, 60, 280, 180), 0);
            QVERIFY2(qAbs(mean - tone) < 9.0, qPrintable(QString("tone %1 -> %2").arg(tone).arg(mean)));
        }
    }

    void printHalftoneMakesInkDotsOnPaper()
    {
        const EffectSpec *ht = findEffect(QStringLiteral("halftoneprint"));
        QImage lightGrey(1000, 600, QImage::Format_RGBA8888); // dots about 11 px apart
        lightGrey.fill(QColor(200, 200, 200));
        EffectValues v = defaultEffectValues(*ht);
        v[1] = 2; // black ink only
        const QImage out = applyEffect(lightGrey, QStringLiteral("halftoneprint"), v);
        int paper = 0, ink = 0, other = 0;
        for (int y = 100; y < 500; ++y)
            for (int x = 100; x < 900; ++x) {
                const int r = qRed(at(out, x, y));
                if (r > 250) ++paper; else if (r < 5) ++ink; else ++other;
            }
        QVERIFY(paper > ink * 2); // light tone: mostly paper, small dots
        QVERIFY(ink > 5000);
        QVERIFY(other < (paper + ink) / 2); // dots with crisp edges, not a blur
        // pure colours print as solid ink
        QImage cyan(100, 100, QImage::Format_RGBA8888);
        cyan.fill(QColor(0, 255, 255));
        const QImage solidCyan = applyEffect(cyan, QStringLiteral("halftoneprint"), defaultEffectValues(*ht));
        QVERIFY(qRed(at(solidCyan, 50, 50)) < 8 && qGreen(at(solidCyan, 50, 50)) > 247);
    }

    // --- Dibujo: lines, rings, speed lines, fills, border line ------------------------------------

    void linesAreSpacedByDensityRelativeToTheLongSide()
    {
        QImage black(400, 200, QImage::Format_RGBA8888);
        black.fill(Qt::black);
        const EffectSpec *fx = findEffect(QStringLiteral("lines"));
        EffectValues v = defaultEffectValues(*fx);
        v[0] = 0; // horizontal
        v[1] = 50; v[2] = 20; v[8] = 100; v[9] = 0;
        const QImage out = applyEffect(black, QStringLiteral("lines"), v);
        // spacing = long side * 0.95 / density = 400 * 0.95 / 50 = 7.6 px: count the lit rows along a column
        int rows = 0;
        bool prev = false;
        for (int y = 0; y < 200; ++y) {
            const bool lit = qRed(at(out, 100, y)) > 100;
            if (lit && !prev)
                ++rows;
            prev = lit;
        }
        QVERIFY2(rows >= 25 && rows <= 27, qPrintable(QString::number(rows)));
        // the same on a picture twice as big has twice the spacing in pixels (same number of lines)
        QImage big(800, 400, QImage::Format_RGBA8888);
        big.fill(Qt::black);
        const QImage outBig = applyEffect(big, QStringLiteral("lines"), v);
        int rowsBig = 0;
        prev = false;
        for (int y = 0; y < 400; ++y) {
            const bool lit = qRed(at(outBig, 100, y)) > 100;
            if (lit && !prev)
                ++rowsBig;
            prev = lit;
        }
        QVERIFY(qAbs(rowsBig - rows) <= 1);
    }

    void ringsDrawOnlyBetweenTheirRadii()
    {
        QImage black(300, 300, QImage::Format_RGBA8888);
        black.fill(Qt::black);
        const EffectSpec *fx = findEffect(QStringLiteral("rings"));
        EffectValues v = defaultEffectValues(*fx);
        v[3] = 40; v[4] = 70; v[10] = 100; v[11] = 0; // from 40% to 70% of the reach
        const QImage out = applyEffect(black, QStringLiteral("rings"), v);
        const double reach = std::hypot(149.5, 149.5);
        bool inside = false;
        for (int x = 150; x < 300; ++x) {
            const double r = x - 149.5;
            const bool lit = qRed(at(out, x, 150)) > 60;
            if (lit)
                QVERIFY2(r > 0.4 * reach - 3 && r < 0.7 * reach + 3, qPrintable(QString::number(r)));
            inside |= lit;
        }
        QVERIFY(inside);
        QCOMPARE(qRed(at(out, 150, 150)), 0); // nothing at the very centre
    }

    void speedLinesAreReproducibleAndSeedDependent()
    {
        const QImage src = solid(30, 60, 120, 240, 160);
        const EffectSpec *fx = findEffect(QStringLiteral("speed"));
        EffectValues v = defaultEffectValues(*fx);
        const QImage a = applyEffect(src, QStringLiteral("speed"), v);
        QCOMPARE(a, applyEffect(src, QStringLiteral("speed"), v));
        v[6] = 123;
        QVERIFY(applyEffect(src, QStringLiteral("speed"), v) != a);
        // nothing inside the inner radius, and the original shows between rays
        QCOMPARE(at(a, 119, 79), at(src, 119, 79));
    }

    void gradientFillCoversEveryPixelAtFullOpacity()
    {
        const QImage src = solid(0, 0, 0, 100, 60);
        const EffectSpec *fx = findEffect(QStringLiteral("gradient"));
        EffectValues v = defaultEffectValues(*fx);
        v[1] = 0; // custom
        v[2] = packRgb(255, 0, 0);
        v[3] = packRgb(0, 0, 255);
        v[11] = 100; v[12] = 0; // opacity 100, normal blend
        v[0] = 0; v[7] = 0;     // linear, left to right
        const QImage out = applyEffect(src, QStringLiteral("gradient"), v);
        QVERIFY(qRed(at(out, 0, 30)) > 240 && qBlue(at(out, 0, 30)) < 15);
        QVERIFY(qBlue(at(out, 99, 30)) > 240 && qRed(at(out, 99, 30)) < 15);
        QVERIFY(qAbs(qRed(at(out, 50, 30)) - 127) < 8);
        v[6] = 1; // inverted
        const QImage flipped = applyEffect(src, QStringLiteral("gradient"), v);
        QVERIFY(qBlue(at(flipped, 0, 30)) > 240);
    }

    void patternFillAlternatesBetweenItsTwoColours()
    {
        const QImage src = solid(128, 128, 128, 200, 120);
        const EffectSpec *fx = findEffect(QStringLiteral("pattern"));
        EffectValues v = defaultEffectValues(*fx);
        v[0] = 5;  // checkerboard
        v[1] = packRgb(0, 0, 0);
        v[2] = packRgb(255, 255, 255);
        v[3] = 100; v[7] = 100; v[8] = 0; // opacity 100, normal
        const QImage out = applyEffect(src, QStringLiteral("pattern"), v);
        int black = 0, white = 0;
        for (int y = 0; y < 120; y += 3)
            for (int x = 0; x < 200; x += 3) {
                const int r = qRed(at(out, x, y));
                if (r < 30) ++black; else if (r > 225) ++white;
            }
        QVERIFY(black > 100 && white > 100);
        QVERIFY(qAbs(black - white) < (black + white) / 6); // about half and half
        // every motif renders (no crash, different from the picture)
        const EffectParam &motifs = fx->params[0];
        for (int k = 0; k < motifs.options.size(); ++k) {
            v[0] = k;
            QVERIFY2(applyEffect(src, QStringLiteral("pattern"), v) != src, qPrintable(motifs.options.at(k)));
        }
    }

    void borderLineDrawsAFrameInsideTheEdge()
    {
        QImage black(200, 140, QImage::Format_RGBA8888);
        black.fill(Qt::black);
        const EffectSpec *fx = findEffect(QStringLiteral("edgeline"));
        EffectValues v = defaultEffectValues(*fx);
        v[5] = 100; v[6] = 0;
        for (int style = 0; style < 5; ++style) {
            v[3] = style;
            const QImage out = applyEffect(black, QStringLiteral("edgeline"), v);
            QVERIFY2(out != black, qPrintable(QString::number(style)));
            QCOMPARE(qRed(at(out, 100, 70)), 0);  // the middle is untouched
            QCOMPARE(qRed(at(out, 0, 0)), 0);     // and so is the very corner (the line is inset)
        }
        v[3] = 0; v[2] = 100; // fully round
        const QImage round = applyEffect(black, QStringLiteral("edgeline"), v);
        QVERIFY(qRed(at(round, 100, 70)) == 0);
    }

    // --- Luz ----------------------------------------------------------------------------------------

    static QImage dimScene(int w, int h)
    {
        QImage img(w, h, QImage::Format_RGBA8888);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                img.setPixelColor(x, y, QColor(30 + x * 40 / w, 40 + y * 40 / h, 70));
        for (int y = h / 2; y < h / 2 + h / 8; ++y) // a bright patch, for the effects that work from the highlights
            for (int x = w / 2; x < w / 2 + w / 8; ++x)
                img.setPixelColor(x, y, QColor(250, 245, 230));
        return img;
    }

    void lightEffectsOnlyAddLightAndAreReproducible()
    {
        const QImage src = dimScene(320, 220);
        for (const char *name : {"bokeh", "flare", "leak", "beams", "dust", "sparkles", "glow"}) {
            const QString id = QString::fromLatin1(name);
            const EffectSpec *fx = findEffect(id);
            QVERIFY2(fx, name);
            QCOMPARE(fx->group, QStringLiteral("light"));
            EffectValues v = defaultEffectValues(*fx);
            const QImage out = applyEffect(src, id, v);
            QCOMPARE(out, applyEffect(src, id, v)); // the same picture every time
            int brighter = 0;
            for (int y = 0; y < src.height(); ++y)
                for (int x = 0; x < src.width(); ++x)
                    for (int c = 0; c < 3; ++c) {
                        const int d = int(out.constScanLine(y)[x * 4 + c]) - int(src.constScanLine(y)[x * 4 + c]);
                        QVERIFY2(d >= -1, name); // Trama never darkens
                        brighter += d > 8;
                    }
            QVERIFY2(brighter > 50, name);
            // a seed makes another arrangement
            for (size_t i = 0; i < fx->params.size(); ++i)
                if (fx->params[i].seed) {
                    EffectValues other = v;
                    other[i] = v[i] + 17;
                    QVERIFY2(applyEffect(src, id, other) != out, name);
                }
        }
    }

    void bokehShapesAllDrawSomething()
    {
        const QImage src = dimScene(300, 200);
        const EffectSpec *fx = findEffect(QStringLiteral("bokeh"));
        EffectValues v = defaultEffectValues(*fx);
        const EffectParam &shapes = fx->params[3];
        QImage first;
        for (int k = 0; k < shapes.options.size(); ++k) {
            v[3] = k;
            const QImage out = applyEffect(src, QStringLiteral("bokeh"), v);
            QVERIFY2(out != src, qPrintable(shapes.options.at(k)));
            if (k == 0)
                first = out;
            else
                QVERIFY2(out != first, qPrintable(shapes.options.at(k)));
        }
    }

    void theSpotlightDarkensTheOutsideAndLightsTheInside()
    {
        const QImage src = solid(120, 120, 120, 200, 140);
        const EffectSpec *fx = findEffect(QStringLiteral("spotlight"));
        EffectValues v = defaultEffectValues(*fx);
        v[0] = 50; v[1] = 50; v[2] = 25; v[3] = 20; v[4] = 60; v[5] = 70;
        const QImage out = applyEffect(src, QStringLiteral("spotlight"), v);
        QVERIFY(qRed(at(out, 100, 70)) > 150);   // the middle is lit
        QVERIFY(qRed(at(out, 2, 2)) < 60);        // the corner is in shade
        v[5] = 0; v[4] = 0;
        QCOMPARE(applyEffect(src, QStringLiteral("spotlight"), v), src);
    }

    void sparklesGoWhereTheLightIs()
    {
        QImage src(900, 600, QImage::Format_RGBA8888);
        src.fill(QColor(15, 15, 25));
        for (int y = 288; y < 312; ++y)
            for (int x = 588; x < 612; ++x)
                src.setPixelColor(x, y, QColor(255, 255, 255));
        const EffectSpec *fx = findEffect(QStringLiteral("sparkles"));
        EffectValues v = defaultEffectValues(*fx);
        v[0] = 1;  // a single sparkle
        v[5] = 1;  // on the lights
        v[1] = 60;
        const QImage out = applyEffect(src, QStringLiteral("sparkles"), v);
        // the arms of the star reach past the bright square it sits on
        int lit = 0;
        for (int y = 258; y < 342; ++y)
            for (int x = 558; x < 642; ++x)
                if ((std::abs(x - 600) > 16 || std::abs(y - 300) > 16) && qRed(at(out, x, y)) > 15 + 30)
                    ++lit;
        QVERIFY2(lit > 20, qPrintable(QString::number(lit)));
        // while the far corners did not change
        QCOMPARE(at(out, 5, 5), at(src, 5, 5));
        QCOMPARE(at(out, 894, 594), at(src, 894, 594));
    }

    // --- every effect, tiny and odd sizes, cancellation -----------------------------------------

    void everyEffectSurvivesTinyPictures()
    {
        for (const QSize &size : {QSize(2, 2), QSize(3, 17), QSize(41, 2), QSize(5, 5)}) {
            QImage src(size, QImage::Format_RGBA8888);
            src.fill(QColor(90, 120, 200));
            for (const EffectSpec &fx : allEffects()) {
                const QImage out = applyEffect(src, fx.id, defaultEffectValues(fx));
                const QSize expect = effectOutputSize(fx.id, defaultEffectValues(fx), size);
                QVERIFY2(out.size() == expect, qPrintable(fx.id));
            }
        }
    }

    void everyEffectHandlesTransparentPictures()
    {
        QImage src(60, 40, QImage::Format_RGBA8888);
        src.fill(QColor(0, 0, 0, 0));
        for (int y = 10; y < 30; ++y)
            for (int x = 15; x < 45; ++x)
                src.setPixelColor(x, y, QColor(200, 80, 40, 255));
        for (const EffectSpec &fx : allEffects()) {
            const QImage out = applyEffect(src, fx.id, defaultEffectValues(fx));
            QVERIFY2(!out.isNull() && out.format() == QImage::Format_RGBA8888, qPrintable(fx.id));
        }
    }

    void extremeValuesAreClampedNotCrashing()
    {
        const QImage src = solid(100, 150, 200, 64, 48);
        for (const EffectSpec &fx : allEffects()) {
            EffectValues lo{}, hi{};
            for (size_t i = 0; i < fx.params.size(); ++i) {
                lo[i] = fx.params[i].min - 1000;
                hi[i] = fx.params[i].max + 1000;
            }
            QVERIFY2(!applyEffect(src, fx.id, lo).isNull(), qPrintable(fx.id));
            QVERIFY2(!applyEffect(src, fx.id, hi).isNull(), qPrintable(fx.id));
        }
    }
};

QTEST_MAIN(TestEffectFamilies)
#include "test_effect_families.moc"
