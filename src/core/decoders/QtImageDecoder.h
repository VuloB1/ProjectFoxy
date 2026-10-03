#pragma once

#include "IImageDecoder.h"

namespace core {

// Decodes the simple still formats that Qt reads natively and that this build's
// libvips (no ImageMagick) cannot: BMP, single-frame GIF and ICO. Animated GIFs
// are claimed earlier by AnimatedDecoder.
class QtImageDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override;
    DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) override;
    QStringList supportedExtensions() const override;
};

} // namespace core
