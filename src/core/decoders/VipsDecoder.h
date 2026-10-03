#pragma once

#include "IImageDecoder.h"

namespace core {

// Handles the common raster formats via libvips: JPEG, PNG, GIF (incl.
// animated - first frame for now), BMP, WebP, TIFF, JPEG XL, PSD (flattened).
// libvips decodes by region/shrink-on-load so requesting a small maxSize
// avoids ever touching full-resolution pixel data.
class VipsDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override;
    DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) override;
    QStringList supportedExtensions() const override;
};

} // namespace core
