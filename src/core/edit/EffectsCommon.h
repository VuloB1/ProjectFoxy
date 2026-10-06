#pragma once

// Building blocks shared by the effect implementations (Effects.cpp and the files that hold the
// newer families: colour, pattern, light, geometry). Internal to core/edit - nothing outside it
// includes this. Everything lives in core::edit::fxk.

#include "Effects.h"
#include "Operations.h"
#include "ParallelRows.h"

#include <QImage>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace core::edit::fxk {

constexpr double kPi = 3.14159265358979323846;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline uint8_t toByte(double v) { return v <= 0.0 ? 0 : (v >= 255.0 ? 255 : static_cast<uint8_t>(v + 0.5)); }
inline double smoothStep01(double t)
{
    t = clampd(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Deterministic per-position randomness (the same on every run, so a preview
// and the applied result agree exactly).
inline uint32_t hash2(int x, int y, uint32_t salt)
{
    uint32_t h = static_cast<uint32_t>(x) * 0x9E3779B1u + static_cast<uint32_t>(y) * 0x85EBCA77u + salt * 0xC2B2AE3Du;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}
inline double hashUnit(uint32_t h) { return double(h >> 8) / 16777216.0; } // [0, 1)

// A small sequential random generator (xorshift32) for effects that scatter objects: the same
// seed always gives the same sequence on every machine.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 0x9E3779B9u) { if (s == 0) s = 1; next(); next(); }
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    double unit() { return double(next() >> 8) / 16777216.0; }      // [0, 1)
    double range(double a, double b) { return a + (b - a) * unit(); }
    double gauss() { return (unit() + unit() + unit() + unit() - 2.0) * 1.7320508; } // ~N(0,1)
};

// --- Cancellation ------------------------------------------------------------

// Every pass runs through rows()/bands(), which stop starting new rows as soon
// as the caller raises its cancel flag: a slider drag abandons the stale,
// full-size computation within a few rows instead of waiting for it to finish.
// A cancelled result is half-written garbage and must be discarded.
struct Job {
    const std::atomic<bool> *cancel = nullptr;
    bool cancelled() const { return cancel && cancel->load(std::memory_order_relaxed); }
};

template <class F>
void rows(const Job &job, int height, F &&fn)
{
    forEachRowParallel(height, [&](int y) {
        if (!job.cancelled())
            fn(y);
    });
}

template <class F>
void bands(const Job &job, int count, int perBand, F &&fn)
{
    forEachBandParallel(count, perBand, [&](int a, int b) {
        if (!job.cancelled())
            fn(a, b);
    });
}

// --- Pixel buffers -----------------------------------------------------------

// A tightly packed 8-bit image with `ch` interleaved channels (4 = RGBA
// premultiplied, 1 = a single gray plane).
struct Plane {
    int w = 0;
    int h = 0;
    int ch = 4;
    std::vector<uint8_t> d;

    Plane() = default;
    Plane(int width, int height, int channels)
        : w(width), h(height), ch(channels), d(size_t(width) * size_t(height) * size_t(channels)) {}
    uint8_t *row(int y) { return d.data() + size_t(y) * w * ch; }
    const uint8_t *row(int y) const { return d.data() + size_t(y) * w * ch; }
};

Plane planeFrom(const QImage &rgba);
QImage imageFrom(const Plane &p, QImage::Format format);

// Separable Gaussian (three box passes) of a Plane; sigma < 0.45 returns the source.
Plane gaussianBlur(const Job &job, const Plane &src, double sigma);

// --- Sampling ---------------------------------------------------------------

// Bilinear read of an RGBA plane; pixel (i, j) is centered on integer
// coordinates and anything outside is clamped to the border.
inline void bilinear(const Plane &p, double sx, double sy, float out[4])
{
    sx = clampd(sx, 0.0, p.w - 1);
    sy = clampd(sy, 0.0, p.h - 1);
    const int x0 = static_cast<int>(sx);
    const int y0 = static_cast<int>(sy);
    const int x1 = std::min(x0 + 1, p.w - 1);
    const int y1 = std::min(y0 + 1, p.h - 1);
    const float fx = static_cast<float>(sx - x0);
    const float fy = static_cast<float>(sy - y0);
    const uint8_t *a = p.row(y0) + size_t(x0) * 4;
    const uint8_t *b = p.row(y0) + size_t(x1) * 4;
    const uint8_t *c = p.row(y1) + size_t(x0) * 4;
    const uint8_t *e = p.row(y1) + size_t(x1) * 4;
    for (int k = 0; k < 4; ++k) {
        const float top = a[k] + (b[k] - a[k]) * fx;
        const float bottom = c[k] + (e[k] - c[k]) * fx;
        out[k] = top + (bottom - top) * fy;
    }
}

// Adds the bilinear sample at (sx, sy) to acc[0..3]; the blurs call it up to ~100 times per pixel,
// so it works in float, with one clamp per axis and no per-call row arithmetic beyond two offsets.
inline void accumulate(const Plane &p, double px, double py, float acc[4])
{
    const float sx = static_cast<float>(clampd(px, 0.0, p.w - 1));
    const float sy = static_cast<float>(clampd(py, 0.0, p.h - 1));
    const int x0 = static_cast<int>(sx);
    const int y0 = static_cast<int>(sy);
    const int x1 = x0 + (x0 < p.w - 1);
    const int y1 = y0 + (y0 < p.h - 1);
    const float fx = sx - x0, fy = sy - y0;
    const size_t stride = size_t(p.w) * 4;
    const uint8_t *r0 = p.d.data() + size_t(y0) * stride;
    const uint8_t *r1 = p.d.data() + size_t(y1) * stride;
    const uint8_t *a = r0 + size_t(x0) * 4, *b = r0 + size_t(x1) * 4;
    const uint8_t *c = r1 + size_t(x0) * 4, *e = r1 + size_t(x1) * 4;
    const float w00 = (1.f - fx) * (1.f - fy), w10 = fx * (1.f - fy), w01 = (1.f - fx) * fy, w11 = fx * fy;
    for (int k = 0; k < 4; ++k)
        acc[k] += a[k] * w00 + b[k] * w10 + c[k] * w01 + e[k] * w11;
}

struct FloatPlane {
    int w = 0, h = 0;
    std::vector<float> d;
    float at(int x, int y) const { return d[size_t(clampi(y, 0, h - 1)) * w + clampi(x, 0, w - 1)]; }
    float bilinear(double sx, double sy) const
    {
        sx = clampd(sx, 0.0, w - 1);
        sy = clampd(sy, 0.0, h - 1);
        const int x0 = static_cast<int>(sx);
        const int y0 = static_cast<int>(sy);
        const float fx = static_cast<float>(sx - x0);
        const float fy = static_cast<float>(sy - y0);
        const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * fx;
        const float bottom = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * fx;
        return top + (bottom - top) * fy;
    }
};

FloatPlane lumaPlane(const Job &job, const QImage &rgba);

// Builds the output by asking, for every destination pixel, where in the
// source it should come from.
template <class Mapper>
Plane remap(const Job &job, const Plane &src, Mapper &&mapper)
{
    Plane out(src.w, src.h, 4);
    rows(job, src.h, [&](int y) {
        uint8_t *d = out.row(y);
        for (int x = 0; x < src.w; ++x) {
            double sx = x, sy = y;
            mapper(x, y, sx, sy);
            float c[4];
            bilinear(src, sx, sy, c);
            for (int k = 0; k < 4; ++k)
                d[x * 4 + k] = toByte(c[k]);
        }
    });
    return out;
}

// --- Colour parameters ----------------------------------------------------------

// A colour slot of an effect is stored in a double as 0xRRGGBB.
struct Rgb {
    double r = 0, g = 0, b = 0; // 0..255
};
inline Rgb unpackColor(double v)
{
    const uint32_t c = static_cast<uint32_t>(clampd(v, 0.0, 16777215.0));
    return {double((c >> 16) & 255), double((c >> 8) & 255), double(c & 255)};
}
inline double packColor(int r, int g, int b) { return double((uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b)); }

// Hue in degrees (0..360), saturation and value 0..1 -> RGB 0..255.
inline Rgb hsvToRgb(double h, double s, double v)
{
    h = std::fmod(h, 360.0);
    if (h < 0)
        h += 360.0;
    const double c = v * s, hp = h / 60.0;
    const double x = c * (1.0 - std::abs(std::fmod(hp, 2.0) - 1.0));
    double r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; }
    else if (hp < 2) { r = x; g = c; }
    else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; }
    else if (hp < 5) { r = x; b = c; }
    else { r = c; b = x; }
    const double m = v - c;
    return {(r + m) * 255.0, (g + m) * 255.0, (b + m) * 255.0};
}

// Luma, Rec. 601 (the weights the rest of the effects use).
inline double lumaOf(double r, double g, double b) { return 0.299 * r + 0.587 * g + 0.114 * b; }

// --- Catalogue helpers -------------------------------------------------------------

EffectParam slider(const char *label, double lo, double hi, double def, const char *suffix = "%", bool integer = false);
EffectParam choice(const char *label, QStringList options, int def);
EffectParam toggle(const char *label, bool def = false);
EffectParam colorParam(const char *label, int r, int g, int b);
EffectParam seedParam(const char *label = "Semilla", int def = 1);
// Marks `p` as shown only when parameter `on` holds one of `values`.
EffectParam shownWhen(EffectParam p, int on, std::initializer_list<int> values);
EffectParam withHint(EffectParam p, const char *hint);
EffectSpec makeSpec(const char *id, const char *name, const char *group, std::vector<EffectParam> params);

// The "Fusión" choice every decorative effect has (see Blend.h).
EffectParam blendParam(int def = 0);

// Each family of newer effects lives in its own file and offers the same two entry points: its
// catalogue entries, and a renderer that returns false when `id` is not one of its effects.
void addDecorSpecs(std::vector<EffectSpec> &out);
bool renderDecor(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result);
void addPatternSpecs(std::vector<EffectSpec> &out);
bool renderPattern(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                   QImage &result);
void addColorSpecs(std::vector<EffectSpec> &out);
bool renderColor(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result);
void addLightSpecs(std::vector<EffectSpec> &out);
bool renderLight(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result);
void addGeometrySpecs(std::vector<EffectSpec> &out);
bool renderGeometry(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                    QImage &result);
// The lens correction (EffectsLens.cpp): the render pass and its (hidden) catalogue entry.
QImage fxLens(const Job &job, const QImage &src, const EffectValues &v);
EffectSpec lensSpec();
// The picture frame of the Recortar tool (EffectsFrame.cpp): the hidden "frame" effect.
EffectSpec frameSpec();
QImage fxFrameRender(const Job &job, const QImage &src, const EffectValues &v);
QSize frameOutputSize(const EffectValues &v, QSize input);
// The meme caption (EffectsCaption.cpp): a band with a text, or the text over the picture.
EffectSpec captionSpec();
QImage fxCaption(const Job &job, const QImage &src, const EffectValues &v, const QString &text);
QSize captionOutputSize(const EffectValues &v, const QString &text, QSize input);
// The size a geometry effect gives a picture of `input` size (see effectOutputSize()).
QSize geometryOutputSize(const QString &id, const EffectValues &v, QSize input);

} // namespace core::edit::fxk
