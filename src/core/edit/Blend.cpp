#include "Blend.h"

#include <algorithm>
#include <cmath>

namespace core::edit {

namespace {

inline float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

inline float colorBurn(float b, float s)
{
    if (b >= 1.f)
        return 1.f;
    if (s <= 0.f)
        return 0.f;
    return 1.f - std::min(1.f, (1.f - b) / s);
}

inline float colorDodge(float b, float s)
{
    if (b <= 0.f)
        return 0.f;
    if (s >= 1.f)
        return 1.f;
    return std::min(1.f, b / (1.f - s));
}

inline float softLight(float b, float s)
{
    if (s <= 0.5f)
        return b - (1.f - 2.f * s) * b * (1.f - b);
    const float d = b <= 0.25f ? ((16.f * b - 12.f) * b + 4.f) * b : std::sqrt(b);
    return b + (2.f * s - 1.f) * (d - b);
}

inline float overlay(float b, float s) { return b <= 0.5f ? 2.f * b * s : 1.f - 2.f * (1.f - b) * (1.f - s); }

float separable(BlendMode m, float b, float s)
{
    switch (m) {
    case BlendMode::Normal: return s;
    case BlendMode::Darken: return std::min(b, s);
    case BlendMode::Multiply: return b * s;
    case BlendMode::ColorBurn: return colorBurn(b, s);
    case BlendMode::LinearBurn: return std::max(0.f, b + s - 1.f);
    case BlendMode::Lighten: return std::max(b, s);
    case BlendMode::Screen: return b + s - b * s;
    case BlendMode::ColorDodge: return colorDodge(b, s);
    case BlendMode::LinearDodge: return std::min(1.f, b + s);
    case BlendMode::Overlay: return overlay(b, s);
    case BlendMode::SoftLight: return softLight(b, s);
    case BlendMode::HardLight: return overlay(s, b); // overlay with the layers swapped
    case BlendMode::VividLight: return s < 0.5f ? colorBurn(b, 2.f * s) : colorDodge(b, 2.f * (s - 0.5f));
    case BlendMode::PinLight: return s < 0.5f ? std::min(b, 2.f * s) : std::max(b, 2.f * s - 1.f);
    case BlendMode::HardMix: return b + s >= 1.f ? 1.f : 0.f;
    case BlendMode::Difference: return std::abs(b - s);
    case BlendMode::Exclusion: return b + s - 2.f * b * s;
    case BlendMode::Xor: {
        const int a8 = int(std::lround(clamp01(b) * 255.f)), b8 = int(std::lround(clamp01(s) * 255.f));
        return float(a8 ^ b8) / 255.f;
    }
    default: return s;
    }
}

// --- the HSL modes (W3C compositing spec) ---

inline float lum(const float c[3]) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

void clipColor(float c[3])
{
    const float l = lum(c);
    const float n = std::min({c[0], c[1], c[2]});
    const float x = std::max({c[0], c[1], c[2]});
    if (n < 0.f) {
        for (int i = 0; i < 3; ++i)
            c[i] = l + (c[i] - l) * l / (l - n);
    }
    if (x > 1.f) {
        for (int i = 0; i < 3; ++i)
            c[i] = l + (c[i] - l) * (1.f - l) / (x - l);
    }
}

void setLum(const float c[3], float l, float out[3])
{
    const float d = l - lum(c);
    for (int i = 0; i < 3; ++i)
        out[i] = c[i] + d;
    clipColor(out);
}

inline float sat(const float c[3]) { return std::max({c[0], c[1], c[2]}) - std::min({c[0], c[1], c[2]}); }

void setSat(const float c[3], float s, float out[3])
{
    int mx = 0, mn = 0;
    for (int i = 1; i < 3; ++i) {
        if (c[i] > c[mx])
            mx = i;
        if (c[i] < c[mn])
            mn = i;
    }
    if (mx == mn) {
        out[0] = out[1] = out[2] = 0.f;
        return;
    }
    const int mid = 3 - mx - mn;
    out[mid] = (c[mid] - c[mn]) * s / (c[mx] - c[mn]);
    out[mx] = s;
    out[mn] = 0.f;
}

} // namespace

const QStringList &blendModeNames()
{
    static const QStringList names = {
        QStringLiteral("Normal"),
        QStringLiteral("Oscurecer"),
        QStringLiteral("Multiplicar"),
        QStringLiteral("Subexponer color"),
        QStringLiteral("Subexposición lineal"),
        QStringLiteral("Aclarar"),
        QStringLiteral("Trama"),
        QStringLiteral("Sobreexponer color"),
        QStringLiteral("Añadir"),
        QStringLiteral("Superponer"),
        QStringLiteral("Luz suave"),
        QStringLiteral("Luz fuerte"),
        QStringLiteral("Luz intensa"),
        QStringLiteral("Luz focal"),
        QStringLiteral("Mezcla definida"),
        QStringLiteral("Diferencia"),
        QStringLiteral("Exclusión"),
        QStringLiteral("Xor"),
        QStringLiteral("Tono"),
        QStringLiteral("Saturación"),
        QStringLiteral("Color"),
        QStringLiteral("Luminosidad"),
    };
    return names;
}

void blendRgb(BlendMode mode, const float base[3], const float top[3], float out[3])
{
    if (mode == BlendMode::Hue || mode == BlendMode::Saturation || mode == BlendMode::Color
        || mode == BlendMode::Luminosity) {
        float t[3];
        switch (mode) {
        case BlendMode::Hue:
            setSat(top, sat(base), t);
            setLum(t, lum(base), out);
            break;
        case BlendMode::Saturation:
            setSat(base, sat(top), t);
            setLum(t, lum(base), out);
            break;
        case BlendMode::Color:
            setLum(top, lum(base), out);
            break;
        default: // Luminosity
            setLum(base, lum(top), out);
            break;
        }
        return;
    }
    for (int i = 0; i < 3; ++i)
        out[i] = clamp01(separable(mode, base[i], top[i]));
}

void compositeOver(uint8_t *dst, double r, double g, double b, double layerAlpha, BlendMode mode)
{
    if (layerAlpha <= 0.0)
        return;
    layerAlpha = std::min(1.0, layerAlpha);
    const float ab = dst[3] / 255.f;
    const float as = static_cast<float>(layerAlpha);
    const float ao = as + ab * (1.f - as);
    if (ao <= 0.f)
        return;
    const float cb[3] = {dst[0] / 255.f, dst[1] / 255.f, dst[2] / 255.f};
    const float cs[3] = {float(r) / 255.f, float(g) / 255.f, float(b) / 255.f};
    float blended[3];
    if (mode == BlendMode::Normal) {
        blended[0] = cs[0];
        blended[1] = cs[1];
        blended[2] = cs[2];
    } else {
        blendRgb(mode, cb, cs, blended);
    }
    for (int i = 0; i < 3; ++i) {
        // Where the base has no colour of its own (transparent) the layer shows as it is.
        const float mixed = (1.f - ab) * cs[i] + ab * blended[i];
        const float co = ((1.f - as) * ab * cb[i] + as * mixed) / ao;
        const float v = clamp01(co) * 255.f;
        dst[i] = static_cast<uint8_t>(v + 0.5f);
    }
    dst[3] = static_cast<uint8_t>(clamp01(ao) * 255.f + 0.5f);
}

} // namespace core::edit
