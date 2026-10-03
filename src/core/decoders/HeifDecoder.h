#pragma once

#include "IImageDecoder.h"

namespace core {

// HEIC/HEIF and AVIF via libheif.
class HeifDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override;
    DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) override;
    QStringList supportedExtensions() const override;
};

} // namespace core
