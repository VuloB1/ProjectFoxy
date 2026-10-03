#pragma once

#include "IImageDecoder.h"

namespace core {

// Real frame-by-frame playback for animated GIF and animated PNG (APNG).
// Deliberately narrow: canDecode() only returns true for a file that is
// GENUINELY multi-frame (GIF with more than one image, or a PNG that
// actually carries an acTL chunk) - every ordinary static .gif/.png file
// still falls through to VipsDecoder exactly as before, so this can't
// regress the (much more common) static case's decode path/performance.
//
// GIF frames come from Qt's own bundled "qgif" plugin (QImageReader) - no
// new dependency, since that plugin is already deployed alongside qjpeg.
// APNG has no Qt/vips support at all, so frames are reconstructed by hand:
// each fcTL/fdAT chunk pair is rewritten into a standalone one-frame PNG
// (fdAT's 4-byte sequence-number prefix stripped, chunk renamed to IDAT)
// and decoded via QImage::fromData - reusing Qt's own PNG codec rather than
// re-implementing deflate/PNG filtering - then composited frame-over-frame
// per the APNG spec's dispose/blend rules.
class AnimatedDecoder : public IImageDecoder {
public:
    bool canDecode(const QString &filePath) const override;
    DecodeResult decode(const QString &filePath, QSize maxSize = QSize()) override;
    QStringList supportedExtensions() const override;
};

} // namespace core
