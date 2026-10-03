#include "ColorManagement.h"
#include "color_reference.h"
#include <QColorSpace>
#include <QFile>
#include <QtTest>
#include <array>
#include <cmath>

namespace {

using namespace colorref;

QImage onePixel(int r, int g, int b, int a = 255)
{
    QImage img(4, 4, QImage::Format_RGBA8888);
    img.fill(QColor(r, g, b, a));
    return img;
}

QByteArray fileBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestColorManagement : public QObject {
    Q_OBJECT

private slots:
    // A Display P3 photo (every recent iPhone) shown without conversion looks washed out:
    // the numbers are the same but they mean more saturated colours.
    void displayP3MatchesTheReference_data()
    {
        QTest::addColumn<int>("r");
        QTest::addColumn<int>("g");
        QTest::addColumn<int>("b");
        QTest::newRow("orange") << 200 << 120 << 60;
        QTest::newRow("teal") << 40 << 160 << 150;
        QTest::newRow("skin") << 225 << 170 << 140;
        QTest::newRow("mid grey") << 128 << 128 << 128;
        QTest::newRow("dark blue") << 20 << 30 << 150;
    }
    void displayP3MatchesTheReference()
    {
        QFETCH(int, r);
        QFETCH(int, g);
        QFETCH(int, b);
        QImage img = onePixel(r, g, b);
        const QByteArray profile = QColorSpace(QColorSpace::DisplayP3).iccProfile();
        QVERIFY(!profile.isEmpty());

        QCOMPARE(core::convertToSrgb(img, profile), core::ColorConversion::Converted);

        const Vec3 want = referenceToSrgb(kDisplayP3ToXyz, srgbToLinear, r, g, b);
        const QRgb got = img.pixel(1, 1);
        const QString msg = QStringLiteral("got %1,%2,%3 want %4,%5,%6")
                                .arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                                .arg(want[0], 0, 'f', 1).arg(want[1], 0, 'f', 1).arg(want[2], 0, 'f', 1);
        QVERIFY2(std::abs(qRed(got) - want[0]) <= 2.0 && std::abs(qGreen(got) - want[1]) <= 2.0
                     && std::abs(qBlue(got) - want[2]) <= 2.0, qPrintable(msg));
    }

    void adobeRgbMatchesTheReference()
    {
        for (const auto &c : {std::array<int, 3>{60, 160, 200}, {200, 80, 80}, {30, 140, 60}, {128, 128, 128}}) {
            QImage img = onePixel(c[0], c[1], c[2]);
            QCOMPARE(core::convertToSrgb(img, QColorSpace(QColorSpace::AdobeRgb).iccProfile()),
                     core::ColorConversion::Converted);
            const Vec3 want = referenceToSrgb(kAdobeRgbToXyz, adobeDecode, c[0], c[1], c[2]);
            const QRgb got = img.pixel(1, 1);
            const QString msg = QStringLiteral("in %1,%2,%3: got %4,%5,%6 want %7,%8,%9")
                                    .arg(c[0]).arg(c[1]).arg(c[2]).arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                                    .arg(want[0], 0, 'f', 1).arg(want[1], 0, 'f', 1).arg(want[2], 0, 'f', 1);
            QVERIFY2(std::abs(qRed(got) - want[0]) <= 2.0 && std::abs(qGreen(got) - want[1]) <= 2.0
                         && std::abs(qBlue(got) - want[2]) <= 2.0, qPrintable(msg));
        }
    }

    // The point of the conversion, in one line: the same numbers, read as P3, are a MORE
    // saturated colour than read as sRGB - so converting must move them.
    void p3NumbersMeanADifferentColourThanSrgbNumbers()
    {
        QImage img = onePixel(40, 200, 90);
        const QRgb before = img.pixel(1, 1);
        QCOMPARE(core::convertToSrgb(img, QColorSpace(QColorSpace::DisplayP3).iccProfile()), core::ColorConversion::Converted);
        QVERIFY(img.pixel(1, 1) != before);
    }

    void srgbIsLeftAlone()
    {
        QImage img = onePixel(40, 200, 90);
        const QImage before = img;
        QCOMPARE(core::convertToSrgb(img, core::srgbProfile()), core::ColorConversion::AlreadySrgb);
        QCOMPARE(img, before);
    }

    // Windows' own sRGB profile (a different file from Qt's) is sRGB too.
    void windowsSrgbProfileIsLeftAlone()
    {
        const QByteArray profile = fileBytes(QStringLiteral("C:/Windows/System32/spool/drivers/color/sRGB Color Space Profile.icm"));
        if (profile.isEmpty())
            QSKIP("this machine has no sRGB Color Space Profile.icm");
        QImage img = onePixel(40, 200, 90);
        const QImage before = img;
        QCOMPARE(core::convertToSrgb(img, profile), core::ColorConversion::AlreadySrgb);
        QCOMPARE(img, before);
    }

    void noProfileOrAnUnusableOneChangesNothing()
    {
        QImage img = onePixel(40, 200, 90);
        const QImage before = img;
        QCOMPARE(core::convertToSrgb(img, QByteArray()), core::ColorConversion::NoProfile);
        QCOMPARE(core::convertToSrgb(img, QByteArray("this is not an ICC profile")), core::ColorConversion::Unsupported);
        QCOMPARE(img, before);
    }

    // A print (CMYK) profile does not describe RGB pixels.
    void aCmykProfileIsUnsupported()
    {
        const QByteArray profile = fileBytes(QStringLiteral("C:/Windows/System32/spool/drivers/color/RSWOP.icm"));
        if (profile.isEmpty())
            QSKIP("this machine has no RSWOP.icm");
        QImage img = onePixel(40, 200, 90);
        const QImage before = img;
        QCOMPARE(core::convertToSrgb(img, profile), core::ColorConversion::Unsupported);
        QCOMPARE(img, before);
    }

    void transparencyIsNeverTouched()
    {
        QImage img(64, 1, QImage::Format_RGBA8888);
        for (int x = 0; x < 64; ++x)
            img.setPixelColor(x, 0, QColor(40, 200, 90, x * 4));
        QCOMPARE(core::convertToSrgb(img, QColorSpace(QColorSpace::DisplayP3).iccProfile()), core::ColorConversion::Converted);
        QCOMPARE(img.format(), QImage::Format_RGBA8888); // and it is still straight alpha
        for (int x = 0; x < 64; ++x)
            QCOMPARE(qAlpha(img.pixel(x, 0)), x * 4);
    }

    // Converting must not write into a buffer another QImage still shares.
    void aSharedImageIsNotModified()
    {
        QImage original = onePixel(40, 200, 90);
        QImage copy = original;
        QCOMPARE(core::convertToSrgb(copy, QColorSpace(QColorSpace::DisplayP3).iccProfile()), core::ColorConversion::Converted);
        QCOMPARE(original.pixel(1, 1), qRgb(40, 200, 90));
        QVERIFY(copy.pixel(1, 1) != original.pixel(1, 1));
    }

    void costOfConvertingAPhoto()
    {
        QImage img(4000, 3000, QImage::Format_RGBA8888); // 12 MP of varied pixels (a uniform fill would flatter the cost)
        quint32 seed = 12345;
        for (int y = 0; y < img.height(); ++y) {
            uchar *line = img.scanLine(y);
            for (int x = 0; x < img.width() * 4; ++x) {
                seed = seed * 1664525u + 1013904223u;
                line[x] = (x % 4 == 3) ? 255 : uchar(seed >> 24);
            }
        }
        const QByteArray profile = QColorSpace(QColorSpace::DisplayP3).iccProfile();
        QElapsedTimer t;
        t.start();
        QCOMPARE(core::convertToSrgb(img, profile), core::ColorConversion::Converted);
        qInfo() << "converting 12 MP of varied pixels from Display P3 to sRGB took" << t.elapsed() << "ms";
        QVERIFY(t.elapsed() < 5000);
    }
};

QTEST_MAIN(TestColorManagement)
#include "test_color_management.moc"
