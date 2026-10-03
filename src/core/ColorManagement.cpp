#include "ColorManagement.h"

#include <QColorTransform>
#include <algorithm>
#include <cstdlib>

namespace core {

namespace {

// Does `t` leave colours alone (to within one level of 255)? Judged on a grid of samples;
// profiles that call themselves sRGB differ from the ideal one by rounding, not by colour.
bool isIdentity(const QColorTransform &t)
{
    constexpr int levels[] = {0, 32, 96, 160, 224, 255};
    for (int r : levels)
        for (int g : levels)
            for (int b : levels) {
                const QRgb in = qRgb(r, g, b);
                const QRgb out = t.map(in);
                if (std::abs(qRed(out) - r) > 1 || std::abs(qGreen(out) - g) > 1 || std::abs(qBlue(out) - b) > 1)
                    return false;
            }
    return true;
}

} // namespace

ColorConversion convertToSrgb(QImage &image, const QColorSpace &source)
{
    // (Qt 6.7 only builds a valid colour space from an RGB matrix/curves profile: grey,
    // CMYK and table-based profiles come out invalid, which is exactly "unsupported".)
    if (!source.isValid())
        return ColorConversion::Unsupported;

    const QColorTransform transform = source.transformationToColorSpace(QColorSpace(QColorSpace::SRgb));
    if (isIdentity(transform))
        return ColorConversion::AlreadySrgb;

    image.detach(); // the pixels are about to change: never write into a buffer shared with a copy
    image.applyColorTransform(transform);
    return ColorConversion::Converted;
}

ColorConversion convertToSrgb(QImage &image, const QByteArray &iccProfile)
{
    if (iccProfile.isEmpty())
        return ColorConversion::NoProfile;
    return convertToSrgb(image, QColorSpace::fromIccProfile(iccProfile));
}

bool isRgbProfile(const QByteArray &iccProfile)
{
    // The data colour space is the 4-byte signature at offset 16 of the profile header.
    return iccProfile.size() >= 128 && iccProfile.mid(16, 4) == "RGB ";
}

QByteArray srgbProfile()
{
    return QColorSpace(QColorSpace::SRgb).iccProfile();
}

} // namespace core
