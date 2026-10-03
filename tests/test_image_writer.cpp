#include <QtTest>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <cmath>
#include "DecoderRegistry.h"
#include "ImageWriter.h"
#include "MetadataReader.h"
#include "ColorManagement.h"
#include <QColorSpace>
#include <exiv2/exiv2.hpp>

namespace {

const QString kFixtures = QStringLiteral(FIXTURES_DIR);

// Soft shapes on a gradient with a little texture - photo-like enough for a JPEG
// quality comparison, unlike flat colour or pure noise.
QImage photoLike(int w = 320, int h = 240)
{
    QImage img(w, h, QImage::Format_RGBA8888);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient grad(0, 0, w, h);
    grad.setColorAt(0.0, QColor(30, 60, 140));
    grad.setColorAt(0.5, QColor(200, 120, 80));
    grad.setColorAt(1.0, QColor(240, 220, 100));
    p.fillRect(img.rect(), grad);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 40, 50));
    p.drawEllipse(QPointF(w * 0.3, h * 0.45), w * 0.14, w * 0.14);
    p.setBrush(QColor(30, 160, 90));
    p.drawRoundedRect(QRectF(w * 0.55, h * 0.2, w * 0.28, h * 0.5), 10, 10);
    p.setPen(QPen(QColor(20, 20, 20), 1.2));
    for (int i = 0; i < 30; ++i)
        p.drawLine(QPointF(w * 0.05 + i * 3.0, h * 0.8), QPointF(w * 0.05 + i * 3.0 + 14, h * 0.95));
    p.end();
    return img;
}

double psnr(const QImage &a, const QImage &b)
{
    if (a.size() != b.size())
        return 0.0;
    const QImage x = a.convertToFormat(QImage::Format_RGBA8888);
    const QImage y = b.convertToFormat(QImage::Format_RGBA8888);
    double se = 0.0;
    for (int row = 0; row < x.height(); ++row)
        for (int col = 0; col < x.width(); ++col)
            for (int c = 0; c < 3; ++c) {
                const double d = double(x.constScanLine(row)[col * 4 + c]) - y.constScanLine(row)[col * 4 + c];
                se += d * d;
            }
    se /= double(x.width()) * x.height() * 3.0;
    return se <= 0.0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / se);
}

QImage reload(const QString &path)
{
    auto decoder = core::DecoderRegistry::instance().decoderFor(path);
    return decoder ? decoder->decode(path).image : QImage();
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// What a saved file really holds, read back through exiv2 (compressed PNG chunks cannot
// be found by searching the bytes). Empty strings = not there.
struct Held {
    QString model;      // Exif.Image.Model
    QString xmpTitle;   // Xmp.dc.title
    QString iptcCaption; // Iptc.Application2.Caption
    QByteArray icc;      // the ICC profile, if any
    bool readable = false;
};

Held readBack(const QString &path)
{
    Held held;
    const QByteArray bytes = readAll(path);
    try {
        auto image = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte *>(bytes.constData()),
                                               static_cast<size_t>(bytes.size()));
        if (!image)
            return held;
        image->readMetadata();
        held.readable = true;
        if (image->iccProfile().size() > 0)
            held.icc = QByteArray(reinterpret_cast<const char *>(image->iccProfile().c_data()), qsizetype(image->iccProfile().size()));
        auto model = image->exifData().findKey(Exiv2::ExifKey("Exif.Image.Model"));
        if (model != image->exifData().end())
            held.model = QString::fromStdString(model->toString());
        auto title = image->xmpData().findKey(Exiv2::XmpKey("Xmp.dc.title"));
        if (title != image->xmpData().end())
            held.xmpTitle = QString::fromStdString(title->toString())
                                .remove(QRegularExpression(QStringLiteral("^lang=\"[^\"]*\" "))); // exiv2 prefixes the language
        auto caption = image->iptcData().findKey(Exiv2::IptcKey("Iptc.Application2.Caption"));
        if (caption != image->iptcData().end())
            held.iptcCaption = QString::fromStdString(caption->toString());
    } catch (const Exiv2::Error &) {
    }
    return held;
}

} // namespace

class TestImageWriter : public QObject {
    Q_OBJECT
private slots:
    void everyFormatRoundTrips_data()
    {
        QTest::addColumn<QString>("ext");
        QTest::addColumn<double>("minPsnr");
        QTest::newRow("png") << "png" << 90.0;   // lossless
        QTest::newRow("bmp") << "bmp" << 90.0;
        QTest::newRow("tif") << "tif" << 90.0;   // lossless LZW
        QTest::newRow("jpg") << "jpg" << 42.0;
        QTest::newRow("webp") << "webp" << 34.0; // WebP is always 4:2:0: thin colour lines lose a little
    }
    void everyFormatRoundTrips()
    {
        QFETCH(QString, ext);
        QFETCH(double, minPsnr);
        QTemporaryDir dir;
        const QImage src = photoLike();
        const QString path = dir.filePath("out." + ext);
        const core::SaveResult r = core::saveImageWithOptions(src, path);
        QVERIFY2(r.ok, qPrintable(r.error));
        const QImage back = reload(path);
        QCOMPARE(back.size(), src.size());
        const double q = psnr(src, back);
        qInfo().noquote() << ext << "PSNR" << QString::number(q, 'f', 1) << "dB," << QFileInfo(path).size() << "bytes";
        QVERIFY2(q >= minPsnr, qPrintable(QString::number(q)));
    }

    // Qt's own JPEG default (quality 75, chroma subsampled) is what "Guardar" used to
    // write. The new default must be clearly better, since a re-saved photo is
    // recompressed on top of what the camera already did.
    void defaultJpegIsMuchBetterThanQtsDefault()
    {
        QTemporaryDir dir;
        const QImage src = photoLike(640, 480);
        const QString old = dir.filePath("qt_default.jpg");
        QVERIFY(src.save(old)); // what the previous code did
        const QString now = dir.filePath("new_default.jpg");
        QVERIFY(core::saveImage(src, now));
        const double before = psnr(src, reload(old));
        const double after = psnr(src, reload(now));
        qInfo().noquote() << "JPEG PSNR: Qt default" << QString::number(before, 'f', 1) << "dB ->"
                          << QString::number(after, 'f', 1) << "dB with the new default;"
                          << QFileInfo(old).size() << "->" << QFileInfo(now).size() << "bytes";
        QVERIFY(after > before + 4.0);
    }

    void explicitQualityIsHonoured()
    {
        QTemporaryDir dir;
        const QImage src = photoLike(640, 480);
        core::SaveOptions low, high;
        low.quality = 30;
        high.quality = 98;
        QVERIFY(core::saveImageWithOptions(src, dir.filePath("low.jpg"), low).ok);
        QVERIFY(core::saveImageWithOptions(src, dir.filePath("high.jpg"), high).ok);
        QVERIFY(QFileInfo(dir.filePath("low.jpg")).size() * 2 < QFileInfo(dir.filePath("high.jpg")).size());
        QVERIFY(psnr(src, reload(dir.filePath("high.jpg"))) > psnr(src, reload(dir.filePath("low.jpg"))) + 5.0);
    }

    void unicodeFileNamesWorkForEveryFormat_data()
    {
        QTest::addColumn<QString>("name");
        for (const char *ext : {"png", "jpg", "bmp", "tif", "webp"}) {
            QTest::newRow(ext) << QStringLiteral("ñandú 日本語 тест \U0001F600.") + QString::fromLatin1(ext);
        }
    }
    void unicodeFileNamesWorkForEveryFormat()
    {
        QFETCH(QString, name);
        QTemporaryDir dir;
        const QString path = dir.filePath(name);
        const core::SaveResult r = core::saveImageWithOptions(photoLike(64, 48), path);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(QFileInfo::exists(path));
        QCOMPARE(reload(path).size(), QSize(64, 48));
    }

    void transparencyBecomesWhiteInJpegAndSurvivesInPngAndWebp()
    {
        QTemporaryDir dir;
        QImage src(40, 30, QImage::Format_RGBA8888);
        src.fill(QColor(0, 0, 0, 0)); // fully transparent
        for (int y = 10; y < 20; ++y)
            for (int x = 10; x < 20; ++x)
                src.setPixelColor(x, y, QColor(200, 30, 30, 255));
        QVERIFY(core::saveImage(src, dir.filePath("t.jpg"), 98));
        const QImage jpg = reload(dir.filePath("t.jpg"));
        QVERIFY(qRed(jpg.pixel(2, 2)) > 240 && qGreen(jpg.pixel(2, 2)) > 240 && qBlue(jpg.pixel(2, 2)) > 240);
        QVERIFY(core::saveImage(src, dir.filePath("t.png")));
        QCOMPARE(qAlpha(reload(dir.filePath("t.png")).pixel(2, 2)), 0);
        QVERIFY(core::saveImage(src, dir.filePath("t.webp"), 95));
        QVERIFY(qAlpha(reload(dir.filePath("t.webp")).pixel(2, 2)) < 10);
        QVERIFY(qAlpha(reload(dir.filePath("t.webp")).pixel(15, 15)) > 245);
    }

    // The original must never be lost: a save that cannot complete leaves it
    // exactly as it was, and no temporary files are left behind.
    void aFailedSaveLeavesTheOriginalUntouched()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("keep.png");
        QVERIFY(core::saveImage(photoLike(32, 32), path));
        const QByteArray before = readAll(path);
        QVERIFY(!before.isEmpty());

        // unsupported extension: refused before touching anything
        const core::SaveResult bad = core::saveImageWithOptions(photoLike(), dir.filePath("keep.xyz"));
        QVERIFY(!bad.ok);
        QVERIFY(!bad.error.isEmpty());
        QVERIFY(!QFileInfo::exists(dir.filePath("keep.xyz")));

        // destination folder that does not exist
        const core::SaveResult missing = core::saveImageWithOptions(photoLike(), dir.filePath("nope/keep.png"));
        QVERIFY(!missing.ok);

        // an empty image
        QVERIFY(!core::saveImageWithOptions(QImage(), path).ok);
        QCOMPARE(readAll(path), before);

        // a successful overwrite replaces it completely and leaves nothing else around
        QVERIFY(core::saveImage(photoLike(64, 64), path));
        QVERIFY(readAll(path) != before);
        QCOMPARE(reload(path).size(), QSize(64, 64));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    // "Guardar" over a photo must keep what the camera recorded: model, exposure,
    // GPS, date and the colour profile - with the orientation reset, because the
    // pixels have been turned upright by then.
    void savingCarriesTheOriginalsMetadata()
    {
        QTemporaryDir dir;
        const QString original = kFixtures + QStringLiteral("/exif_rich_phone.jpg");
        auto decoder = core::DecoderRegistry::instance().decoderFor(original);
        const core::DecodeResult decoded = decoder->decode(original);
        QVERIFY(decoded.ok);
        QCOMPARE(decoded.image.size(), QSize(48, 64)); // stored 64x48, Orientation 6

        // without a source: a clean file, no camera data
        QVERIFY(core::saveImage(decoded.image, dir.filePath("bare.jpg")));
        QVERIFY(core::MetadataReader::read(dir.filePath("bare.jpg")).cameraModel.isEmpty());

        // with the original as the source: everything descriptive travels
        core::SaveOptions options;
        options.metadataSource = original;
        const core::SaveResult r = core::saveImageWithOptions(decoded.image, dir.filePath("kept.jpg"), options);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY2(r.metadataCopied, qPrintable(r.warning));

        const core::ImageMetadata meta = core::MetadataReader::read(dir.filePath("kept.jpg"));
        QCOMPARE(meta.cameraMake, QStringLiteral("ImageViewerTest"));
        QCOMPARE(meta.cameraModel, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(meta.isoSpeed, 200);
        QVERIFY(qAbs(meta.apertureF - 1.8) < 0.05);
        QVERIFY(meta.hasGps);
        QVERIFY2(qAbs(meta.gpsLatitude - (-33.4375)) < 0.001, qPrintable(QString::number(meta.gpsLatitude)));
        QVERIFY(qAbs(meta.gpsLongitude - (-70.65)) < 0.001);
        QVERIFY(meta.dateTaken.isValid());
        QCOMPARE(meta.raw.value(QStringLiteral("Exif.Image.Orientation")), QStringLiteral("1"));
        QCOMPARE(meta.raw.value(QStringLiteral("Exif.Photo.PixelXDimension")), QStringLiteral("48"));
        QCOMPARE(meta.raw.value(QStringLiteral("Exif.Photo.PixelYDimension")), QStringLiteral("64"));
        QVERIFY(readAll(dir.filePath("kept.jpg")).contains("ICC_PROFILE")); // colour profile carried over

        // and the picture is still upright when it is opened again (no double rotation)
        const QImage again = reload(dir.filePath("kept.jpg"));
        QCOMPARE(again.size(), QSize(48, 64));
        QVERIFY(psnr(decoded.image, again) > 38.0);
    }

    // Overwriting the original with itself as the metadata source (the normal
    // "Guardar" case) must work and keep the metadata.
    void overwritingTheOriginalKeepsItsMetadata()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("photo.jpg");
        QVERIFY(QFile::copy(kFixtures + QStringLiteral("/exif_rich_phone.jpg"), path));
        const QImage upright = reload(path);
        core::SaveOptions options;
        options.metadataSource = path;
        const core::SaveResult r = core::saveImageWithOptions(upright, path, options);
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(r.metadataCopied);
        QCOMPARE(core::MetadataReader::read(path).cameraModel, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(reload(path).size(), QSize(48, 64));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1);
    }

    // ---- the rest of the metadata: XMP and IPTC, and PNG on either side -----------

    // Saves the fixture's pixels to `outName` with the fixture itself as the metadata source.
    QString saveFrom(const QTemporaryDir &dir, const QString &fixture, const QString &outName, core::SaveResult *result = nullptr)
    {
        const QString source = kFixtures + QLatin1Char('/') + fixture;
        auto decoder = core::DecoderRegistry::instance().decoderFor(source);
        const core::DecodeResult decoded = decoder->decode(source);
        core::SaveOptions options;
        options.metadataSource = source;
        const core::SaveResult r = core::saveImageWithOptions(decoded.image, dir.filePath(outName), options);
        if (result)
            *result = r;
        return dir.filePath(outName);
    }

    // The fixture's own metadata is readable at all (the baseline for the tests below).
    void fixturesCarryWhatTheyShould()
    {
        const Held jpg = readBack(kFixtures + QStringLiteral("/meta_rich.jpg"));
        QCOMPARE(jpg.model, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(jpg.xmpTitle, QStringLiteral("ViewerTestXmpTitle"));
        QCOMPARE(jpg.iptcCaption, QStringLiteral("ViewerTestIptcCaption"));
        const Held png = readBack(kFixtures + QStringLiteral("/meta_rich.png"));
        QVERIFY2(png.readable, "exiv2 does not recognise a PNG at all (built without zlib?)");
        QCOMPARE(png.model, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(png.xmpTitle, QStringLiteral("ViewerTestXmpTitle"));
    }

    // The information panel reads a PNG's EXIF too.
    void metadataReaderReadsAPng()
    {
        QCOMPARE(core::MetadataReader::read(kFixtures + QStringLiteral("/meta_rich.png")).cameraModel,
                 QStringLiteral("PhoneCam 9000"));
    }

    // README: "conserva EXIF/IPTC/XMP" - for a JPEG over a JPEG all three must travel.
    void jpegKeepsExifXmpAndIptc()
    {
        QTemporaryDir dir;
        core::SaveResult r;
        const Held held = readBack(saveFrom(dir, QStringLiteral("meta_rich.jpg"), QStringLiteral("o.jpg"), &r));
        QVERIFY2(r.ok && r.metadataCopied, qPrintable(r.error + r.warning));
        QCOMPARE(held.model, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(held.xmpTitle, QStringLiteral("ViewerTestXmpTitle"));
        QCOMPARE(held.iptcCaption, QStringLiteral("ViewerTestIptcCaption"));
    }

    // A PNG saved over a PNG (screenshots, exports from other editors) keeps its metadata.
    void pngKeepsExifAndXmp()
    {
        QTemporaryDir dir;
        core::SaveResult r;
        const Held held = readBack(saveFrom(dir, QStringLiteral("meta_rich.png"), QStringLiteral("o.png"), &r));
        QVERIFY2(r.ok && r.metadataCopied, qPrintable(r.error + r.warning));
        QCOMPARE(held.model, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(held.xmpTitle, QStringLiteral("ViewerTestXmpTitle"));
    }

    // Converting between formats keeps what both can hold.
    void metadataTravelsBetweenFormats_data()
    {
        QTest::addColumn<QString>("fixture");
        QTest::addColumn<QString>("out");
        QTest::newRow("jpg->png") << "meta_rich.jpg" << "o.png";
        QTest::newRow("jpg->webp") << "meta_rich.jpg" << "o.webp";
        QTest::newRow("png->jpg") << "meta_rich.png" << "o.jpg";
        QTest::newRow("png->webp") << "meta_rich.png" << "o.webp";
    }
    void metadataTravelsBetweenFormats()
    {
        QFETCH(QString, fixture);
        QFETCH(QString, out);
        QTemporaryDir dir;
        core::SaveResult r;
        const Held held = readBack(saveFrom(dir, fixture, out, &r));
        QVERIFY2(r.ok && r.metadataCopied, qPrintable(r.error + r.warning));
        QCOMPARE(held.model, QStringLiteral("PhoneCam 9000"));
        QCOMPARE(held.xmpTitle, QStringLiteral("ViewerTestXmpTitle"));
    }

    // HEIC/RAW sources cannot hand their metadata over: the save works and SAYS so.
    void aSourceWhoseMetadataIsOutOfReachIsReported()
    {
        QTemporaryDir dir;
        core::SaveOptions options;
        options.metadataSource = kFixtures + QStringLiteral("/format_sample.heic");
        const core::SaveResult r = core::saveImageWithOptions(photoLike(64, 48), dir.filePath("out.jpg"), options);
        QVERIFY(r.ok);
        QVERIFY(!r.metadataCopied);
        QVERIFY2(r.warning.contains(QStringLiteral(".heic")), qPrintable(r.warning));
        // ...but a format that never carries camera metadata is nothing to warn about
        options.metadataSource = kFixtures + QStringLiteral("/format_sample.gif");
        QVERIFY(core::saveImageWithOptions(photoLike(64, 48), dir.filePath("out2.jpg"), options).warning.isEmpty());
    }

    // ---- colour profile of what is written ---------------------------------------------

    // The pixels are sRGB (everything is converted when it is opened), so the file says
    // sRGB - whatever profile the ORIGINAL carried. Copying the original's profile over
    // converted pixels would apply the conversion a second time when anyone opens the file.
    void savedFilesAreTaggedSrgbNotWithTheOriginalsProfile_data()
    {
        QTest::addColumn<QString>("out");
        QTest::newRow("jpg") << "o.jpg";
        QTest::newRow("png") << "o.png";
        QTest::newRow("webp") << "o.webp";
        QTest::newRow("tiff") << "o.tif";
    }
    void savedFilesAreTaggedSrgbNotWithTheOriginalsProfile()
    {
        QFETCH(QString, out);
        QTemporaryDir dir;
        // an original tagged Display P3 (a JPEG made here, with the profile in it)
        const QString original = dir.filePath("p3.jpg");
        QImage p3(32, 32, QImage::Format_RGBA8888);
        p3.fill(QColor(200, 120, 60));
        p3.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
        QVERIFY(p3.save(original, nullptr, 95));

        core::SaveOptions options;
        options.metadataSource = original;
        const QImage srgbPixels(32, 32, QImage::Format_RGBA8888);
        QImage px = srgbPixels;
        px.fill(QColor(213, 115, 42));
        const core::SaveResult r = core::saveImageWithOptions(px, dir.filePath(out), options);
        QVERIFY2(r.ok, qPrintable(r.error + r.warning));

        const QByteArray icc = readBack(dir.filePath(out)).icc;
        QVERIFY2(!icc.isEmpty(), "the file carries no colour profile at all");
        QImage probe(2, 2, QImage::Format_RGBA8888);
        probe.fill(QColor(213, 115, 42));
        // it is an sRGB profile (not the original's Display P3: that one would move the colour)
        QCOMPARE(core::convertToSrgb(probe, icc), core::ColorConversion::AlreadySrgb);
    }

    // The PNG's profile chunk is a proper one: a real name (exiv2 writes it empty, which the
    // format does not allow), a good checksum - Qt reads it back as an sRGB colour space.
    void theProfileChunkOfAPngIsValid()
    {
        QTemporaryDir dir;
        QVERIFY(core::saveImageWithOptions(photoLike(48, 32), dir.filePath("o.png")).ok);
        const QByteArray bytes = readAll(dir.filePath("o.png"));
        const int at = bytes.indexOf("iCCP");
        QVERIFY(at > 0);
        QCOMPARE(bytes.mid(at + 4, 12), QByteArray("ICC Profile\0", 12));
        const QImage back(dir.filePath("o.png"));
        QVERIFY(!back.isNull());
        QVERIFY(back.colorSpace().isValid());
        QCOMPARE(back.colorSpace().primaries(), QColorSpace::Primaries::SRgb);
    }

    // A profile that is still needed (Qt could not apply it, so the pixels are not sRGB)
    // is what gets attached instead.
    void aPendingProfileIsWhatGetsAttached()
    {
        QTemporaryDir dir;
        const QByteArray odd = QColorSpace(QColorSpace::DisplayP3).iccProfile().left(300); // unusable, but "RGB "
        core::SaveOptions options;
        options.iccProfile = odd;
        QVERIFY(core::saveImageWithOptions(photoLike(48, 32), dir.filePath("o.jpg"), options).ok);
        // (exiv2 would refuse to hand back a profile whose size field is wrong, so look at the file itself)
        const QByteArray bytes = readAll(dir.filePath("o.jpg"));
        QVERIFY(bytes.contains("ICC_PROFILE"));
        QVERIFY(bytes.contains(odd));
    }

    // The file a user opens again shows the same colours: P3 original -> converted pixels
    // -> saved -> opened gives the same numbers (no second conversion).
    void aConvertedPictureSurvivesASaveAndReopen()
    {
        QTemporaryDir dir;
        const QString original = dir.filePath("p3.png");
        QImage p3(32, 32, QImage::Format_RGBA8888);
        p3.fill(QColor(200, 120, 60));
        p3.setColorSpace(QColorSpace(QColorSpace::DisplayP3));
        QVERIFY(p3.save(original));

        const core::DecodeResult first = core::DecoderRegistry::instance().decoderFor(original)->decode(original);
        QVERIFY(first.ok);
        core::SaveOptions options;
        options.metadataSource = original;
        for (const char *ext : {"png", "jpg", "tif"}) {
            const QString out = dir.filePath(QStringLiteral("saved.") + QLatin1String(ext));
            QVERIFY(core::saveImageWithOptions(first.image, out, options).ok);
            const core::DecodeResult again = core::DecoderRegistry::instance().decoderFor(out)->decode(out);
            QVERIFY2(again.ok, qPrintable(again.error));
            const QRgb a = first.image.pixel(8, 8), b = again.image.pixel(8, 8);
            QVERIFY2(qAbs(qRed(a) - qRed(b)) <= 3 && qAbs(qGreen(a) - qGreen(b)) <= 3 && qAbs(qBlue(a) - qBlue(b)) <= 3,
                     qPrintable(QStringLiteral("%1: first %2 reopened %3").arg(QLatin1String(ext)).arg(a, 0, 16).arg(b, 0, 16)));
        }
    }

    void metadataFromAnUnreadableSourceIsOnlyAWarning()
    {
        QTemporaryDir dir;
        core::SaveOptions options;
        options.metadataSource = dir.filePath("does_not_exist.jpg");
        const core::SaveResult r = core::saveImageWithOptions(photoLike(64, 48), dir.filePath("out.jpg"), options);
        QVERIFY(r.ok); // the picture is still saved
        QVERIFY(!r.metadataCopied);
        QVERIFY(QFileInfo::exists(dir.filePath("out.jpg")));
    }
};

QTEST_MAIN(TestImageWriter)
#include "test_image_writer.moc"
