#pragma once

#include <QImage>
#include <QSize>

namespace core::edit {

// High-quality resize used by the Redimensionar tab.
//
// The pixels are resampled with a separable Lanczos-3 filter (a wider, sharper
// kernel than the bilinear/box mix behind Qt::SmoothTransformation; when
// shrinking the kernel is stretched, so it also averages properly instead of
// aliasing). When `enhanceDetail` is set and the picture is being ENLARGED, a
// halo-free sharpening pass follows: interpolation can only smooth, so edges
// come out softer than they would in a photo taken at that size. The pass
// brightens/darkens the luminance around each edge the way an unsharp mask
// does, but never past the range of the pixel's own neighborhood, so it
// restores crispness without ringing.
//
// `targetSize` is a bounding box, fitted with the aspect ratio kept when
// `keepAspectRatio` is true. Returns Format_RGBA8888.
QImage resizeHighQuality(const QImage &source, QSize targetSize, bool keepAspectRatio, bool enhanceDetail);

// Nearest-neighbour resize for pixel pictures (modo pixel): every output pixel is a copy of exactly one source
// pixel (the one under its centre), so no colour that was not in the picture appears and
// edges stay hard. Enlarging by a whole number repeats each pixel the same number of times.
// `targetSize` / `keepAspectRatio` mean the same as above. Returns Format_RGBA8888.
QImage resizeNearest(const QImage &source, QSize targetSize, bool keepAspectRatio);

// Exposed for tests: the strength of the enlargement sharpening for a given
// linear enlargement factor (>1).
struct EnhanceParams {
    double sigma;  // blur radius of the unsharp mask, in output pixels
    double amount; // how much of (pixel - blur) is added back
};
EnhanceParams enhanceParamsFor(double enlargement);

} // namespace core::edit
