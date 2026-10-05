#include <QtTest>
#include "edit/Effects.h"

#include <cmath>

using namespace core::edit;

namespace {

QImage solid(int w, int h, int r, int g, int b)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(r, g, b));
    return img;
}

double packRgb(int r, int g, int b) { return double((r << 16) | (g << 8) | b); }

// The frame's values: the catalogue defaults with the named parameters changed.
EffectValues frameValues(std::initializer_list<std::pair<const char *, double>> sets)
{
    const EffectSpec *spec = findEffect(QStringLiteral("frame"));
    EffectValues v = defaultEffectValues(*spec);
    for (const auto &[label, value] : sets) {
        bool found = false;
        for (size_t i = 0; i < spec->params.size(); ++i)
            if (spec->params[i].label == QString::fromUtf8(label)) {
                v[i] = value;
                found = true;
            }
        if (!found)
            qFatal("no frame parameter called %s", label);
    }
    return v;
}

// Nothing but the picture: no corners, outline, margin or shadow.
EffectValues plain(std::initializer_list<std::pair<const char *, double>> sets = {})
{
    EffectValues v = frameValues({{"Redondez", 0}, {"Contorno", 0}, {"Sombra", 0}, {"Margen", 0}});
    const EffectSpec *spec = findEffect(QStringLiteral("frame"));
    for (const auto &[label, value] : sets)
        for (size_t i = 0; i < spec->params.size(); ++i)
            if (spec->params[i].label == QString::fromUtf8(label))
                v[i] = value;
    return v;
}

QImage frame(const QImage &src, const EffectValues &v) { return applyEffect(src, QStringLiteral("frame"), v); }

int alphaAt(const QImage &img, int x, int y) { return qAlpha(img.pixel(x, y)); }
bool opaque(const QImage &img, int x, int y) { return alphaAt(img, x, y) >= 250; }
bool clear(const QImage &img, int x, int y) { return alphaAt(img, x, y) <= 5; }
bool near(QRgb c, int r, int g, int b, int tol = 6)
{
    return std::abs(qRed(c) - r) <= tol && std::abs(qGreen(c) - g) <= tol && std::abs(qBlue(c) - b) <= tol;
}

} // namespace

class TestFrame : public QObject {
    Q_OBJECT

private slots:
    void aPlainFrameChangesNothing()
    {
        QImage src(120, 80, QImage::Format_RGBA8888);
        for (int y = 0; y < 80; ++y)
            for (int x = 0; x < 120; ++x)
                src.setPixelColor(x, y, QColor(x * 2, y * 3, (x + y) % 256));
        const QImage out = frame(src, plain());
        QCOMPARE(out.size(), src.size());
        for (int y : {0, 7, 40, 79})
            for (int x : {0, 5, 60, 119})
                QVERIFY2(near(out.pixel(x, y), qRed(src.pixel(x, y)), qGreen(src.pixel(x, y)), qBlue(src.pixel(x, y)), 1),
                         qPrintable(QString("%1,%2").arg(x).arg(y)));
    }

    void roundCornersAreCutAndSoftEdged()
    {
        const QImage src = solid(200, 200, 90, 160, 220);
        // 50 %: the corner starts 50 px along each edge, a circle of radius 50 centred at (50, 50)
        const QImage out = frame(src, plain({{"Redondez", 50}}));
        QCOMPARE(out.size(), src.size());
        QVERIFY(clear(out, 0, 0));
        QVERIFY(clear(out, 10, 10));      // 56 px from the centre of the arc: outside it
        QVERIFY(opaque(out, 20, 20));     // 42 px: inside
        QVERIFY(opaque(out, 100, 100));
        QVERIFY(opaque(out, 100, 0));     // the middle of an edge is untouched
        QVERIFY(clear(out, 199, 199) && clear(out, 0, 199) && clear(out, 199, 0));
        QVERIFY(near(out.pixel(100, 100), 90, 160, 220, 1));
        int soft = 0;
        for (int y = 0; y < 60; ++y)
            for (int x = 0; x < 60; ++x)
                soft += alphaAt(out, x, y) > 10 && alphaAt(out, x, y) < 245;
        QVERIFY2(soft > 40, qPrintable(QString::number(soft))); // anti-aliased, not a staircase
    }

    void eachCornerCanBeSwitchedOff()
    {
        const QImage src = solid(200, 200, 200, 50, 50);
        // bits: top left 1, top right 2, bottom right 4, bottom left 8
        const QImage out = frame(src, plain({{"Redondez", 50}, {"Esquinas activas", 1}}));
        QVERIFY(clear(out, 0, 0));
        QVERIFY(opaque(out, 199, 0));
        QVERIFY(opaque(out, 199, 199));
        QVERIFY(opaque(out, 0, 199));
        const QImage two = frame(src, plain({{"Redondez", 50}, {"Esquinas activas", 6}}));
        QVERIFY(opaque(two, 0, 0) && clear(two, 199, 0) && clear(two, 199, 199) && opaque(two, 0, 199));
    }

    void cutCornersAreStraight()
    {
        const QImage src = solid(200, 200, 40, 200, 90);
        // style 2 = Cortadas: a straight line from 50 px along one edge to 50 px along the other
        const QImage out = frame(src, plain({{"Redondez", 50}, {"Esquinas", 2}}));
        QVERIFY(clear(out, 10, 10));   // x + y = 20 < 50
        QVERIFY(clear(out, 20, 20));   // 40 < 50
        QVERIFY(opaque(out, 30, 30));  // 60 > 50
        QVERIFY(clear(out, 40, 5));
        QVERIFY(opaque(out, 60, 5));
        // a round corner of the same size keeps more of the picture than a cut one
        const QImage round = frame(src, plain({{"Redondez", 50}, {"Esquinas", 0}}));
        QVERIFY(opaque(round, 20, 20) && clear(out, 20, 20));
    }

    void hollowCornersBiteIn()
    {
        const QImage src = solid(200, 200, 40, 90, 200);
        // style 3 = Cóncavas: an arc centred on the corner itself (a ticket stub)
        const QImage out = frame(src, plain({{"Redondez", 50}, {"Esquinas", 3}}));
        QVERIFY(clear(out, 10, 10));
        QVERIFY(clear(out, 40, 3));    // 40 px from the corner < 50
        QVERIFY(clear(out, 30, 30));   // 42 px from the corner < 50
        QVERIFY(opaque(out, 45, 45));  // 63.6 px
        QVERIFY(opaque(out, 100, 100));
    }

    void shapesCutTheirOwnOutline()
    {
        const QImage src = solid(160, 120, 120, 120, 120);
        struct Case { const char *name; int shape; bool cornerClear; };
        for (const Case &c : {Case{"elipse", 1, true}, Case{"hexágono", 2, true}, Case{"octágono", 3, true}, Case{"rombo", 4, true},
                              Case{"triángulo", 5, true}, Case{"estrella", 6, true}, Case{"corazón", 7, true}}) {
            const QImage out = frame(src, plain({{"Forma", double(c.shape)}}));
            QVERIFY2(out.size() == src.size(), c.name);
            QVERIFY2(clear(out, 0, 0), c.name);
            QVERIFY2(clear(out, 159, 0) || c.shape == 5 || c.shape == 6, c.name);
            QVERIFY2(opaque(out, 80, 70), c.name);
        }
        // a triangle stands on its base: the apex is the top middle, the lower corners are full
        const QImage tri = frame(src, plain({{"Forma", 5}}));
        QVERIFY(opaque(tri, 3, 118) && opaque(tri, 156, 118) && opaque(tri, 80, 3) && clear(tri, 20, 20));
        // an ellipse touches the middle of every side
        const QImage ell = frame(src, plain({{"Forma", 1}}));
        QVERIFY(opaque(ell, 80, 1) && opaque(ell, 1, 60) && opaque(ell, 158, 60) && opaque(ell, 80, 118));
        // the corners of the other shapes can be rounded too
        const QImage hex = frame(src, plain({{"Forma", 2}, {"Redondez", 100}}));
        const QImage hexSharp = frame(src, plain({{"Forma", 2}}));
        QVERIFY(clear(hex, 3, 60) && opaque(hexSharp, 3, 60)); // the left tip is taken off
    }

    void anOutlineGrowsTheCanvasAndKeepsTheCorners()
    {
        const QImage src = solid(200, 100, 30, 120, 220);
        // 10 % of the short side (100 px) = 10 px all around
        const EffectValues v = plain({{"Contorno", 10}, {"Color del contorno", packRgb(255, 0, 0)}});
        const QImage out = frame(src, v);
        QCOMPARE(out.size(), QSize(220, 120));
        QCOMPARE(effectOutputSize(QStringLiteral("frame"), v, src.size()), QSize(220, 120));
        QVERIFY(opaque(out, 0, 0) && near(out.pixel(0, 0), 255, 0, 0, 2)); // a square shape keeps a square outline
        QVERIFY(near(out.pixel(219, 119), 255, 0, 0, 2));
        QVERIFY(near(out.pixel(5, 60), 255, 0, 0, 2));
        QVERIFY(near(out.pixel(110, 60), 30, 120, 220, 1));
        QVERIFY(near(out.pixel(10, 10), 30, 120, 220, 3));  // the picture starts exactly at (10, 10)
        QVERIFY(near(out.pixel(9, 9), 255, 0, 0, 3));
        // no light seam where the picture meets the outline: every pixel of the first row inside is picture or outline
        for (int x = 0; x < 220; ++x) {
            const QRgb c = out.pixel(x, 10);
            QVERIFY2(near(c, 255, 0, 0, 3) || near(c, 30, 120, 220, 3), qPrintable(QString::number(x)));
        }
    }

    void anOutlineFollowsRoundCorners()
    {
        const QImage src = solid(200, 200, 30, 120, 220);
        // 5 % of the short side (200 px) = a 10 px outline
        const EffectValues v = plain({{"Redondez", 50}, {"Contorno", 5}, {"Color del contorno", packRgb(255, 255, 255)}});
        const QImage out = frame(src, v);
        QCOMPARE(out.size(), QSize(220, 220));
        QVERIFY(clear(out, 0, 0));                           // the outline is round too
        QVERIFY(near(out.pixel(110, 4), 255, 255, 255, 2));  // a white band along the top
        QVERIFY(near(out.pixel(110, 12), 30, 120, 220, 2));  // then the picture
        // the band is about as thick on the diagonal as at the sides: the point on the arc at 45 degrees
        const double r = 60.0;                                // outline radius = 50 + 10
        const double c = 60.0;                                // arc centre (50, 50) moved by the 10 px outline
        const int onArc = int(c - r * std::sqrt(0.5)) + 3;    // just inside the outer edge
        QVERIFY2(near(out.pixel(onArc, onArc), 255, 255, 255, 3), qPrintable(QString::number(onArc)));
    }

    void anInsideOutlineKeepsTheSize()
    {
        const QImage src = solid(200, 100, 30, 120, 220);
        const EffectValues v = plain({{"Contorno", 10}, {"Contorno hacia adentro", 1}, {"Color del contorno", packRgb(255, 0, 0)}});
        const QImage out = frame(src, v);
        QCOMPARE(out.size(), src.size());
        QVERIFY(near(out.pixel(3, 50), 255, 0, 0, 2));
        QVERIFY(near(out.pixel(100, 3), 255, 0, 0, 2));
        QVERIFY(near(out.pixel(100, 50), 30, 120, 220, 1));
        QVERIFY(near(out.pixel(15, 50), 30, 120, 220, 2)); // past the 10 px band
    }

    void aSemiTransparentOutlineLetsTheBackgroundShowThrough()
    {
        const QImage src = solid(100, 100, 0, 0, 255);
        const EffectValues v = plain({{"Contorno", 10}, {"Color del contorno", packRgb(255, 0, 0)}, {"Opacidad del contorno", 50},
                                      {"Fondo", 0}, {"Color de fondo", packRgb(255, 255, 255)}, {"Margen", 5}});
        const QImage out = frame(src, v);
        // 50 % red over white = (255, 128, 128)
        QVERIFY2(near(out.pixel(8, 50), 255, 128, 128, 4), qPrintable(QString::number(qGreen(out.pixel(8, 50)))));
        QVERIFY(near(out.pixel(1, 50), 255, 255, 255, 2)); // the margin outside it
    }

    void marginAndKeepingTheSize()
    {
        const QImage src = solid(200, 100, 40, 40, 200);
        const EffectValues grown = plain({{"Margen", 10}, {"Fondo", 0}, {"Color de fondo", packRgb(255, 255, 255)}});
        const QImage a = frame(src, grown);
        QCOMPARE(a.size(), QSize(220, 120));
        QVERIFY(near(a.pixel(0, 0), 255, 255, 255, 1));
        QVERIFY(near(a.pixel(110, 60), 40, 40, 200, 1));

        EffectValues kept = grown;
        for (size_t i = 0; i < findEffect(QStringLiteral("frame"))->params.size(); ++i)
            if (findEffect(QStringLiteral("frame"))->params[i].label == QStringLiteral("Mantener el tamaño"))
                kept[i] = 1;
        const QImage b = frame(src, kept);
        QCOMPARE(b.size(), src.size());
        QCOMPARE(effectOutputSize(QStringLiteral("frame"), kept, src.size()), src.size());
        QVERIFY(near(b.pixel(0, 0), 255, 255, 255, 1));
        QVERIFY(near(b.pixel(100, 50), 40, 40, 200, 2));
        // the composition (220 x 120) shrank to 200/240 = 0.83 to fit a 200 x 100 picture's height, and is centred:
        // the picture is 167 px wide, starting at x = 17, and 83 px tall, starting at y = 8
        QVERIFY(near(b.pixel(13, 50), 255, 255, 255, 3));
        QVERIFY(near(b.pixel(21, 50), 40, 40, 200, 4));
        QVERIFY(near(b.pixel(100, 4), 255, 255, 255, 3));
        QVERIFY(near(b.pixel(100, 12), 40, 40, 200, 4));
    }

    void aShadowGrowsTheCanvasTowardsItsDirection()
    {
        const QImage src = solid(200, 100, 200, 200, 200);
        // straight to the right (angle 0), 10 % = 10 px away, no blur
        const EffectValues v = plain({{"Sombra", 100}, {"Distancia de la sombra", 10}, {"Desenfoque de la sombra", 0},
                                      {"Ángulo de la sombra", 0}});
        const QImage out = frame(src, v);
        QCOMPARE(out.size(), QSize(210, 100));
        QVERIFY(opaque(out, 205, 50));
        QVERIFY(near(out.pixel(205, 50), 0, 0, 0, 3));
        QVERIFY(near(out.pixel(100, 50), 200, 200, 200, 1));
        QVERIFY(clear(out, 205, 2) == false); // the shadow of a rectangle covers its height
        // pointing down-right (45) it grows both ways, up-left stays
        const EffectValues dr = plain({{"Sombra", 60}, {"Distancia de la sombra", 10}, {"Desenfoque de la sombra", 0},
                                       {"Ángulo de la sombra", 90}});
        QCOMPARE(frame(src, dr).size(), QSize(200, 110));
    }

    void aBlurredShadowFadesOut()
    {
        const QImage src = solid(300, 300, 220, 220, 220);
        const EffectValues v = plain({{"Sombra", 100}, {"Distancia de la sombra", 4}, {"Desenfoque de la sombra", 10},
                                      {"Ángulo de la sombra", 90}});
        const QImage out = frame(src, v);
        QVERIFY(out.height() > 330 && out.width() > 330);
        // below the picture: dark close to it, fading further down
        const int x = out.width() / 2;
        const int nearEdge = alphaAt(out, x, 304), far = alphaAt(out, x, out.height() - 2);
        QVERIFY2(nearEdge > 120, qPrintable(QString::number(nearEdge)));
        QVERIFY2(far < nearEdge / 2, qPrintable(QString("%1 vs %2").arg(far).arg(nearEdge)));
        QVERIFY(alphaAt(out, x, 304) > 0);
        // no shadow when its strength is 0
        QCOMPARE(frame(src, plain()).size(), src.size());
    }

    void backgrounds()
    {
        const QImage src = solid(100, 100, 200, 60, 60);
        auto bg = [&](int mode, std::initializer_list<std::pair<const char *, double>> more) {
            QList<std::pair<const char *, double>> sets = {{"Margen", 20}, {"Fondo", double(mode)}};
            for (const auto &m : more)
                sets.append(m);
            EffectValues v = plain();
            const EffectSpec *spec = findEffect(QStringLiteral("frame"));
            for (const auto &[label, value] : sets)
                for (size_t i = 0; i < spec->params.size(); ++i)
                    if (spec->params[i].label == QString::fromUtf8(label))
                        v[i] = value;
            return frame(src, v);
        };
        // transparent
        QVERIFY(clear(bg(2, {}), 0, 0));
        // a gradient from red on the left to blue on the right (angle 0)
        const QImage g = bg(1, {{"Color de fondo", packRgb(255, 0, 0)}, {"Segundo color", packRgb(0, 0, 255)}, {"Ángulo del degradado", 0}});
        QVERIFY(g.size() == QSize(140, 140));
        QVERIFY(qRed(g.pixel(0, 5)) > 230 && qBlue(g.pixel(0, 5)) < 25);
        QVERIFY(qBlue(g.pixel(139, 5)) > 230 && qRed(g.pixel(139, 5)) < 25);
        QVERIFY(opaque(g, 0, 0));
        // half-transparent colour
        const QImage half = bg(0, {{"Color de fondo", packRgb(255, 255, 255)}, {"Opacidad del fondo", 50}});
        QVERIFY(alphaAt(half, 0, 0) > 120 && alphaAt(half, 0, 0) < 136);
        // the blurred photo: the picture's own colour fills the margin
        const QImage p = bg(3, {});
        QVERIFY(opaque(p, 0, 0) && opaque(p, 139, 139));
        QVERIFY2(near(p.pixel(2, 2), 200, 60, 60, 4), qPrintable(QString::number(qRed(p.pixel(2, 2)))));
    }

    void everyShapeAndStyleRendersAtTheAnnouncedSize()
    {
        QImage src(97, 61, QImage::Format_RGBA8888);
        src.fill(QColor(120, 80, 200));
        const EffectSpec *spec = findEffect(QStringLiteral("frame"));
        QVERIFY(spec && spec->hidden && spec->changesSize);
        QVERIFY(spec->params.size() <= kMaxEffectParams);
        for (int shape = 0; shape <= 7; ++shape)
            for (int style = 0; style <= 3; ++style)
                for (int keep : {0, 1})
                    for (int inside : {0, 1}) {
                        EffectValues v = frameValues({{"Forma", double(shape)}, {"Esquinas", double(style)}, {"Redondez", 40},
                                                      {"Contorno", 6}, {"Margen", 4}, {"Sombra", 50}, {"Desenfoque de la sombra", 5},
                                                      {"Mantener el tamaño", double(keep)}, {"Contorno hacia adentro", double(inside)}});
                        const QImage out = frame(src, v);
                        const QSize announced = effectOutputSize(QStringLiteral("frame"), v, src.size());
                        QVERIFY2(out.size() == announced, qPrintable(QString("shape %1 style %2 keep %3 inside %4: %5x%6 vs %7x%8")
                                     .arg(shape).arg(style).arg(keep).arg(inside).arg(out.width()).arg(out.height())
                                     .arg(announced.width()).arg(announced.height())));
                        QVERIFY(!out.isNull());
                        if (keep)
                            QCOMPARE(out.size(), src.size());
                    }
    }

    void theCanvasNeverGrowsPastTheLimit()
    {
        // 6000 x 6000 with the largest margin, outline and shadow would be ~280 MP: it is drawn smaller instead
        const EffectValues v = frameValues({{"Margen", 50}, {"Contorno", 40}, {"Sombra", 100}, {"Distancia de la sombra", 20},
                                            {"Desenfoque de la sombra", 30}});
        const QSize size = effectOutputSize(QStringLiteral("frame"), v, QSize(6000, 6000));
        QVERIFY(double(size.width()) * size.height() <= 100.5e6);
        QVERIFY(size.width() <= 20000 && size.height() <= 20000);
        QVERIFY(size.width() > 6000); // still bigger than the picture
    }

    void aCancelledRenderReturnsQuickly()
    {
        const QImage src = solid(400, 300, 10, 20, 30);
        std::atomic<bool> cancel{true};
        const QImage out = applyEffect(src, QStringLiteral("frame"), frameValues({}), 1.0, &cancel);
        QVERIFY(!out.isNull()); // an unfinished image the caller discards
    }

    void presetsAreInRange()
    {
        const EffectSpec *spec = findEffect(QStringLiteral("frame"));
        QVERIFY(!spec->presets.empty());
        const QImage src = solid(120, 90, 90, 90, 90);
        for (const EffectPreset &p : spec->presets) {
            EffectValues v = applyEffectPreset(*spec, p, defaultEffectValues(*spec));
            const QImage out = frame(src, v);
            QVERIFY2(!out.isNull() && out.width() >= src.width(), qPrintable(p.name));
        }
    }
};

QTEST_MAIN(TestFrame)
#include "test_frame.moc"
