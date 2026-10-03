#pragma once

#include "IImageDecoder.h"

namespace core {

// Camera RAW formats via LibRaw: CR2/CR3, NEF, ARW, DNG, ORF, RW2, etc.
// Supports fast half-size decode for previews (LibRaw's half_size option)
// and embedded-thumbnail extraction for instant filmstrip rendering.
class RawDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override;
    DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) override;
    QStringList supportedExtensions() const override;
};

} // namespace core
