#include <vips/vips8> // before any Qt header: GLib has a member called `signals`
#include <libheif/heif_cxx.h>
#include <QtTest>
#include <QColorSpace>
#include <QFile>
#include <QTemporaryDir>
#include <mutex>
#include "DecoderRegistry.h"
#include "decoders/VipsGuard.h"
#include "color_reference.h"

namespace {

const QString kFixtures = QStringLiteral(FIXTURES_DIR);

// The eight exif_orient_N.jpg fixtures store their pixels rotated/mirrored and
// carry the EXIF Orientation tag N that undoes it. Whatever N, a viewer must end
// up showing the same upright 48x32 picture: top-left red, top-right green,
// bottom-left blue, bottom-right yellow, a black dot near the top-left corner.
bool near(QRgb p, int r, int g, int b, int tol = 60)
{
    return qAbs(qRed(p) - r) <= tol && qAbs(qGreen(p) - g) <= tol && qAbs(qBlue(p) - b) <= tol;
}

QString describe(const QImage &img)
{
    return QStringLiteral("%1x%2 TL=%3 TR=%4 BL=%5 BR=%6")
        .arg(img.width()).arg(img.height())
        .arg(img.width() > 12 ? QString::number(img.pixel(12, 8), 16) : QString())
        .arg(img.width() > 36 ? QString::number(img.pixel(36, 8), 16) : QString())
        .arg(img.height() > 24 ? QString::number(img.pixel(12, 24), 16) : QString())
        .arg(img.height() > 24 && img.width() > 36 ? QString::number(img.pixel(36, 24), 16) : QString());
}

bool isUpright(const QImage &img)
{
    if (img.size() != QSize(48, 32))
        return false;
    return near(img.pixel(12, 8), 220, 30, 30) && near(img.pixel(36, 8), 30, 200, 30)
        && near(img.pixel(12, 24), 30, 30, 220) && near(img.pixel(36, 24), 230, 220, 30)
        && qGray(img.pixel(3, 3)) < 90;
}

// A small picture straight from libvips: `bands` channels of `type`, every pixel holding
// `values` (one per band), tagged with `interpretation`, written to `path` by its extension.
void writeWithVips(const QString &path, VipsBandFormat type, VipsInterpretation interpretation, int bands,
                   const QList<int> &values, const QByteArray &icc = QByteArray())
{
    core::ensureVipsInitialized();
    std::lock_guard<std::mutex> lock(core::vipsEntryMutex());
    constexpr int W = 16, H = 16;
    const bool wide = type == VIPS_FORMAT_USHORT;
    QByteArray pixels(W * H * bands * (wide ? 2 : 1), 0);
    for (int i = 0; i < W * H; ++i) {
        for (int b = 0; b < bands; ++b) {
            if (wide)
                reinterpret_cast<quint16 *>(pixels.data())[i * bands + b] = quint16(values.at(b));
            else
                reinterpret_cast<quint8 *>(pixels.data())[i * bands + b] = quint8(values.at(b));
        }
    }
    vips::VImage img = vips::VImage::new_from_memory(pixels.data(), size_t(pixels.size()), W, H, bands, type);
    img = img.copy(vips::VImage::option()->set("interpretation", interpretation));
    if (!icc.isEmpty())
        vips_image_set_blob_copy(img.get_image(), VIPS_META_ICC_NAME, icc.constData(), size_t(icc.size()));
    img.write_to_file(path.toUtf8().constData());
}

} // namespace

class TestDecoderRegistry : public QObject {
    Q_OBJECT
private slots:
    void picksVipsForJpeg()
    {
        auto decoder = core::DecoderRegistry::instance().decoderFor("photo.jpg");
        QVERIFY(decoder != nullptr);
    }

    void picksRawForCr2()
    {
        auto decoder = core::DecoderRegistry::instance().decoderFor("photo.cr2");
        QVERIFY(decoder != nullptr);
    }

    void returnsNullForUnknownExtension()
    {
        auto decoder = core::DecoderRegistry::instance().decoderFor("document.pdf");
        QVERIFY(decoder == nullptr);
    }

    // Every format the viewer says it opens must really open. (An extension in the
    // registry's list that cannot be decoded shows up in the file dialog and the
    // folder strip and then fails with an error.)
    void everyClaimedStillFormatOpens_data()
    {
        QTest::addColumn<QString>("ext");
        for (const char *ext : {"png", "jpg", "bmp", "tif", "webp", "gif", "ico", "heic", "avif"})
            QTest::newRow(ext) << QString::fromLatin1(ext);
    }
    void everyClaimedStillFormatOpens()
    {
        QFETCH(QString, ext);
        const QString path = kFixtures + QStringLiteral("/format_sample.") + ext;
        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY2(decoder, "no decoder claims this extension");
        const core::DecodeResult full = decoder->decode(path);
        QVERIFY2(full.ok, qPrintable(full.error));
        QVERIFY(full.image.width() >= 16 && full.image.height() >= 16);
        QVERIFY(!full.image.isNull());
        // the reduced-size path used by thumbnails
        const core::DecodeResult preview = decoder->decode(path, QSize(24, 24));
        QVERIFY2(preview.ok, qPrintable(preview.error));
    }

    void doesNotAdvertiseFormatsItCannotOpen()
    {
        const QStringList claimed = core::DecoderRegistry::instance().allSupportedExtensions();
        // libvips in this build has no ImageMagick / libjxl: these never opened.
        for (const char *ext : {"psd", "tga", "jxl"})
            QVERIFY2(!claimed.contains(QLatin1String(ext)), ext);
    }

    // On Windows a file that is still open cannot be deleted, renamed or replaced.
    // After decoding, the viewer must not keep the file open (the user can delete it
    // from Explorer, and "Guardar" replaces the original).
    void decodingDoesNotLockTheFile_data()
    {
        QTest::addColumn<QString>("ext");
        for (const char *ext : {"png", "jpg", "bmp", "tif", "webp", "gif", "ico", "heic", "avif"})
            QTest::newRow(ext) << QString::fromLatin1(ext);
    }
    void decodingDoesNotLockTheFile()
    {
        QFETCH(QString, ext);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("locked.") + ext);
        QVERIFY(QFile::copy(kFixtures + QStringLiteral("/format_sample.") + ext, path));
        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY(decoder);
        QVERIFY(decoder->decode(path).ok);
        QVERIFY(decoder->decode(path, QSize(24, 24)).ok);
        QVERIFY2(QFile::remove(path), "the file is still held open after decoding");
    }

    // Photos from phones are stored sideways with an EXIF Orientation tag. The
    // viewer must show them upright, whether it decodes the full picture or a
    // reduced preview (thumbnails, quick first paint).
    void honorsExifOrientation_data()
    {
        QTest::addColumn<int>("orientation");
        for (int o = 1; o <= 8; ++o)
            QTest::newRow(qPrintable(QStringLiteral("orientation %1").arg(o))) << o;
    }
    void honorsExifOrientation()
    {
        QFETCH(int, orientation);
        const QString path = kFixtures + QStringLiteral("/exif_orient_%1.jpg").arg(orientation);
        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY(decoder);

        const core::DecodeResult full = decoder->decode(path);
        QVERIFY2(full.ok, qPrintable(full.error));
        QVERIFY2(isUpright(full.image), qPrintable("full decode: " + describe(full.image)));
        QCOMPARE(full.sourceSize, QSize(48, 32)); // the size the user sees, not the stored one

        const core::DecodeResult preview = decoder->decode(path, QSize(64, 64));
        QVERIFY2(preview.ok, qPrintable(preview.error));
        QVERIFY2(isUpright(preview.image), qPrintable("preview decode: " + describe(preview.image)));
    }

    // Windows file names are UTF-16: characters outside the ANSI code page
    // (Japanese, Cyrillic, emoji...) must open like any other file.
    // ---- colour models: every one must land as plain 8-bit sRGB RGBA ------------

    // Photo editors export 16 bits per channel PNG/TIFF routinely. Squeezing the numbers into
    // 8 bits without scaling them turns almost everything white.
    void sixteenBitPicturesAreScaledNotClipped_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("png") << "rgb16.png";
        QTest::newRow("tiff") << "rgb16.tif";
    }
    void sixteenBitPicturesAreScaledNotClipped()
    {
        QFETCH(QString, name);
        QTemporaryDir dir;
        const QString path = dir.filePath(name);
        writeWithVips(path, VIPS_FORMAT_USHORT, VIPS_INTERPRETATION_RGB16, 3, {50 * 257, 128 * 257, 200 * 257});

        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY(decoder);
        const core::DecodeResult r = decoder->decode(path);
        QVERIFY2(r.ok, qPrintable(r.error));
        const QRgb p = r.image.pixel(8, 8);
        QVERIFY2(near(p, 50, 128, 200, 3), qPrintable(QString::number(p, 16)));
        QCOMPARE(qAlpha(p), 255);
    }

    void sixteenBitGreyIsScaledToo()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("grey16.png"));
        writeWithVips(path, VIPS_FORMAT_USHORT, VIPS_INTERPRETATION_GREY16, 1, {128 * 257});
        const core::DecodeResult r = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY2(near(r.image.pixel(8, 8), 128, 128, 128, 3), qPrintable(QString::number(r.image.pixel(8, 8), 16)));
    }

    // Grey + alpha is two bands: the second one is the transparency, not a colour.
    void greyWithAlphaKeepsItsAlpha()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("greya.png"));
        writeWithVips(path, VIPS_FORMAT_UCHAR, VIPS_INTERPRETATION_B_W, 2, {100, 128});
        const core::DecodeResult r = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(r.ok, qPrintable(r.error));
        const QRgb p = r.image.pixel(8, 8);
        QVERIFY2(near(p, 100, 100, 100, 2), qPrintable(QString::number(p, 16)));
        QCOMPARE(qAlpha(p), 128);
    }

    // A CMYK JPEG has four channels, none of which is alpha: reading them as R, G, B, A
    // gives wrong colours and (the K channel being "alpha") holes in the picture.
    void cmykJpegBecomesRgb()
    {
        const QString path = kFixtures + QStringLiteral("/cmyk_sample.jpg"); // red | cyan | black | white
        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY(decoder);
        for (const QSize hint : {QSize(), QSize(32, 32)}) { // the full decode and the preview one
            const core::DecodeResult r = decoder->decode(path, hint);
            QVERIFY2(r.ok, qPrintable(r.error));
            const int w = r.image.width(), h = r.image.height();
            const QRgb red = r.image.pixel(w / 8, h / 2), cyan = r.image.pixel(w * 3 / 8, h / 2);
            const QRgb black = r.image.pixel(w * 5 / 8, h / 2), white = r.image.pixel(w * 7 / 8, h / 2);
            const QString seen = QStringLiteral("red=%1 cyan=%2 black=%3 white=%4")
                                     .arg(red, 0, 16).arg(cyan, 0, 16).arg(black, 0, 16).arg(white, 0, 16);
            QVERIFY2(near(red, 255, 0, 0, 45), qPrintable(seen));
            QVERIFY2(near(cyan, 0, 255, 255, 45), qPrintable(seen));
            QVERIFY2(near(black, 0, 0, 0, 45), qPrintable(seen));
            QVERIFY2(near(white, 255, 255, 255, 45), qPrintable(seen));
            for (const QRgb p : {red, cyan, black, white})
                QCOMPARE(qAlpha(p), 255); // no K-channel holes
        }
    }

    // ---- colour profiles: everything must land in sRGB --------------------------------

    // A Display P3 photo (every recent iPhone) shown with its numbers read as sRGB looks
    // washed out. Each format that can carry a profile, through the real decoder.
    void profiledPicturesAreConvertedToSrgb_data()
    {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<int>("tolerance");
        QTest::newRow("png 8-bit") << "png8" << 3;
        QTest::newRow("jpeg") << "jpeg" << 6;
        QTest::newRow("tiff 8-bit") << "tiff8" << 3;
        QTest::newRow("png 16-bit") << "png16" << 3;
    }
    void profiledPicturesAreConvertedToSrgb()
    {
        QFETCH(QString, kind);
        QFETCH(int, tolerance);
        QTemporaryDir dir;
        const QByteArray p3 = QColorSpace(QColorSpace::DisplayP3).iccProfile();
        const int r = 200, g = 120, b = 60;
        QString path;
        if (kind == QLatin1String("png8") || kind == QLatin1String("jpeg")) {
            path = dir.filePath(kind == QLatin1String("png8") ? "p3.png" : "p3.jpg");
            QImage img(16, 16, QImage::Format_RGBA8888);
            img.fill(QColor(r, g, b));
            img.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
            QVERIFY(img.save(path, nullptr, 100));
        } else if (kind == QLatin1String("tiff8")) {
            path = dir.filePath("p3.tif");
            writeWithVips(path, VIPS_FORMAT_UCHAR, VIPS_INTERPRETATION_sRGB, 3, {r, g, b}, p3);
        } else {
            path = dir.filePath("p3_16.png");
            writeWithVips(path, VIPS_FORMAT_USHORT, VIPS_INTERPRETATION_RGB16, 3, {r * 257, g * 257, b * 257}, p3);
        }

        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        QVERIFY(res.iccProfile.isEmpty()); // converted: the pixels are sRGB now, no profile to keep

        const colorref::Vec3 want = colorref::referenceToSrgb(colorref::kDisplayP3ToXyz, colorref::srgbToLinear, r, g, b);
        const QRgb got = res.image.pixel(8, 8);
        const QString msg = QStringLiteral("got %1,%2,%3 want %4,%5,%6")
                                .arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                                .arg(want[0], 0, 'f', 1).arg(want[1], 0, 'f', 1).arg(want[2], 0, 'f', 1);
        QVERIFY2(std::abs(qRed(got) - want[0]) <= tolerance && std::abs(qGreen(got) - want[1]) <= tolerance
                     && std::abs(qBlue(got) - want[2]) <= tolerance, qPrintable(msg));
        QCOMPARE(qAlpha(got), 255);
    }

    // The preview decode (filmstrip thumbnails, quick first paint) takes another path
    // through libvips: it must be converted too, or thumbnails would not match the photo.
    void thePreviewDecodeIsConvertedToo()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("p3.png");
        QImage img(64, 64, QImage::Format_RGBA8888);
        img.fill(QColor(200, 120, 60));
        img.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
        QVERIFY(img.save(path));

        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        const core::DecodeResult full = decoder->decode(path);
        const core::DecodeResult preview = decoder->decode(path, QSize(16, 16));
        QVERIFY(full.ok && preview.ok);
        QVERIFY(full.image.pixel(8, 8) != qRgb(200, 120, 60));
        const QRgb a = full.image.pixel(8, 8), b = preview.image.pixel(4, 4);
        QVERIFY2(std::abs(qRed(a) - qRed(b)) <= 3 && std::abs(qGreen(a) - qGreen(b)) <= 3 && std::abs(qBlue(a) - qBlue(b)) <= 3,
                 qPrintable(QStringLiteral("full %1 preview %2").arg(a, 0, 16).arg(b, 0, 16)));
    }

    // A file already in sRGB keeps its numbers.
    void srgbTaggedPicturesKeepTheirNumbers()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("srgb.png");
        QImage img(16, 16, QImage::Format_RGBA8888);
        img.fill(QColor(200, 120, 60));
        img.setColorSpace(QColorSpace(QColorSpace::SRgb));
        QVERIFY(img.save(path));
        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        QVERIFY(near(res.image.pixel(8, 8), 200, 120, 60, 1));
        QVERIFY(res.iccProfile.isEmpty());
    }

    // iPhone photos are HEIC in Display P3. The profile travels as an ICC blob, or (AVIF
    // mostly) as nclx colour primaries. Solid (200,120,60) in P3, lossy-coded.
    void heifPicturesInDisplayP3AreConvertedToSrgb_data()
    {
        QTest::addColumn<QString>("fixture");
        QTest::newRow("heic with an ICC profile") << "p3_icc.heic";
        QTest::newRow("avif with an ICC profile") << "p3_icc.avif";
    }
    void heifPicturesInDisplayP3AreConvertedToSrgb()
    {
        QFETCH(QString, fixture);
        const QString path = kFixtures + QLatin1Char('/') + fixture;
        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        const colorref::Vec3 want = colorref::referenceToSrgb(colorref::kDisplayP3ToXyz, colorref::srgbToLinear, 200, 120, 60);
        const QRgb got = res.image.pixel(16, 16);
        const QString msg = QStringLiteral("got %1,%2,%3 want %4,%5,%6")
                                .arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                                .arg(want[0], 0, 'f', 1).arg(want[1], 0, 'f', 1).arg(want[2], 0, 'f', 1);
        // (lossy codec and a YCbCr round trip: a few levels of room)
        QVERIFY2(std::abs(qRed(got) - want[0]) <= 8 && std::abs(qGreen(got) - want[1]) <= 8
                     && std::abs(qBlue(got) - want[2]) <= 8, qPrintable(msg));
        QVERIFY(res.iccProfile.isEmpty());
    }

    // The other way a phone tags colour: nclx primaries, no ICC blob. Built here with
    // libheif itself (no tool writes it for us) - a solid (200,120,60) AVIF whose primaries
    // say "Display P3".
    void avifWithNclxP3PrimariesIsConvertedToSrgb()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("p3_nclx.avif");
        try {
            heif::Image image;
            image.create(32, 32, heif_colorspace_RGB, heif_chroma_interleaved_RGB);
            image.add_plane(heif_channel_interleaved, 32, 32, 8);
            int stride = 0;
            uint8_t *plane = image.get_plane(heif_channel_interleaved, &stride);
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x) {
                    plane[y * stride + x * 3] = 200;
                    plane[y * stride + x * 3 + 1] = 120;
                    plane[y * stride + x * 3 + 2] = 60;
                }
            heif::ColorProfile_nclx nclx;
            nclx.set_color_primaries(heif_color_primaries_SMPTE_EG_432_1);
            nclx.set_transfer_characteristics(heif_transfer_characteristic_IEC_61966_2_1);
            nclx.set_matrix_coefficients(heif_matrix_coefficients_ITU_R_BT_601_6);
            nclx.set_full_range_flag(true);
            image.set_nclx_color_profile(nclx);

            heif::Context ctx;
            heif::Encoder encoder(heif_compression_AV1);
            encoder.set_lossy_quality(95);
            ctx.encode_image(image, encoder);
            ctx.write_to_file(path.toStdString());
        } catch (const heif::Error &e) {
            QSKIP(qPrintable(QStringLiteral("this libheif cannot write AVIF: %1").arg(QString::fromStdString(e.get_message()))));
        }

        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        const colorref::Vec3 want = colorref::referenceToSrgb(colorref::kDisplayP3ToXyz, colorref::srgbToLinear, 200, 120, 60);
        const QRgb got = res.image.pixel(16, 16);
        const QString msg = QStringLiteral("got %1,%2,%3 want %4,%5,%6")
                                .arg(qRed(got)).arg(qGreen(got)).arg(qBlue(got))
                                .arg(want[0], 0, 'f', 1).arg(want[1], 0, 'f', 1).arg(want[2], 0, 'f', 1);
        QVERIFY2(std::abs(qRed(got) - want[0]) <= 8 && std::abs(qGreen(got) - want[1]) <= 8
                     && std::abs(qBlue(got) - want[2]) <= 8, qPrintable(msg));
    }

    // An RGB profile Qt cannot use (here a damaged one): the pixels are NOT touched and
    // the profile is handed on, so saving can attach it again.
    void anUnusableRgbProfileIsKeptForSaving()
    {
        const QByteArray odd = QColorSpace(QColorSpace::DisplayP3).iccProfile().left(300); // truncated
        QTemporaryDir dir;
        const QString path = dir.filePath("odd.tif");
        writeWithVips(path, VIPS_FORMAT_UCHAR, VIPS_INTERPRETATION_sRGB, 3, {200, 120, 60}, odd);

        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        QVERIFY(near(res.image.pixel(8, 8), 200, 120, 60, 1)); // untouched
        QCOMPARE(res.iccProfile, odd);
    }

    // A print (CMYK) profile does not describe RGB pixels: not applied, not kept.
    void aNonRgbProfileIsDropped()
    {
        QFile f(QStringLiteral("C:/Windows/System32/spool/drivers/color/RSWOP.icm"));
        if (!f.open(QIODevice::ReadOnly))
            QSKIP("this machine has no RSWOP.icm");
        QTemporaryDir dir;
        const QString path = dir.filePath("print.tif");
        writeWithVips(path, VIPS_FORMAT_UCHAR, VIPS_INTERPRETATION_sRGB, 3, {200, 120, 60}, f.readAll());
        const core::DecodeResult res = core::DecoderRegistry::instance().decoderFor(path)->decode(path);
        QVERIFY2(res.ok, qPrintable(res.error));
        QVERIFY(near(res.image.pixel(8, 8), 200, 120, 60, 1));
        QVERIFY(res.iccProfile.isEmpty());
    }

    void opensFilesWithUnicodeNames_data()
    {
        QTest::addColumn<QString>("name");
        QTest::newRow("latin accents") << QStringLiteral("ñandú café.jpg");
        QTest::newRow("cyrillic") << QStringLiteral("тест.jpg");
        QTest::newRow("japanese") << QStringLiteral("日本語の写真.jpg");
        QTest::newRow("korean") << QStringLiteral("한국어.jpg");
        QTest::newRow("emoji") << QStringLiteral("foto \U0001F600 playa.jpg");
    }
    void opensFilesWithUnicodeNames()
    {
        QFETCH(QString, name);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(name);
        QVERIFY(QFile::copy(kFixtures + QStringLiteral("/exif_orient_1.jpg"), path));
        QVERIFY(QFileInfo::exists(path));

        auto decoder = core::DecoderRegistry::instance().decoderFor(path);
        QVERIFY(decoder);
        const core::DecodeResult full = decoder->decode(path);
        QVERIFY2(full.ok, qPrintable(full.error));
        QCOMPARE(full.image.size(), QSize(48, 32));
        const core::DecodeResult preview = decoder->decode(path, QSize(64, 64));
        QVERIFY2(preview.ok, qPrintable(preview.error));
    }
};

QTEST_MAIN(TestDecoderRegistry)
#include "test_decoder_registry.moc"
