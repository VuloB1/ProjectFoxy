#pragma once

#include <QByteArray>
#include <QString>
#include <QImage>
#include <QSize>
#include <QVector>
#include <memory>

namespace core {

// Result of a decode operation. `image` may be a downsampled preview when
// `requestedMaxSize` was smaller than the source — decoders are expected to
// use native region/shrink-on-load support (libvips, LibRaw half-size, etc.)
// rather than decoding full-res and scaling down.
struct DecodeResult {
    QImage image;
    QSize sourceSize;
    bool ok = false;
    QString error;

    // `image` is always in sRGB (see ColorManagement.h): a picture that carried another
    // profile has been converted. This holds the profile that STILL describes the pixels,
    // and only when it could not be applied (one Qt cannot read): the pixels were left as
    // they were and saving puts this profile back. Empty in the normal case.
    QByteArray iccProfile;

    // Only AnimatedDecoder ever populates these, and only for a full decode
    // (maxSize invalid) of a genuinely multi-frame GIF/APNG - every other
    // decoder/call leaves them empty, which ImageLoader forwards as-is onto
    // ImageDocument's own animationFrames/animationDelaysMs.
    QVector<QImage> frames;
    QVector<int> frameDelaysMs;
};

// One decoder implementation per format family (raster, RAW, HEIF/AVIF, SVG).
// Implementations must be safe to call from a worker thread.
class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;

    virtual bool canDecode(const QString &filePath) const = 0;

    // maxSize: hint for the largest dimension the caller actually needs
    // (e.g. viewport size). QSize() means "full resolution".
    virtual DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) = 0;

    virtual QStringList supportedExtensions() const = 0;
};

using IImageDecoderPtr = std::shared_ptr<IImageDecoder>;

} // namespace core
