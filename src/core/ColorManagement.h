#pragma once

#include <QByteArray>
#include <QColorSpace>
#include <QImage>

namespace core {

// The editor works in ONE colour space: sRGB, 8 bits. Every picture is brought into it
// when it is decoded, so the maths of Ajustes / Filtros / Efectos, the preview and the
// saved file all talk about the same numbers - and a Display P3 or Adobe RGB photo is
// shown with its real colours instead of looking washed out.
enum class ColorConversion {
    NoProfile,    // nothing to convert: the picture is taken to be sRGB already
    AlreadySrgb,  // it has a profile, but the profile IS sRGB (to within rounding): untouched
    Converted,    // the pixels were converted to sRGB
    Unsupported,  // the profile cannot be used (not an RGB profile, or one Qt cannot read):
                  // pixels untouched - the caller keeps the profile so saving can re-attach it
};

// Converts `image` (RGBA8888, straight alpha - alpha is never touched) from the colour
// space described by `iccProfile` / `source` to sRGB, in place.
ColorConversion convertToSrgb(QImage &image, const QByteArray &iccProfile);
ColorConversion convertToSrgb(QImage &image, const QColorSpace &source);

// Is this an ICC profile for RGB data (as opposed to grey, CMYK, Lab...)? Only such a
// profile can go on describing RGB pixels.
bool isRgbProfile(const QByteArray &iccProfile);

// An sRGB ICC profile, to tag saved files with.
QByteArray srgbProfile();

} // namespace core
