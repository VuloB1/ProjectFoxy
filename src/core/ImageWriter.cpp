#include "ImageWriter.h"
#include "ColorManagement.h"
#include "decoders/VipsGuard.h"

#include <exiv2/exiv2.hpp>
#include <vips/vips8>
#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <array>
#include <mutex>
#include <vector>

using namespace vips;

namespace core {

namespace {

enum class Format { Png, Bmp, Jpeg, Webp, Tiff, Unknown };

Format formatFor(const QString &ext)
{
    if (ext == QLatin1String("png")) return Format::Png;
    if (ext == QLatin1String("bmp")) return Format::Bmp;
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg")) return Format::Jpeg;
    if (ext == QLatin1String("webp")) return Format::Webp;
    if (ext == QLatin1String("tif") || ext == QLatin1String("tiff")) return Format::Tiff;
    return Format::Unknown;
}

// --- Encoding (everything is encoded in memory first) -------------------------

bool encodeWithQt(const QImage &image, const char *format, QByteArray &out, QString &error)
{
    QBuffer buffer(&out);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, format)) {
        error = QStringLiteral("Qt no pudo codificar la imagen (%1)").arg(QString::fromLatin1(format));
        return false;
    }
    return true;
}

bool encodeWithVips(const QImage &image, Format format, int quality, const QByteArray &icc, QByteArray &out,
                    QString &error)
{
    const QImage rgba = image.format() == QImage::Format_RGBA8888 ? image : image.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.isNull()) {
        error = QStringLiteral("La imagen está vacía");
        return false;
    }

    QString suffix;
    switch (format) {
    case Format::Jpeg: {
        const int q = quality > 0 ? qBound(1, quality, 100) : kDefaultJpegQuality;
        // 4:4:4 (no chroma subsampling) from quality 90 up: the saturated edges of
        // a re-saved photo do not bleed.
        suffix = QStringLiteral(".jpg[Q=%1,optimize_coding,subsample_mode=%2]").arg(q).arg(q >= 90 ? "off" : "auto");
        break;
    }
    case Format::Webp: {
        const int q = quality > 0 ? qBound(1, quality, 100) : kDefaultWebpQuality;
        suffix = QStringLiteral(".webp[Q=%1]").arg(q);
        break;
    }
    case Format::Tiff:
        suffix = QStringLiteral(".tif[compression=lzw,predictor=horizontal]");
        break;
    default:
        error = QStringLiteral("Formato no soportado");
        return false;
    }

    if (!ensureVipsInitialized()) {
        error = QStringLiteral("libvips no se pudo iniciar: %1").arg(QString::fromStdString(vipsInitError()));
        return false;
    }
    // See VipsGuard.h - serializes every entry into libvips across the app.
    std::lock_guard<std::mutex> vipsLock(vipsEntryMutex());

    try {
        // Wraps rgba's buffer directly (no copy); it outlives the write below.
        VImage vimg = VImage::new_from_memory(const_cast<uchar *>(rgba.constBits()), static_cast<size_t>(rgba.sizeInBytes()),
                                              rgba.width(), rgba.height(), 4, VIPS_FORMAT_UCHAR);
        vimg = vimg.copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
        // TIFF only: JPEG, PNG and WebP get their profile from the exiv2 step below.
        if (format == Format::Tiff && !icc.isEmpty())
            vips_image_set_blob_copy(vimg.get_image(), VIPS_META_ICC_NAME, icc.constData(), size_t(icc.size()));
        if (format == Format::Jpeg) // no alpha channel in JPEG: put the picture on white
            vimg = vimg.flatten(VImage::option()->set("background", std::vector<double>{255.0, 255.0, 255.0}));

        void *buf = nullptr;
        size_t len = 0;
        vimg.write_to_buffer(suffix.toUtf8().constData(), &buf, &len);
        if (!buf || len == 0) {
            error = QStringLiteral("libvips no produjo datos");
            return false;
        }
        out = QByteArray(static_cast<const char *>(buf), static_cast<qsizetype>(len));
        g_free(buf);
        return true;
    } catch (const VError &e) {
        error = QStringLiteral("libvips: %1").arg(QString::fromUtf8(e.what()).trimmed());
        return false;
    }
}

// --- Metadata -----------------------------------------------------------------

bool canHoldMetadata(Format f) { return f == Format::Jpeg || f == Format::Png || f == Format::Webp; }

bool sourceHasReadableMetadata(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    static const QStringList ok = {"jpg", "jpeg", "jpe", "jfif", "png", "webp", "tif", "tiff"};
    return ok.contains(ext);
}

// Camera files whose metadata this build cannot hand over (HEIC/AVIF/CR3 need exiv2's
// BMFF support, which is left out; RAW files are not read as a source here). Saving a
// picture from one of them says so instead of dropping the metadata in silence.
bool sourceMetadataIsOutOfReach(const QString &path)
{
    static const QStringList lost = {"heic", "heif", "avif", "cr3", "dng", "cr2", "nef", "nrw", "arw",
                                     "orf", "rw2", "pef", "raf", "srw", "raw"};
    return lost.contains(QFileInfo(path).suffix().toLower());
}

// Which EXIF tags of the original travel with the picture. The structural tags
// of the stored image (size, strips, compression, thumbnail pointers...) describe
// the OLD file and must not be copied; the descriptive ones (camera, lens,
// exposure, dates, GPS, copyright...) are the point. Maker notes are left out:
// their internal offsets are tied to the original file layout.
bool exifTagTravels(const Exiv2::Exifdatum &d)
{
    const std::string group = d.groupName();
    if (group == "Photo") {
        const std::string name = d.tagName();
        return name != "MakerNote" && name != "InteroperabilityTag" && name != "PixelXDimension"
            && name != "PixelYDimension";
    }
    if (group == "GPSInfo" || group == "Iop")
        return true;
    if (group == "Image") {
        static const char *const keep[] = {"Make", "Model", "Software", "DateTime", "Artist", "Copyright",
                                           "ImageDescription", "XResolution", "YResolution", "ResolutionUnit"};
        for (const char *k : keep)
            if (d.tagName() == k)
                return true;
    }
    return false;
}

// --- PNG colour profile ----------------------------------------------------------
// exiv2 writes the PNG "iCCP" chunk with an EMPTY profile name, which the PNG format does
// not allow (libpng warns "bad keyword" and other readers may ignore the profile), so the
// chunk is built here, with a proper name, and slipped in right after the header.

quint32 crc32Of(const QByteArray &bytes)
{
    static const auto table = [] {
        std::array<quint32, 256> t{};
        for (quint32 n = 0; n < 256; ++n) {
            quint32 c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[n] = c;
        }
        return t;
    }();
    quint32 c = 0xFFFFFFFFu;
    for (const char b : bytes)
        c = table[(c ^ static_cast<quint8>(b)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

QByteArray bigEndian32(quint32 v)
{
    QByteArray out(4, 0);
    out[0] = char(v >> 24);
    out[1] = char(v >> 16);
    out[2] = char(v >> 8);
    out[3] = char(v);
    return out;
}

// `png` with an iCCP chunk for `icc` inserted after IHDR. Unchanged when `png` is not a
// PNG that starts the usual way.
QByteArray withPngProfile(const QByteArray &png, const QByteArray &icc)
{
    constexpr int kIhdrEnd = 8 + 4 + 4 + 13 + 4; // signature + length + "IHDR" + data + crc
    if (icc.isEmpty() || png.size() < kIhdrEnd || png.mid(12, 4) != "IHDR")
        return png;
    // keyword, NUL, compression method 0, then the zlib-compressed profile (qCompress puts a
    // 4-byte length in front of a standard zlib stream: drop it)
    QByteArray data = QByteArray("ICC Profile") + '\0' + '\0' + qCompress(icc, 9).mid(4);
    QByteArray chunk = bigEndian32(quint32(data.size())) + "iCCP" + data;
    chunk += bigEndian32(crc32Of(QByteArray("iCCP") + data));
    return png.left(kIhdrEnd) + chunk + png.mid(kIhdrEnd);
}

// Finishes a JPEG/PNG/WebP file in `encoded`: carries the original's metadata into it (when
// `sourcePath` is given) and tags it with the colour profile `icc`. Returns the new bytes,
// or an empty array when it could not be done.
//
// The ORIGINAL's ICC profile is deliberately not copied: the editor converts every picture
// to sRGB when it is opened, so that profile no longer describes these pixels - attaching it
// would make any viewer convert them a second time.
QByteArray withMetadata(const QByteArray &encoded, const QString &sourcePath, const QByteArray &icc, QSize size,
                        QString &warning, bool &copied)
{
    try {
        auto dst = Exiv2::ImageFactory::open(Exiv2::BasicIo::UniquePtr(new Exiv2::MemIo(
            reinterpret_cast<const Exiv2::byte *>(encoded.constData()), static_cast<size_t>(encoded.size()))));
        if (!dst)
            return {};
        dst->readMetadata();

        if (!sourcePath.isEmpty()) {
            QFile srcFile(sourcePath); // read through QFile: correct with Unicode paths on Windows
            if (!srcFile.open(QIODevice::ReadOnly)) {
                warning = QStringLiteral("No se pudieron leer los metadatos del original");
            } else {
                const QByteArray srcBytes = srcFile.readAll();
                srcFile.close();
                try {
                    auto src = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte *>(srcBytes.constData()),
                                                         static_cast<size_t>(srcBytes.size()));
                    if (src) {
                        src->readMetadata();

                        Exiv2::ExifData exif;
                        for (const Exiv2::Exifdatum &d : src->exifData())
                            if (exifTagTravels(d))
                                exif.add(d);
                        if (!exif.empty()) {
                            exif["Exif.Image.Orientation"] = static_cast<uint16_t>(1); // pixels are upright already
                            exif["Exif.Photo.PixelXDimension"] = static_cast<uint32_t>(size.width());
                            exif["Exif.Photo.PixelYDimension"] = static_cast<uint32_t>(size.height());
                            dst->setExifData(exif);
                        }

                        Exiv2::XmpData xmp = src->xmpData();
                        for (const char *key : {"Xmp.tiff.Orientation", "Xmp.tiff.ImageWidth", "Xmp.tiff.ImageLength",
                                                "Xmp.exif.PixelXDimension", "Xmp.exif.PixelYDimension"}) {
                            auto it = xmp.findKey(Exiv2::XmpKey(key));
                            if (it != xmp.end())
                                xmp.erase(it);
                        }
                        if (!xmp.empty())
                            dst->setXmpData(xmp);

                        if (!src->iptcData().empty())
                            dst->setIptcData(src->iptcData());
                        copied = true;
                    }
                } catch (const Exiv2::Error &e) {
                    warning = QStringLiteral("No se pudieron copiar los metadatos: %1").arg(QString::fromUtf8(e.what()));
                }
            }
        }

        if (!icc.isEmpty())
            dst->setIccProfile(Exiv2::DataBuf(reinterpret_cast<const Exiv2::byte *>(icc.constData()),
                                              static_cast<size_t>(icc.size())),
                               false);

        dst->writeMetadata();

        Exiv2::BasicIo &io = dst->io();
        const Exiv2::byte *data = io.mmap();
        const size_t n = io.size();
        if (!data || n == 0)
            return {};
        return QByteArray(reinterpret_cast<const char *>(data), static_cast<qsizetype>(n));
    } catch (const Exiv2::Error &e) {
        copied = false;
        if (warning.isEmpty())
            warning = QStringLiteral("No se pudieron copiar los metadatos: %1").arg(QString::fromUtf8(e.what()));
        return {};
    }
}

} // namespace

SaveResult saveImageWithOptions(const QImage &image, const QString &path, const SaveOptions &options)
{
    SaveResult result;
    if (image.isNull() || path.isEmpty()) {
        result.error = QStringLiteral("No hay imagen que guardar");
        return result;
    }

    const QString ext = QFileInfo(path).suffix().toLower();
    const Format format = formatFor(ext);
    if (format == Format::Unknown) {
        result.error = QStringLiteral("Formato no soportado para escritura (.%1)").arg(ext);
        return result;
    }

    // Everything is sRGB by the time it is saved (see ColorManagement.h), unless a decoder
    // handed on a profile it could not apply.
    const QByteArray icc = options.iccProfile.isEmpty() ? srgbProfile() : options.iccProfile;

    QByteArray bytes;
    bool encoded = false;
    switch (format) {
    case Format::Png: encoded = encodeWithQt(image, "PNG", bytes, result.error); break;
    case Format::Bmp: encoded = encodeWithQt(image, "BMP", bytes, result.error); break;
    default: encoded = encodeWithVips(image, format, options.quality, icc, bytes, result.error); break;
    }
    if (!encoded)
        return result;

    // (When the source IS the file being replaced - "Guardar" over the original - it is read
    // completely into memory here, before anything is written.)
    if (canHoldMetadata(format)) {
        const bool copyFromSource = !options.metadataSource.isEmpty() && sourceHasReadableMetadata(options.metadataSource);
        // (PNG's profile is attached separately below - see withPngProfile)
        const QByteArray exivIcc = format == Format::Png ? QByteArray() : icc;
        QString warning;
        if (copyFromSource || !exivIcc.isEmpty()) {
            bool copied = false;
            const QByteArray finished = withMetadata(bytes, copyFromSource ? options.metadataSource : QString(), exivIcc,
                                                     image.size(), warning, copied);
            if (!finished.isEmpty()) {
                bytes = finished;
                result.metadataCopied = copied;
            }
        }
        if (format == Format::Png)
            bytes = withPngProfile(bytes, icc);
        if (!warning.isEmpty())
            result.warning = warning;
        if (!copyFromSource && !options.metadataSource.isEmpty() && sourceMetadataIsOutOfReach(options.metadataSource)) {
            result.warning = QStringLiteral("Los metadatos de los archivos .%1 no se pueden conservar al guardar")
                                 .arg(QFileInfo(options.metadataSource).suffix().toLower());
        }
    }

    // Atomic write: QSaveFile writes to a temporary file in the target's folder
    // and renames it over the target on commit(), so the original is either
    // fully replaced or untouched.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = QStringLiteral("No se pudo abrir el destino para escribir: %1").arg(file.errorString());
        return result;
    }
    if (file.write(bytes) != bytes.size()) {
        const QString why = file.errorString();
        file.cancelWriting();
        result.error = QStringLiteral("No se pudo escribir el archivo: %1").arg(why);
        return result;
    }
    if (!file.commit()) {
        result.error = QStringLiteral("No se pudo reemplazar el archivo: %1").arg(file.errorString());
        return result;
    }

    result.ok = true;
    return result;
}

bool saveImage(const QImage &image, const QString &path, int quality)
{
    SaveOptions options;
    options.quality = quality;
    return saveImageWithOptions(image, path, options).ok;
}

QStringList supportedSaveExtensions()
{
    return { "png", "jpg", "jpeg", "bmp", "tif", "tiff", "webp" };
}

} // namespace core
