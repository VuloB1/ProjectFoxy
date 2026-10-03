#include "RawDecoder.h"

#include <libraw/libraw.h>

namespace core {

bool RawDecoder::canDecode(const QString &filePath) const
{
    const QString ext = filePath.section('.', -1).toLower();
    return supportedExtensions().contains(ext);
}

DecodeResult RawDecoder::decode(const QString &filePath, QSize maxSize)
{
    DecodeResult result;

    // A fresh LibRaw instance per call, never shared across threads/calls -
    // safe as long as this links against libraw_r (see CMakeLists.txt).
    LibRaw raw;

#ifdef _WIN32
    int ret = raw.open_file(reinterpret_cast<const wchar_t *>(filePath.utf16()));
#else
    const QByteArray path = filePath.toUtf8();
    int ret = raw.open_file(path.constData());
#endif
    if (ret != LIBRAW_SUCCESS) {
        result.error = QStringLiteral("LibRaw: %1 (%2)").arg(QString::fromUtf8(LibRaw::strerror(ret)), filePath);
        return result;
    }

    // The true source resolution, independent of any half_size preview
    // decode below - populated by open_file()'s metadata parse already, no
    // need to wait for unpack()/dcraw_process().
    // (LibRaw reports the sensor's width/height; its output is turned upright by
    // the camera's orientation, and for the quarter turns - flip 5 and 6, bit 4
    // set - that swaps the two.)
    result.sourceSize = (raw.imgdata.sizes.flip & 4)
        ? QSize(raw.imgdata.sizes.height, raw.imgdata.sizes.width)
        : QSize(raw.imgdata.sizes.width, raw.imgdata.sizes.height);

    // half_size skips full demosaicing - much faster for filmstrip
    // thumbnails/quick previews than decoding full resolution and scaling
    // down afterwards.
    const bool wantsPreview = maxSize.isValid() && maxSize.width() > 0 && maxSize.height() > 0;
    raw.imgdata.params.half_size = wantsPreview ? 1 : 0;
    raw.imgdata.params.use_camera_wb = 1; // matches what most raw viewers show by default
    raw.imgdata.params.output_color = 1;  // sRGB
    raw.imgdata.params.output_bps = 8;

    ret = raw.unpack();
    if (ret != LIBRAW_SUCCESS) {
        result.error = QStringLiteral("LibRaw unpack: %1 (%2)").arg(QString::fromUtf8(LibRaw::strerror(ret)), filePath);
        return result;
    }

    ret = raw.dcraw_process();
    if (ret != LIBRAW_SUCCESS) {
        result.error = QStringLiteral("LibRaw dcraw_process: %1 (%2)").arg(QString::fromUtf8(LibRaw::strerror(ret)), filePath);
        return result;
    }

    int memErr = 0;
    libraw_processed_image_t *processed = raw.dcraw_make_mem_image(&memErr);
    if (!processed || memErr != LIBRAW_SUCCESS) {
        result.error = QStringLiteral("LibRaw dcraw_make_mem_image failed for %1").arg(filePath);
        if (processed)
            LibRaw::dcraw_clear_mem(processed);
        return result;
    }

    QImage::Format sourceFormat;
    if (processed->type == LIBRAW_IMAGE_BITMAP && processed->bits == 8 && processed->colors == 4)
        sourceFormat = QImage::Format_RGBA8888;
    else if (processed->type == LIBRAW_IMAGE_BITMAP && processed->bits == 8 && processed->colors == 3)
        sourceFormat = QImage::Format_RGB888;
    else if (processed->type == LIBRAW_IMAGE_BITMAP && processed->bits == 8 && processed->colors == 1)
        sourceFormat = QImage::Format_Grayscale8;
    else {
        result.error = QStringLiteral("LibRaw: unexpected output (%1 channels, %2 bits) for %3")
                           .arg(processed->colors).arg(processed->bits).arg(filePath);
        LibRaw::dcraw_clear_mem(processed);
        return result;
    }

    // Wraps processed->data directly (no copy yet); convertToFormat always
    // allocates its own buffer for a genuine format change, and .copy()
    // forces one even in the (impossible here, since output_bps=8 never
    // yields RGBA8888 directly from LibRaw) case where it wouldn't - either
    // way `qimg` must fully own its data before processed's buffer is freed.
    QImage wrapped(processed->data, processed->width, processed->height,
                   processed->colors * processed->width, sourceFormat);
    QImage qimg = wrapped.convertToFormat(QImage::Format_RGBA8888).copy();
    LibRaw::dcraw_clear_mem(processed);

    if (result.sourceSize.isEmpty())
        result.sourceSize = qimg.size();

    result.image = qimg;
    result.ok = true;
    return result;
}

QStringList RawDecoder::supportedExtensions() const
{
    return {
        "cr2", "cr3", "nef", "arw", "dng", "orf", "rw2",
        "raf", "srw", "pef", "raw"
    };
}

} // namespace core
