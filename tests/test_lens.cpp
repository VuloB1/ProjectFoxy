#include <QtTest>
#include "edit/Effects.h"
#include "lens/LensDatabase.h"

#include <cmath>

using namespace core::lens;
using core::edit::applyEffect;
using core::edit::EffectValues;

namespace {

const char *kSample = R"(<?xml version="1.0"?>
<lensdatabase version="2">
  <mount><name>Test A</name><compat>Test B</compat></mount>
  <camera>
    <maker>Foo Corp</maker><maker lang="en">Foo</maker>
    <model>Foo Cam 1</model><model lang="en">Cam 1</model>
    <mount>Test A</mount><cropfactor>1.5</cropfactor>
  </camera>
  <lens>
    <maker>Foo</maker><model>Foo 18-55mm f/3.5-5.6</model><mount>Test B</mount>
    <focal min="18" max="55"/><aperture min="3.5" max="5.6"/>
    <cropfactor>1.5</cropfactor>
    <calibration>
      <distortion model="ptlens" focal="18" a="-0.02" b="0.04" c="0"/>
      <distortion model="ptlens" focal="35" a="-0.01" b="0.02" c="0"/>
      <distortion model="ptlens" focal="55" a="0.0" b="0.0" c="0.01"/>
      <tca model="poly3" focal="18" vr="1.0004" vb="0.9996" cr="0" cb="0" br="0" bb="0"/>
      <tca model="poly3" focal="55" vr="1.0008" vb="0.9992" cr="0" cb="0" br="0" bb="0"/>
      <vignetting model="pa" focal="18" aperture="3.5" distance="1000" k1="-0.5" k2="0.1" k3="0"/>
      <vignetting model="pa" focal="18" aperture="5.6" distance="1000" k1="-0.2" k2="0.05" k3="0"/>
    </calibration>
  </lens>
</lensdatabase>)";

QImage grey(int w, int h, int v)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(v, v, v));
    return img;
}

// The radius (from the middle of the row) of the first pixel of `channel` above 128 scanning right along the middle row.
double ringRadius(const QImage &img, int channel)
{
    const int y = img.height() / 2;
    const double cx = img.width() * 0.5;
    for (int x = int(cx) + 2; x < img.width(); ++x)
        if (img.constScanLine(y)[x * 4 + channel] > 128)
            return x + 0.5 - cx;
    return -1;
}

} // namespace

class TestLens : public QObject {
    Q_OBJECT

private slots:
    void parsesCamerasLensesAndCalibrations()
    {
        LensDatabase db;
        QVERIFY(db.loadXml(QByteArray(kSample)));
        QCOMPARE(int(db.cameras().size()), 1);
        QCOMPARE(int(db.lenses().size()), 1);
        const Camera &c = db.cameras().front();
        QCOMPARE(c.makerName, QStringLiteral("Foo"));
        QCOMPARE(c.modelName, QStringLiteral("Cam 1"));
        QCOMPARE(c.crop, 1.5);
        const Lens &l = db.lenses().front();
        QVERIFY(l.hasDistortion() && l.hasTca() && l.hasVignetting());
        QCOMPARE(l.focalMin, 18.0);
        QCOMPARE(l.focalMax, 55.0);
        QCOMPARE(db.cameraMakers(), QStringList{QStringLiteral("Foo")});
        QCOMPARE(db.cameraModels(QStringLiteral("Foo")), QStringList{QStringLiteral("Cam 1")});
        QVERIFY(db.findCamera(QStringLiteral("Foo"), QStringLiteral("Cam 1")));
        // a mount that is listed as compatible takes the lens
        QVERIFY(db.mountsFit(QStringLiteral("Test A"), QStringLiteral("Test B")));
        QCOMPARE(db.lensesFor(QStringLiteral("Test A")).size(), size_t(1));
        QCOMPARE(db.lensesFor(QStringLiteral("Test A"), QStringLiteral("18 55")).size(), size_t(1));
        QCOMPARE(db.lensesFor(QStringLiteral("Test A"), QStringLiteral("70-200")).size(), size_t(0));
        QVERIFY(!db.loadXml(QByteArray("<nothing/>")));
    }

    void interpolatesBetweenFocalLengthsLikeLensfun()
    {
        LensDatabase db;
        QVERIFY(db.loadXml(QByteArray(kSample)));
        const Lens &l = db.lenses().front();
        // exactly at a calibration
        Correction c = db.correction(l, 1.5, 18.0, 5.6, 1000.0);
        QVERIFY(c.hasDistortion && c.distModel == kDistPtLens);
        QCOMPARE(c.d1, -0.02);
        QCOMPARE(c.d2, 0.04);
        // between 18 and 35: the terms follow a 1/f law, so they lie between the two but not at the midpoint of the values
        c = db.correction(l, 1.5, 24.0, 5.6, 1000.0);
        QVERIFY(c.d1 > -0.02 && c.d1 < -0.01);
        // outside the calibrated range the nearest calibration is used
        c = db.correction(l, 1.5, 100.0, 5.6, 1000.0);
        QCOMPARE(c.d3, 0.01);
        // TCA between the two entries
        c = db.correction(l, 1.5, 36.0, 5.6, 1000.0);
        QVERIFY(c.hasTca && c.vr > 1.0004 && c.vr < 1.0008);
        // vignetting follows aperture: the narrower aperture has the milder terms
        const Correction wide = db.correction(l, 1.5, 18.0, 3.5, 1000.0);
        const Correction narrow = db.correction(l, 1.5, 18.0, 5.6, 1000.0);
        const Correction mid = db.correction(l, 1.5, 18.0, 4.5, 1000.0);
        QVERIFY(wide.hasVignetting && narrow.hasVignetting && mid.hasVignetting);
        QCOMPARE(wide.k1, -0.5);
        QCOMPARE(narrow.k1, -0.2);
        QVERIFY(mid.k1 < -0.2 && mid.k1 > -0.5);
        // a camera with a bigger sensor than the calibration one: the radii are scaled
        c = db.correction(l, 1.0, 18.0, 5.6, 1000.0);
        QVERIFY(std::abs(c.scaleDist - 1.5) < 1e-9);
    }

    void theShippedDatabaseLoadsAndKnowsCommonGear()
    {
        LensDatabase db;
        QVERIFY(db.loadDirectory(QStringLiteral(LENSFUN_DIR)) > 40);
        QVERIFY2(db.cameras().size() > 500, qPrintable(QString::number(db.cameras().size())));
        QVERIFY2(db.lenses().size() > 800, qPrintable(QString::number(db.lenses().size())));
        // cameras from their EXIF names
        const Camera *d40 = db.guessCamera(QStringLiteral("NIKON CORPORATION"), QStringLiteral("NIKON D40"));
        QVERIFY(d40);
        QCOMPARE(d40->modelName, QStringLiteral("D40"));
        QVERIFY(std::abs(d40->crop - 1.5) < 0.1);
        const Camera *eos = db.guessCamera(QStringLiteral("Canon"), QStringLiteral("Canon EOS 5D Mark III"));
        QVERIFY(eos);
        QVERIFY(std::abs(eos->crop - 1.0) < 0.05);
        QVERIFY(db.guessCamera(QStringLiteral("SONY"), QStringLiteral("ILCE-7M3")));
        QVERIFY(!db.guessCamera(QStringLiteral("Nobody"), QStringLiteral("Not a camera 9000")));
        // lenses for a mount, and a text search over them
        const std::vector<int> nikon = db.lensesFor(d40->mount);
        QVERIFY(nikon.size() > 100);
        const std::vector<int> found = db.lensesFor(d40->mount, QStringLiteral("18-55 nikkor"));
        QVERIFY(!found.empty() && found.size() < nikon.size());
        // a lens that exists with all three kinds of data gives a full correction
        const std::vector<int> canon = db.lensesFor(eos->mount, QStringLiteral("24-70 f/2.8"));
        QVERIFY(!canon.empty());
        bool any = false;
        for (int i : canon) {
            const Correction c = db.correction(db.lenses()[size_t(i)], eos->crop, 35.0, 4.0, 1000.0);
            any |= c.hasDistortion && c.hasVignetting;
        }
        QVERIFY(any);
        // and the EXIF text of a lens finds it
        const int guess = db.guessLens(QStringLiteral("EF24-70mm f/2.8L USM"), eos->mount, 35.0);
        QVERIFY(guess >= 0);
        QVERIFY(db.lenses()[size_t(guess)].modelName.contains(QStringLiteral("24-70")));
    }

    // --- the correction itself --------------------------------------------------------------------

    static EffectValues values(double model, double a, double b, double c)
    {
        EffectValues v{};
        v[0] = model; v[1] = a; v[2] = b; v[3] = c;
        v[4] = 1; v[5] = 1;       // no chromatic aberration
        v[13] = 1.0;              // kr: r = 1 at half the diagonal
        v[14] = 0;                // no auto-scale
        v[15] = 100;
        return v;
    }

    void distortionMovesARingAsTheModelSays()
    {
        // A white ring at radius 120 px on a 400x300 picture (half diagonal 250): with kr = 1 its radius is 0.48 Hugin units.
        QImage ring(400, 300, QImage::Format_RGBA8888);
        ring.fill(Qt::black);
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 400; ++x) {
                const double r = std::hypot(x + 0.5 - 200.0, y + 0.5 - 150.0);
                if (std::abs(r - 120.0) < 1.2)
                    ring.setPixelColor(x, y, Qt::white);
            }
        const EffectValues v = values(1, -0.2, 0, 0); // poly3, k1 = -0.2
        const QImage out = applyEffect(ring, QStringLiteral("lens"), v);
        // the output radius ru satisfies ru * (1 + k' ru^2) = Rs with k' = k / (1 - k)^3
        const double rs = 120.0 / 250.0;
        const double kp = -0.2 / std::pow(1.2, 3.0);
        double ru = rs;
        for (int i = 0; i < 40; ++i)
            ru = rs / (1.0 + kp * ru * ru);
        const double measured = ringRadius(out, 0);
        QVERIFY2(measured > 0, "the ring disappeared");
        QVERIFY2(std::abs(measured - ru * 250.0) < 2.0, qPrintable(QString("%1 vs %2").arg(measured).arg(ru * 250.0)));
        QVERIFY(measured > 120.0); // barrel correction stretches the picture outward
        // zero terms: nothing changes
        QCOMPARE(applyEffect(ring, QStringLiteral("lens"), values(0, 0, 0, 0)), ring);
        QCOMPARE(applyEffect(ring, QStringLiteral("lens"), values(1, 0, 0, 0)), ring);
        // strength 0 too
        EffectValues off = v;
        off[15] = 0;
        QCOMPARE(applyEffect(ring, QStringLiteral("lens"), off), ring);
    }

    void chromaticAberrationMovesRedAndBlueOnly()
    {
        QImage ring(400, 300, QImage::Format_RGBA8888);
        ring.fill(Qt::black);
        for (int y = 0; y < 300; ++y)
            for (int x = 0; x < 400; ++x)
                if (std::abs(std::hypot(x + 0.5 - 200.0, y + 0.5 - 150.0) - 140.0) < 1.5)
                    ring.setPixelColor(x, y, Qt::white);
        EffectValues v = values(0, 0, 0, 0);
        v[4] = 1.02; // red is sampled 2% further out: its ring is drawn 2% closer in
        v[5] = 0.98;
        const QImage out = applyEffect(ring, QStringLiteral("lens"), v);
        const double red = ringRadius(out, 0), green = ringRadius(out, 1), blue = ringRadius(out, 2);
        QVERIFY(red > 0 && green > 0 && blue > 0);
        QVERIFY2(std::abs(green - 140.0) < 2.5, qPrintable(QString::number(green)));
        QVERIFY2(std::abs(red - 140.0 / 1.02) < 2.5, qPrintable(QString::number(red)));
        QVERIFY2(std::abs(blue - 140.0 / 0.98) < 2.5, qPrintable(QString::number(blue)));
    }

    void vignettingBrightensTheCorners()
    {
        EffectValues v = values(0, 0, 0, 0);
        v[10] = -0.2; // c = 1 - 0.2 r^2: the corner (r = 1) gets 1 / 0.8
        const QImage out = applyEffect(grey(400, 300, 120), QStringLiteral("lens"), v);
        QVERIFY(qAbs(qRed(out.pixel(200, 150)) - 120) <= 1);               // the middle is untouched
        QVERIFY2(qAbs(qRed(out.pixel(1, 1)) - 150) <= 4, qPrintable(QString::number(qRed(out.pixel(1, 1)))));
        QVERIFY(qRed(out.pixel(100, 75)) > 120 && qRed(out.pixel(100, 75)) < qRed(out.pixel(1, 1)));
    }

    void autoScaleRemovesTheEmptyEdges()
    {
        const QImage src = grey(400, 300, 200);
        EffectValues v = values(1, 0.15, 0, 0); // a pincushion correction pulls in pixels from outside the picture
        const QImage plain = applyEffect(src, QStringLiteral("lens"), v);
        QVERIFY(qAlpha(plain.pixel(1, 1)) < 128);   // the corner is empty
        v[14] = 1;
        const QImage scaled = applyEffect(src, QStringLiteral("lens"), v);
        for (int y : {0, 1, 150, 298, 299})
            for (int x : {0, 1, 200, 398, 399})
                QVERIFY2(qAlpha(scaled.pixel(x, y)) > 250, qPrintable(QString("%1,%2").arg(x).arg(y)));
    }

    void manualSlidersAreNeutralAtZeroAndSignedLikeTheirNames()
    {
        const auto zero = manualValues(0, 0, 0, true);
        QCOMPARE(zero[0], 0.0);
        QCOMPARE(zero[1], 0.0);
        QCOMPARE(zero[10], 0.0);
        QCOMPARE(zero[4], 1.0);
        QCOMPARE(zero[5], 1.0);
        QCOMPARE(zero[14], 0.0); // nothing to auto-scale without distortion
        // +100 straightens a barrel: k1 < 0 stretches outward, which leaves the middle of the edges empty -> crop
        const auto barrel = manualValues(100, 0, 0, true);
        QVERIFY(barrel[0] == 1.0 && barrel[1] < 0);
        QCOMPARE(barrel[14], 1.0);
        // -100 straightens a pincushion: k1 > 0 pulls pixels from outside the frame, so the empty corners get cropped
        const auto pin = manualValues(-100, 0, 0, true);
        QVERIFY(pin[1] > 0);
        QCOMPARE(pin[14], 1.0);
        QCOMPARE(manualValues(-100, 0, 0, false)[14], 0.0); // unless the option is off
        // and the crop really leaves a full picture for both signs
        for (double d : {-100.0, 100.0}) {
            const QImage out = applyEffect(grey(400, 300, 200), QStringLiteral("lens"), manualValues(d, 0, 0, true));
            for (int y : {0, 1, 150, 298, 299})
                for (int x : {0, 1, 200, 398, 399})
                    QVERIFY2(qAlpha(out.pixel(x, y)) > 250, qPrintable(QString("d=%1 at %2,%3").arg(d).arg(x).arg(y)));
        }
        // fringes: red and blue are scaled in opposite directions, the same amount
        const auto fr = manualValues(0, 100, 0, false);
        QVERIFY(fr[4] > 1.0 && fr[5] < 1.0 && std::abs((fr[4] - 1.0) + (fr[5] - 1.0)) < 1e-12);
        // vignette +100 brightens the corners (negative k1 -> gain above 1 at the corner), -100 darkens them
        QVERIFY(manualValues(0, 0, 100, false)[10] < 0);
        QVERIFY(manualValues(0, 0, -100, false)[10] > 0);
        // a full-strength manual correction leaves the middle of a picture alone and fixes the corners of a flat grey
        const QImage out = applyEffect(grey(400, 300, 120), QStringLiteral("lens"), manualValues(0, 0, 100, false));
        QVERIFY(qAbs(qRed(out.pixel(200, 150)) - 120) <= 1);
        QVERIFY(qRed(out.pixel(1, 1)) > 140);
    }

    void profileValuesOnlyCarryTheKindsAskedFor()
    {
        LensDatabase db;
        QVERIFY(db.loadXml(QByteArray(kSample)));
        const Camera *cam = db.findCamera(QStringLiteral("Foo"), QStringLiteral("Cam 1"));
        QVERIFY(cam);
        const Correction c = db.correction(db.lenses()[0], cam->crop, 18.0, 3.5, 1000.0);
        QVERIFY(c.hasDistortion && c.hasTca && c.hasVignetting);
        const auto all = correctionValues(c, true, true, true, true, 100);
        QVERIFY(all[0] != 0 && all[4] != 1.0 && all[10] != 0.0 && all[14] == 1.0 && all[15] == 100.0);
        const auto onlyDist = correctionValues(c, true, false, false, false, 70);
        QVERIFY(onlyDist[0] != 0);
        QCOMPARE(onlyDist[4], 1.0);
        QCOMPARE(onlyDist[5], 1.0);
        QCOMPARE(onlyDist[10], 0.0);
        QCOMPARE(onlyDist[14], 0.0);
        QCOMPARE(onlyDist[15], 70.0);
        const auto none = correctionValues(c, false, false, false, true, 100);
        QCOMPARE(none[0], 0.0);
        QCOMPARE(none[14], 0.0); // no distortion to correct -> no auto-scale either
        QVERIFY(none[13] > 0);   // the unit is always there
    }
};

QTEST_MAIN(TestLens)
#include "test_lens.moc"
