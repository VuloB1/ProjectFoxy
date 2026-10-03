#include "VipsDecoder.h"
#include "VipsGuard.h"
#include "../ColorManagement.h"

#include <vips/vips8>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QTransform>
#include <QDebug>
#include <cstring>
#include <mutex>

using namespace vips;

namespace {


// The EXIF Orientation (1..8) of the file, 1 when there is none. Phones and
// cameras store pixels in sensor order and rely on this tag to tell the viewer
// how to turn the picture upright.
//
// libvips only reports it for formats that carry it natively (TIFF) - the
// vcpkg build used here has no libexif, so it cannot read a JPEG's EXIF block.
// For JPEG, Qt's own reader (which parses EXIF itself, and opens Unicode paths
// correctly) supplies it.
int orientationOf(const QString &filePath, const VImage &header)
{
    if (header.get_typeof(VIPS_META_ORIENTATION)) {
        const int o = header.get_int(VIPS_META_ORIENTATION);
        if (o >= 1 && o <= 8)
            return o;
    }

    const QString ext = filePath.section('.', -1).toLower();
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("jpe")
        || ext == QLatin1String("jfif")) {
        QImageReader reader(filePath);
        switch (reader.transformation()) {
        case QImageIOHandler::TransformationMirror: return 2;
        case QImageIOHandler::TransformationRotate180: return 3;
        case QImageIOHandler::TransformationFlip: return 4;
        case QImageIOHandler::TransformationFlipAndRotate90: return 5;
        case QImageIOHandler::TransformationRotate90: return 6;
        case QImageIOHandler::TransformationMirrorAndRotate90: return 7;
        case QImageIOHandler::TransformationRotate270: return 8;
        default: break;
        }
    }
    return 1;
}

// Turns `image` upright for EXIF orientation `o` (1 = already upright). The
// mapping is the standard one: 2 mirror, 3 rotate 180, 4 flip, 5 transpose,
// 6 rotate 90 clockwise, 7 transverse, 8 rotate 270 clockwise.
QImage applyOrientation(const QImage &image, int o)
{
    auto rotated = [&image](int degrees) {
        QTransform t;
        t.rotate(degrees);
        return image.transformed(t, Qt::FastTransformation);
    };
    switch (o) {
    case 2: return image.mirrored(true, false);
    case 3: return image.mirrored(true, true);
    case 4: return image.mirrored(false, true);
    case 5: return rotated(90).mirrored(true, false);
    case 6: return rotated(90);
    case 7: return rotated(90).mirrored(false, true);
    case 8: return rotated(270);
    default: return image;
    }
}

} // namespace

namespace core {

bool VipsDecoder::canDecode(const QString &filePath) const
{
    const QString ext = filePath.section('.', -1).toLower();
    return supportedExtensions().contains(ext);
}

DecodeResult VipsDecoder::decode(const QString &filePath, QSize maxSize)
{
    if (!ensureVipsInitialized()) {
        DecodeResult failed;
        failed.error = QStringLiteral("libvips no se pudo iniciar: %1").arg(QString::fromStdString(vipsInitError()));
        return failed;
    }

    // See VipsGuard.h: serializes every entry into libvips across the app,
    // after intermittent heap corruption traced to glib/gio internals when
    // entered concurrently (e.g. filmstrip thumbnails + a full-res decode
    // both hitting vips for the first time on different threads at once).
    std::lock_guard<std::mutex> vipsLock(vipsEntryMutex());

    DecodeResult result;
    // libvips (GLib) takes file names as UTF-8 on every platform. QFile::encodeName
    // would hand it the ANSI code page instead, which cannot open a file named
    // with accents, Cyrillic, Japanese, emoji...
    const QByteArray path = filePath.toUtf8();

    try {
        // Opening a file with libvips is lazy: no pixel data is decoded
        // until something demands it, so reading width/height here is
        // effectively free (header-only).
        VImage header = VImage::new_from_file(
            path.constData(), VImage::option()->set("access", VIPS_ACCESS_SEQUENTIAL));
        const int orientation = orientationOf(filePath, header);
        // The size the user sees: sideways stored pictures (5..8) swap it.
        result.sourceSize = orientation >= 5 ? QSize(header.height(), header.width())
                                             : QSize(header.width(), header.height());

        VImage image;
        const bool wantsPreview = maxSize.isValid() && maxSize.width() > 0 && maxSize.height() > 0;

        // CMYK (print-oriented JPEG/TIFF): four channels, none of them alpha. The libvips
        // used here has no colour-management module, so it cannot convert them to RGB -
        // Qt's JPEG reader can (a plain conversion that also undoes Photoshop's inverted
        // CMYK). Without this the K channel was read as transparency and the colours as
        // R, G, B.
        if (header.interpretation() == VIPS_INTERPRETATION_CMYK) {
            QImageReader reader(filePath);
            reader.setAutoTransform(false); // the orientation is applied below, like for every format
            QImage cmyk = reader.read();
            if (cmyk.isNull()) {
                result.error = QStringLiteral("No se pudo convertir la imagen CMYK: %1").arg(reader.errorString());
                return result;
            }
            cmyk = cmyk.convertToFormat(QImage::Format_RGBA8888);
            if (wantsPreview) {
                const int target = qMax(maxSize.width(), maxSize.height());
                if (qMax(cmyk.width(), cmyk.height()) > target)
                    cmyk = cmyk.scaled(target, target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            result.image = applyOrientation(cmyk, orientation);
            result.ok = true;
            return result;
        }

        if (wantsPreview) {
            const int target = qMax(maxSize.width(), maxSize.height());
            // VImage::thumbnail reopens the file through libvips' own
            // thumbnailing pipeline, which uses format-specific
            // shrink-on-load (e.g. libjpeg DCT scaling, WebP/TIFF/HEIF
            // native downscaling) instead of decoding full resolution and
            // scaling afterwards - this is what makes preview loads fast.
            // libvips' own auto-rotation is switched off: the orientation is
            // applied below, the same way for previews and full decodes.
            image = VImage::thumbnail(
                path.constData(), target,
                VImage::option()->set("height", target)->set("size", VIPS_SIZE_DOWN)->set("no_rotate", true));
        } else {
            image = header;
        }

        // Normalize to 8-bit sRGB with an alpha channel so every format
        // (grayscale, 16-bit, paletted, RGBA PNG, ...) lands in the same
        // QImage::Format_RGBA8888 layout. colourspace() is what scales a 16-bit picture
        // down to 8 bits (a plain cast clips everything above 255 - a 16-bit photo came
        // out white), turns grey into RGB and carries an alpha band through.
        if (image.interpretation() != VIPS_INTERPRETATION_sRGB)
            image = image.colourspace(VIPS_INTERPRETATION_sRGB);
        if (image.bands() < 3)
            image = image.colourspace(VIPS_INTERPRETATION_sRGB);
        if (image.bands() == 3)
            image = image.bandjoin(255);
        image = image.cast(VIPS_FORMAT_UCHAR);

        const int width = image.width();
        const int height = image.height();

        size_t rawSize = 0;
        void *data = image.write_to_memory(&rawSize);
        if (!data) {
            result.error = QStringLiteral("libvips: write_to_memory failed for %1").arg(filePath);
            return result;
        }

        QImage qimg(width, height, QImage::Format_RGBA8888);
        const size_t expected = static_cast<size_t>(qimg.sizeInBytes());
        if (expected == rawSize) {
            std::memcpy(qimg.bits(), data, rawSize);

            // Colour: bring the picture into sRGB, the one space the editor works in. The
            // profile is read from the file's header (libvips attaches it even without a
            // colour engine); grey and CMYK files have no RGB profile to apply.
            QByteArray profile;
            if (header.bands() >= 3 && header.get_typeof(VIPS_META_ICC_NAME)) {
                size_t length = 0;
                const void *blob = header.get_blob(VIPS_META_ICC_NAME, &length);
                if (blob && length > 0)
                    profile = QByteArray(static_cast<const char *>(blob), qsizetype(length));
            }
            // A profile Qt cannot apply leaves the pixels as they were: hand it on so a
            // later save can attach it again instead of silently dropping it.
            if (convertToSrgb(qimg, profile) == ColorConversion::Unsupported && isRgbProfile(profile))
                result.iccProfile = profile;

            // Decoded in stored order: turn it upright.
            result.image = applyOrientation(qimg, orientation);
            result.ok = true;
        } else {
            result.error = QStringLiteral(
                "libvips: unexpected pixel buffer size for %1 (got %2, expected %3)")
                                .arg(filePath)
                                .arg(rawSize)
                                .arg(expected);
        }

        g_free(data);
    } catch (const VError &err) {
        result.error = QString::fromUtf8(err.what());
    }

    return result;
}

QStringList VipsDecoder::supportedExtensions() const
{
    // Only what the libvips build used here can actually load (JPEG, PNG, WebP and
    // TIFF support are compiled in). BMP, GIF and ICO are handled by
    // QtImageDecoder; PSD, TGA and JXL would need ImageMagick / libjxl, which are
    // not part of this build, so they are not advertised.
    return { "jpg", "jpeg", "png", "webp", "tif", "tiff" };
}

} // namespace core
