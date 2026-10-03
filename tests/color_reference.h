#pragma once
// Colour maths written out independently of Qt, to check Qt's (and the decoders') results
// against textbook values: D65 matrices, so no chromatic adaptation is involved.
#include <algorithm>
#include <array>
#include <cmath>

namespace colorref {

using Vec3 = std::array<double, 3>;
using Mat3 = std::array<std::array<double, 3>, 3>;

inline Vec3 mul(const Mat3 &m, const Vec3 &v)
{
    Vec3 out{};
    for (int i = 0; i < 3; ++i)
        out[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
    return out;
}

inline double srgbToLinear(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
inline double linearToSrgb(double c)
{
    c = std::clamp(c, 0.0, 1.0);
    return c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}

// Textbook matrices (D65 white for all of them, so no chromatic adaptation is involved).
inline const Mat3 kDisplayP3ToXyz = {{{0.4865709486, 0.2656676932, 0.1982172852},
                               {0.2289745641, 0.6917385218, 0.0792869141},
                               {0.0000000000, 0.0451133819, 1.0439443689}}};
inline const Mat3 kAdobeRgbToXyz = {{{0.5767309, 0.1855540, 0.1881852},
                              {0.2973769, 0.6273491, 0.0752741},
                              {0.0270343, 0.0706872, 0.9911085}}};
inline const Mat3 kXyzToSrgb = {{{3.2404542, -1.5371385, -0.4985314},
                          {-0.9692660, 1.8760108, 0.0415560},
                          {0.0556434, -0.2040259, 1.0572252}}};

// What colour (r,g,b in 0..255) of a space with the given matrix and transfer function
// is in sRGB - computed here independently of Qt.
inline Vec3 referenceToSrgb(const Mat3 &toXyz, double (*decode)(double), int r, int g, int b)
{
    const Vec3 linear = {decode(r / 255.0), decode(g / 255.0), decode(b / 255.0)};
    const Vec3 srgbLinear = mul(kXyzToSrgb, mul(toXyz, linear));
    return {linearToSrgb(srgbLinear[0]) * 255.0, linearToSrgb(srgbLinear[1]) * 255.0, linearToSrgb(srgbLinear[2]) * 255.0};
}

inline double adobeDecode(double c) { return std::pow(c, 563.0 / 256.0); }


} // namespace colorref
