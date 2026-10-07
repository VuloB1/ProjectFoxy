#include "HeifDecoder.h"
#include "../ColorManagement.h"

#include <libheif/heif_cxx.h>
#include <QFile>
#include <cstring>
#include "Translate.h"

namespace core {

bool HeifDecoder::canDecode(const QString &filePath) const
{
    const QString ext = filePath.section('.', -1).toLower();
    return supportedExtensions().contains(ext);
}

DecodeResult HeifDecoder::decode(const QString &filePath, QSize maxSize)
{
    // Unlike libvips (shrink-on-load) or LibRaw (half_size), the plain
    // libheif C++ API used here has no equally simple native downscale path
    // - a thumbnail/preview request still decodes at full resolution. Only
    // matters for filmstrip speed, not correctness.
    Q_UNUSED(maxSize);

    DecodeResult result;

    // Read the bytes ourselves via QFile (correctly handles Unicode paths on
    // Windows) and hand them to libheif in memory, rather than
    // read_from_file(): libheif's own file opening goes through a plain
    // fopen() on Windows, which uses the ANSI codepage rather than UTF-8 and
    // can fail on non-ASCII paths/filenames (accented characters, etc).
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = core::tr("No se pudo abrir %1").arg(filePath);
        return result;
    }
    const QByteArray fileData = file.readAll();
    file.close();

    try {
        // A fresh Context per call - heif::Context wraps its own
        // heif_context*, never shared across calls/threads.
        heif::Context ctx;
        // fileData must outlive ctx/handle/img below - it does, all local
        // to this function and only released after they are.
        ctx.read_from_memory_without_copy(fileData.constData(), static_cast<size_t>(fileData.size()));

        heif::ImageHandle handle = ctx.get_primary_image_handle();
        result.sourceSize = QSize(handle.get_width(), handle.get_height());

        heif::Image img = handle.decode_image(heif_colorspace_RGB, heif_chroma_interleaved_RGBA);

        int stride = 0;
        const uint8_t *plane = img.get_plane(heif_channel_interleaved, &stride);
        const int width = img.get_width(heif_channel_interleaved);
        const int height = img.get_height(heif_channel_interleaved);
        if (!plane || width <= 0 || height <= 0) {
            result.error = QStringLiteral("libheif: no interleaved RGBA plane for %1").arg(filePath);
            return result;
        }

        // stride can exceed width*4 (row padding) - copy row by row instead
        // of assuming a tightly-packed buffer.
        QImage qimg(width, height, QImage::Format_RGBA8888);
        for (int y = 0; y < height; ++y)
            std::memcpy(qimg.scanLine(y), plane + static_cast<size_t>(y) * stride, static_cast<size_t>(width) * 4);

        if (result.sourceSize.isEmpty())
            result.sourceSize = qimg.size();

        // Colour: phones store HEIC/AVIF in Display P3. The profile is either an ICC blob
        // or just the nclx colour primaries; both are brought into sRGB here (see
        // ColorManagement.h). libheif has already undone the YCbCr matrix, so what is
        // left is the gamut.
        QByteArray profile;
        QColorSpace nclxSpace; // invalid = none
        try {
            // The file-level profile hangs off the image handle (that is where a 'colr'
            // box is read into); fall back to the decoded image's own, which some files
            // carry instead. Only the handle has an nclx accessor in the C API used here.
            const heif_image_handle *rawHandle = handle.get_raw_image_handle();
            heif_color_profile_type type = heif_image_handle_get_color_profile_type(rawHandle);
            if (type == heif_color_profile_type_not_present)
                type = img.get_color_profile_type();

            if (type == heif_color_profile_type_prof || type == heif_color_profile_type_rICC) {
                if (heif_image_handle_get_color_profile_type(rawHandle) == type) {
                    const size_t length = heif_image_handle_get_raw_color_profile_size(rawHandle);
                    profile.resize(qsizetype(length));
                    if (heif_image_handle_get_raw_color_profile(rawHandle, profile.data()).code != heif_error_Ok)
                        profile.clear();
                } else {
                    const std::vector<uint8_t> raw = img.get_raw_color_profile();
                    profile = QByteArray(reinterpret_cast<const char *>(raw.data()), qsizetype(raw.size()));
                }
            } else if (type == heif_color_profile_type_nclx) {
                // Only Display P3 primaries are mapped (the other common value, BT.709,
                // is sRGB already; BT.2020 files are HDR-oriented and left alone).
                heif_color_profile_nclx *nclx = nullptr;
                if (heif_image_handle_get_nclx_color_profile(rawHandle, &nclx).code == heif_error_Ok && nclx) {
                    if (nclx->color_primaries == heif_color_primaries_SMPTE_EG_432_1)
                        nclxSpace = QColorSpace(QColorSpace::DisplayP3);
                    heif_nclx_color_profile_free(nclx);
                }
            }
        } catch (const heif::Error &) {
            // no readable profile: treated as sRGB
        }
        if (!profile.isEmpty()) {
            if (convertToSrgb(qimg, profile) == ColorConversion::Unsupported && isRgbProfile(profile))
                result.iccProfile = profile; // kept so a save can attach it again
        } else if (nclxSpace.isValid()) {
            convertToSrgb(qimg, nclxSpace);
        }

        result.image = qimg;
        result.ok = true;
    } catch (const heif::Error &err) {
        result.error = QStringLiteral("libheif: %1 (%2)")
                           .arg(QString::fromStdString(err.get_message()), filePath);
    }

    return result;
}

QStringList HeifDecoder::supportedExtensions() const
{
    return { "heic", "heif", "avif" };
}

} // namespace core
