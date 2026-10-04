#include "Effects.h"
#include "AdjustMath.h"
#include "Denoise.h"
#include "ParallelRows.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace core::edit {

namespace {

constexpr double kPi = 3.14159265358979323846;

inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline uint8_t toByte(double v) { return v <= 0.0 ? 0 : (v >= 255.0 ? 255 : static_cast<uint8_t>(v + 0.5)); }

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

Plane planeFrom(const QImage &rgba)
{
    Plane p(rgba.width(), rgba.height(), 4);
    for (int y = 0; y < p.h; ++y)
        std::copy_n(rgba.constScanLine(y), size_t(p.w) * 4, p.row(y));
    return p;
}

QImage imageFrom(const Plane &p, QImage::Format format)
{
    QImage img(p.w, p.h, format);
    for (int y = 0; y < p.h; ++y)
        std::copy_n(p.row(y), size_t(p.w) * 4, img.scanLine(y));
    return img;
}

// --- Gaussian blur (three box blurs) ---------------------------------------

void boxBlurH(const Job &job, const Plane &src, Plane &dst, int r)
{
    const int w = src.w, ch = src.ch, win = 2 * r + 1;
    rows(job, src.h, [&](int y) {
        const uint8_t *s = src.row(y);
        uint8_t *d = dst.row(y);
        int sum[4] = {0, 0, 0, 0};
        for (int k = -r; k <= r; ++k) {
            const uint8_t *p = s + size_t(clampi(k, 0, w - 1)) * ch;
            for (int c = 0; c < ch; ++c)
                sum[c] += p[c];
        }
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < ch; ++c)
                d[size_t(x) * ch + c] = static_cast<uint8_t>((sum[c] + win / 2) / win);
            const uint8_t *add = s + size_t(std::min(x + r + 1, w - 1)) * ch;
            const uint8_t *sub = s + size_t(std::max(x - r, 0)) * ch;
            for (int c = 0; c < ch; ++c)
                sum[c] += int(add[c]) - int(sub[c]);
        }
    });
}

// Column strips, each walked top to bottom with one running sum per column:
// far friendlier to the cache than sliding down single columns.
void boxBlurV(const Job &job, const Plane &src, Plane &dst, int r)
{
    const int w = src.w, h = src.h, ch = src.ch, win = 2 * r + 1;
    constexpr int kStrip = 64;
    const int strips = (w + kStrip - 1) / kStrip;
    bands(job, strips, 1, [&](int s0, int s1) {
        for (int si = s0; si < s1; ++si) {
            const int x0 = si * kStrip;
            const int x1 = std::min(w, x0 + kStrip);
            const int n = (x1 - x0) * ch;
            std::vector<int> sum(size_t(n), 0);
            for (int k = -r; k <= r; ++k) {
                const uint8_t *p = src.row(clampi(k, 0, h - 1)) + size_t(x0) * ch;
                for (int j = 0; j < n; ++j)
                    sum[j] += p[j];
            }
            for (int y = 0; y < h; ++y) {
                uint8_t *d = dst.row(y) + size_t(x0) * ch;
                for (int j = 0; j < n; ++j)
                    d[j] = static_cast<uint8_t>((sum[j] + win / 2) / win);
                const uint8_t *add = src.row(std::min(y + r + 1, h - 1)) + size_t(x0) * ch;
                const uint8_t *sub = src.row(std::max(y - r, 0)) + size_t(x0) * ch;
                for (int j = 0; j < n; ++j)
                    sum[j] += int(add[j]) - int(sub[j]);
            }
        }
    });
}

Plane gaussianBlur(const Job &job, const Plane &src, double sigma)
{
    if (sigma < 0.45 || src.w < 2 || src.h < 2)
        return src;
    // Three box widths whose combined variance matches a Gaussian of `sigma`.
    constexpr int kPasses = 3;
    const double ideal = std::sqrt(12.0 * sigma * sigma / kPasses + 1.0);
    int lower = static_cast<int>(std::floor(ideal));
    if (lower % 2 == 0)
        --lower;
    const int upper = lower + 2;
    const double mIdeal = (12.0 * sigma * sigma - kPasses * lower * lower - 4.0 * kPasses * lower - 3.0 * kPasses)
                          / (-4.0 * lower - 4.0);
    const int m = static_cast<int>(std::lround(mIdeal));

    Plane a = src;
    Plane b(src.w, src.h, src.ch);
    for (int i = 0; i < kPasses; ++i) {
        const int size = i < m ? lower : upper;
        const int r = (size - 1) / 2;
        if (r <= 0)
            continue;
        boxBlurH(job, a, b, r);
        boxBlurV(job, b, a, r);
    }
    return a;
}

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

FloatPlane lumaPlane(const Job &job, const QImage &rgba)
{
    FloatPlane y;
    y.w = rgba.width();
    y.h = rgba.height();
    y.d.resize(size_t(y.w) * y.h);
    rows(job, y.h, [&](int row) {
        const uint8_t *s = rgba.constScanLine(row);
        float *d = &y.d[size_t(row) * y.w];
        for (int x = 0; x < y.w; ++x)
            d[x] = 0.299f * s[x * 4] + 0.587f * s[x * 4 + 1] + 0.114f * s[x * 4 + 2];
    });
    return y;
}

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

// --- Blurs ------------------------------------------------------------------

Plane fxGaussian(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    if (v[0] <= 0.0)
        return src;
    return gaussianBlur(job, src, std::max(0.8, v[0] / 100.0 * 0.02 * longSide));
}

Plane fxMotion(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    if (v[0] <= 0.0)
        return src;
    const double dist = std::max(2.0, v[0] / 100.0 * 0.06 * longSide);
    const double angle = v[1] * kPi / 180.0;
    const double ca = std::cos(angle), sa = std::sin(angle);
    const int taps = clampi(static_cast<int>(dist) + 1, 3, 96);
    std::vector<double> offX(taps), offY(taps); // the same line of offsets for every pixel
    for (int k = 0; k < taps; ++k) {
        const double t = (double(k) / (taps - 1) - 0.5) * dist;
        offX[k] = t * ca;
        offY[k] = t * sa;
    }
    Plane out(src.w, src.h, 4);
    rows(job, src.h, [&](int y) {
        uint8_t *d = out.row(y);
        for (int x = 0; x < src.w; ++x) {
            float acc[4] = {0, 0, 0, 0};
            for (int k = 0; k < taps; ++k)
                accumulate(src, x + offX[k], y + offY[k], acc);
            for (int j = 0; j < 4; ++j)
                d[x * 4 + j] = toByte(acc[j] / taps);
        }
    });
    return out;
}

// Blur that smears every pixel toward / around a center: `radial` false = along
// the line to the center (zoom), true = along the circle around it (spin).
Plane fxRadialBlur(const Job &job, const Plane &src, const EffectValues &v, bool spin)
{
    if (v[0] <= 0.0)
        return src;
    const double cx = (src.w - 1) * 0.5 * (1.0 + v[1] / 100.0);
    const double cy = (src.h - 1) * 0.5 * (1.0 + v[2] / 100.0);
    const double amount = v[0] / 100.0;
    const double zoomSpan = amount * 0.4;                 // the scale sweeps +-20% at most
    const double spinSpan = amount * 30.0 * kPi / 180.0;  // and the angle +-15 degrees
    // Spin: the step between two samples depends only on how many there are, so its sine and cosine
    // come from a table and each sample is the previous one turned by that step.
    constexpr int kMaxTaps = 64;
    double stepCos[kMaxTaps + 1] = {}, stepSin[kMaxTaps + 1] = {};
    for (int n = 2; n <= kMaxTaps; ++n) {
        stepCos[n] = std::cos(spinSpan / (n - 1));
        stepSin[n] = std::sin(spinSpan / (n - 1));
    }
    const double startCos = std::cos(-0.5 * spinSpan), startSin = std::sin(-0.5 * spinSpan);
    Plane out(src.w, src.h, 4);
    rows(job, src.h, [&](int y) {
        uint8_t *d = out.row(y);
        for (int x = 0; x < src.w; ++x) {
            const double dx = x - cx, dy = y - cy;
            const double r = std::sqrt(dx * dx + dy * dy);
            // One sample every ~3 px along the smear, so far-from-center pixels
            // (which travel the longest) do not show stepping.
            const double reach = spin ? r * spinSpan : r * zoomSpan;
            const int taps = clampi(static_cast<int>(reach * (1.0 / 3.0)) + 1, 6, kMaxTaps);
            float acc[4] = {0, 0, 0, 0};
            if (spin) {
                const double cs = stepCos[taps], sn = stepSin[taps];
                double vx = dx * startCos - dy * startSin;
                double vy = dx * startSin + dy * startCos;
                for (int k = 0; k < taps; ++k) {
                    accumulate(src, cx + vx, cy + vy, acc);
                    const double nx = vx * cs - vy * sn;
                    vy = vx * sn + vy * cs;
                    vx = nx;
                }
            } else {
                const double step = zoomSpan / (taps - 1);
                double scale = 1.0 - 0.5 * zoomSpan;
                for (int k = 0; k < taps; ++k, scale += step)
                    accumulate(src, cx + dx * scale, cy + dy * scale, acc);
            }
            for (int j = 0; j < 4; ++j)
                d[x * 4 + j] = toByte(acc[j] / taps);
        }
    });
    return out;
}

// Replaces each channel by the median of its (2r+1)x(2r+1) neighborhood: wipes
// out isolated bright/dark specks (impulse noise) while keeping edges sharp.
Plane fxMedian(const Job &job, const Plane &src, const EffectValues &v)
{
    const int r = clampi(static_cast<int>(std::lround(v[0])), 1, 2);
    const int side = 2 * r + 1;
    const int count = side * side;
    Plane out(src.w, src.h, 4);
    if (r == 1) {
        // 3x3: the median of nine values with a fixed network of 19 compare-exchanges, much
        // faster than a general selection for the setting most people use.
        auto sortPair = [](uint8_t &a, uint8_t &b) {
            if (a > b)
                std::swap(a, b);
        };
        rows(job, src.h, [&](int y) {
            uint8_t *d = out.row(y);
            const uint8_t *above = src.row(clampi(y - 1, 0, src.h - 1));
            const uint8_t *mid = src.row(y);
            const uint8_t *below = src.row(clampi(y + 1, 0, src.h - 1));
            for (int x = 0; x < src.w; ++x) {
                const size_t xl = size_t(std::max(x - 1, 0)) * 4, xc = size_t(x) * 4, xr = size_t(std::min(x + 1, src.w - 1)) * 4;
                for (int c = 0; c < 4; ++c) {
                    uint8_t p[9] = {above[xl + c], above[xc + c], above[xr + c], mid[xl + c], mid[xc + c],
                                    mid[xr + c],   below[xl + c], below[xc + c], below[xr + c]};
                    sortPair(p[1], p[2]); sortPair(p[4], p[5]); sortPair(p[7], p[8]);
                    sortPair(p[0], p[1]); sortPair(p[3], p[4]); sortPair(p[6], p[7]);
                    sortPair(p[1], p[2]); sortPair(p[4], p[5]); sortPair(p[7], p[8]);
                    sortPair(p[0], p[3]); sortPair(p[5], p[8]); sortPair(p[4], p[7]);
                    sortPair(p[3], p[6]); sortPair(p[1], p[4]); sortPair(p[2], p[5]);
                    sortPair(p[4], p[7]); sortPair(p[4], p[2]); sortPair(p[6], p[4]);
                    sortPair(p[4], p[2]);
                    d[xc + c] = p[4];
                }
            }
        });
        return out;
    }
    rows(job, src.h, [&](int y) {
        uint8_t *d = out.row(y);
        uint8_t window[25];
        for (int x = 0; x < src.w; ++x) {
            for (int c = 0; c < 4; ++c) {
                int n = 0;
                for (int j = -r; j <= r; ++j) {
                    const uint8_t *row = src.row(clampi(y + j, 0, src.h - 1));
                    for (int i = -r; i <= r; ++i)
                        window[n++] = row[size_t(clampi(x + i, 0, src.w - 1)) * 4 + c];
                }
                std::nth_element(window, window + count / 2, window + count);
                d[x * 4 + c] = window[count / 2];
            }
        }
    });
    return out;
}

// --- Stylizing (straight-alpha RGBA in, alpha kept) -------------------------

QImage fxPosterize(const Job &job, const QImage &src, const EffectValues &v)
{
    const int levels = clampi(static_cast<int>(std::lround(v[0])), 2, 32);
    uint8_t table[256];
    for (int i = 0; i < 256; ++i) {
        const int q = std::min(levels - 1, i * levels / 256);
        table[i] = static_cast<uint8_t>(std::lround(q * 255.0 / (levels - 1)));
    }
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            d[x * 4] = table[s[x * 4]];
            d[x * 4 + 1] = table[s[x * 4 + 1]];
            d[x * 4 + 2] = table[s[x * 4 + 2]];
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

QImage fxThreshold(const Job &job, const QImage &src, const EffectValues &v)
{
    const double level = clampd(v[0], 0.0, 255.0);
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double l = 0.299 * s[x * 4] + 0.587 * s[x * 4 + 1] + 0.114 * s[x * 4 + 2];
            const uint8_t bw = l >= level ? 255 : 0;
            d[x * 4] = d[x * 4 + 1] = d[x * 4 + 2] = bw;
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

QImage fxEmboss(const Job &job, const QImage &src, const EffectValues &v)
{
    const FloatPlane luma = lumaPlane(job, src);
    const double angle = v[1] * kPi / 180.0;
    const double dx = std::cos(angle), dy = std::sin(angle);
    const double gain = v[0] / 100.0 * 6.0;
    const double color = clampd(v[2] / 100.0, 0.0, 1.0);
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double slope = luma.bilinear(x + dx, y + dy) - luma.bilinear(x - dx, y - dy);
            const double e = 128.0 + slope * gain;
            for (int c = 0; c < 3; ++c) {
                const double tinted = s[x * 4 + c] + (e - 128.0);
                d[x * 4 + c] = toByte(e + (tinted - e) * color);
            }
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

QImage fxEdges(const Job &job, const QImage &src, const EffectValues &v)
{
    const FloatPlane luma = lumaPlane(job, src);
    const double gain = 1.0 + v[0] / 100.0 * 5.0;
    const bool lightBackground = v[1] >= 0.5;
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double gx = (luma.at(x + 1, y - 1) + 2.0 * luma.at(x + 1, y) + luma.at(x + 1, y + 1))
                              - (luma.at(x - 1, y - 1) + 2.0 * luma.at(x - 1, y) + luma.at(x - 1, y + 1));
            const double gy = (luma.at(x - 1, y + 1) + 2.0 * luma.at(x, y + 1) + luma.at(x + 1, y + 1))
                              - (luma.at(x - 1, y - 1) + 2.0 * luma.at(x, y - 1) + luma.at(x + 1, y - 1));
            double e = std::sqrt(gx * gx + gy * gy) / 4.0 * gain;
            e = clampd(e, 0.0, 255.0);
            const uint8_t out8 = toByte(lightBackground ? 255.0 - e : e);
            d[x * 4] = d[x * 4 + 1] = d[x * 4 + 2] = out8;
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// Pencil sketch: the picture divided by a blurred negative of itself (a
// "color dodge") - flat areas go white, edges stay as thin dark strokes.
QImage fxSketch(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const FloatPlane luma = lumaPlane(job, src);
    Plane gray(src.width(), src.height(), 1);
    Plane inverted(src.width(), src.height(), 1);
    for (int y = 0; y < gray.h; ++y)
        for (int x = 0; x < gray.w; ++x) {
            const uint8_t g = toByte(luma.d[size_t(y) * gray.w + x]);
            gray.row(y)[x] = g;
            inverted.row(y)[x] = static_cast<uint8_t>(255 - g);
        }
    const double sigma = std::max(1.0, longSide * (0.002 + v[0] / 100.0 * 0.010));
    const Plane blurred = gaussianBlur(job, inverted, sigma);
    const double color = clampd(v[1] / 100.0, 0.0, 1.0);

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double base = gray.row(y)[x];
            const double dodge = std::min(255.0, base * 255.0 / (256.0 - blurred.row(y)[x]));
            for (int c = 0; c < 3; ++c) {
                const double tinted = dodge * s[x * 4 + c] / 255.0;
                d[x * 4 + c] = toByte(dodge + (tinted - dodge) * color);
            }
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// Newspaper dots on a rotated grid: each cell gets one dot whose area follows
// the darkness of the picture there.
QImage fxHalftone(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const double cell = std::max(5.0, longSide * (0.004 + v[0] / 100.0 * 0.020));
    const double angle = v[1] * kPi / 180.0;
    const double ca = std::cos(angle), sa = std::sin(angle);
    const bool colored = v[2] >= 0.5;
    const Plane blurred = gaussianBlur(job, planeFrom(src), cell * 0.35);

    // The dots live on a grid in the rotated frame: find the cells the picture touches and work out
    // each one's dot (size and colour) once, instead of nine times per pixel.
    const double corners[4][2] = {{0, 0}, {src.width() - 1.0, 0}, {0, src.height() - 1.0}, {src.width() - 1.0, src.height() - 1.0}};
    double umin = 1e300, umax = -1e300, wmin = 1e300, wmax = -1e300;
    for (const auto &c : corners) {
        const double u = c[0] * ca + c[1] * sa, w = -c[0] * sa + c[1] * ca;
        umin = std::min(umin, u);
        umax = std::max(umax, u);
        wmin = std::min(wmin, w);
        wmax = std::max(wmax, w);
    }
    const int ci0 = static_cast<int>(std::floor(umin / cell)) - 1, ci1 = static_cast<int>(std::floor(umax / cell)) + 1;
    const int cj0 = static_cast<int>(std::floor(wmin / cell)) - 1, cj1 = static_cast<int>(std::floor(wmax / cell)) + 1;
    const int gw = ci1 - ci0 + 1, gh = cj1 - cj0 + 1;
    struct Dot {
        double radius;
        uint8_t color[3];
    };
    std::vector<Dot> dots(size_t(gw) * size_t(gh));
    rows(job, gh, [&](int j) {
        for (int i = 0; i < gw; ++i) {
            const double cu = (ci0 + i + 0.5) * cell;
            const double cv = (cj0 + j + 0.5) * cell;
            const int px = clampi(static_cast<int>(std::lround(cu * ca - cv * sa)), 0, src.width() - 1);
            const int py = clampi(static_cast<int>(std::lround(cu * sa + cv * ca)), 0, src.height() - 1);
            const uint8_t *p = blurred.row(py) + size_t(px) * 4;
            const double lum = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
            Dot &dot = dots[size_t(j) * gw + i];
            dot.radius = cell * 0.72 * std::sqrt(1.0 - lum / 255.0);
            for (int c = 0; c < 3; ++c)
                dot.color[c] = colored ? p[c] : 0;
        }
    });

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double u = x * ca + y * sa;
            const double w = -x * sa + y * ca;
            const int ci = static_cast<int>(std::floor(u / cell));
            const int cj = static_cast<int>(std::floor(w / cell));
            double cover = 0.0;
            const uint8_t *col = nullptr;
            for (int dj = -1; dj <= 1; ++dj)
                for (int di = -1; di <= 1; ++di) {
                    const Dot &dot = dots[size_t(cj + dj - cj0) * gw + size_t(ci + di - ci0)];
                    const double du = u - (ci + di + 0.5) * cell;
                    const double dv = w - (cj + dj + 0.5) * cell;
                    const double reach = dot.radius + 0.5;
                    const double dist2 = du * du + dv * dv;
                    if (dist2 >= reach * reach)
                        continue; // outside this dot, the usual case
                    const double c = clampd(reach - std::sqrt(dist2), 0.0, 1.0);
                    if (c > cover) {
                        cover = c;
                        col = dot.color;
                    }
                }
            for (int c = 0; c < 3; ++c)
                d[x * 4 + c] = toByte(255.0 * (1.0 - cover) + (col ? col[c] : 0) * cover);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// --- Stylizing (premultiplied) ----------------------------------------------

Plane fxPixelate(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    const int block = std::max(2, static_cast<int>(std::lround(longSide * (0.003 + v[0] / 100.0 * 0.057))));
    Plane out(src.w, src.h, 4);
    const int blockRows = (src.h + block - 1) / block;
    bands(job, blockRows, 1, [&](int r0, int r1) {
        for (int br = r0; br < r1; ++br) {
            const int y0 = br * block;
            const int y1 = std::min(src.h, y0 + block);
            for (int x0 = 0; x0 < src.w; x0 += block) {
                const int x1 = std::min(src.w, x0 + block);
                long long sum[4] = {0, 0, 0, 0};
                for (int y = y0; y < y1; ++y) {
                    const uint8_t *s = src.row(y);
                    for (int x = x0; x < x1; ++x)
                        for (int c = 0; c < 4; ++c)
                            sum[c] += s[size_t(x) * 4 + c];
                }
                const long long n = static_cast<long long>(y1 - y0) * (x1 - x0);
                uint8_t avg[4];
                for (int c = 0; c < 4; ++c)
                    avg[c] = static_cast<uint8_t>((sum[c] + n / 2) / n);
                for (int y = y0; y < y1; ++y) {
                    uint8_t *d = out.row(y);
                    for (int x = x0; x < x1; ++x)
                        for (int c = 0; c < 4; ++c)
                            d[size_t(x) * 4 + c] = avg[c];
                }
            }
        }
    });
    return out;
}

// Stained-glass cells: a jittered grid of seed points, every pixel takes the
// color the picture has at its nearest seed.
Plane fxCrystallize(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    const double cell = std::max(3.0, longSide * (0.006 + v[0] / 100.0 * 0.050));
    const Plane soft = gaussianBlur(job, src, cell * 0.25);
    const int nx = static_cast<int>(std::ceil(src.w / cell)) + 2;
    const int ny = static_cast<int>(std::ceil(src.h / cell)) + 2;
    struct Seed { double x, y; uint8_t c[4]; };
    std::vector<Seed> seeds(size_t(nx) * ny);
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            Seed &s = seeds[size_t(j) * nx + i];
            const int gi = i - 1, gj = j - 1; // grid index, one cell of margin all around
            s.x = (gi + 0.15 + 0.70 * hashUnit(hash2(gi, gj, 1))) * cell;
            s.y = (gj + 0.15 + 0.70 * hashUnit(hash2(gi, gj, 2))) * cell;
            const int px = clampi(static_cast<int>(std::lround(s.x)), 0, src.w - 1);
            const int py = clampi(static_cast<int>(std::lround(s.y)), 0, src.h - 1);
            const uint8_t *p = soft.row(py) + size_t(px) * 4;
            for (int c = 0; c < 4; ++c)
                s.c[c] = p[c];
        }

    Plane out(src.w, src.h, 4);
    rows(job, src.h, [&](int y) {
        uint8_t *d = out.row(y);
        const int cj = static_cast<int>(std::floor(y / cell)) + 1;
        for (int x = 0; x < src.w; ++x) {
            const int ci = static_cast<int>(std::floor(x / cell)) + 1;
            double best = 1e300;
            const Seed *pick = nullptr;
            for (int j = cj - 1; j <= cj + 1; ++j)
                for (int i = ci - 1; i <= ci + 1; ++i) {
                    const Seed &s = seeds[size_t(clampi(j, 0, ny - 1)) * nx + clampi(i, 0, nx - 1)];
                    const double dist = (s.x - x) * (s.x - x) + (s.y - y) * (s.y - y);
                    if (dist < best) {
                        best = dist;
                        pick = &s;
                    }
                }
            for (int c = 0; c < 4; ++c)
                d[size_t(x) * 4 + c] = pick->c[c];
        }
    });
    return out;
}

// --- Distortions ------------------------------------------------------------

Plane fxFisheye(const Job &job, const Plane &src, const EffectValues &v)
{
    const double amount = v[0] / 100.0;
    if (std::abs(amount) < 1e-6)
        return src;
    const double cx = (src.w - 1) * 0.5, cy = (src.h - 1) * 0.5;
    const double radius = 0.5 * std::hypot(src.w, src.h);
    // > 1 pulls the middle outward (bulge), < 1 pinches it; the very edge stays put.
    const double exponent = amount >= 0.0 ? 1.0 + 1.5 * amount : 1.0 / (1.0 + 1.5 * -amount);
    return remap(job, src, [&](int x, int y, double &sx, double &sy) {
        const double dx = x - cx, dy = y - cy;
        const double r = std::sqrt(dx * dx + dy * dy);
        if (r < 1e-9)
            return;
        const double from = radius * std::pow(std::min(r / radius, 1.5), exponent);
        sx = cx + dx * (from / r);
        sy = cy + dy * (from / r);
    });
}

Plane fxSwirl(const Job &job, const Plane &src, const EffectValues &v)
{
    const double angle = v[0] * kPi / 180.0;
    if (std::abs(angle) < 1e-6)
        return src;
    const double cx = (src.w - 1) * 0.5, cy = (src.h - 1) * 0.5;
    const double radius = std::max(1.0, v[1] / 100.0 * 0.5 * std::hypot(src.w, src.h));
    return remap(job, src, [&](int x, int y, double &sx, double &sy) {
        const double dx = x - cx, dy = y - cy;
        const double r = std::sqrt(dx * dx + dy * dy);
        if (r >= radius)
            return;
        const double t = 1.0 - r / radius;
        const double a = angle * t * t;
        const double ca = std::cos(a), sa = std::sin(a);
        sx = cx + dx * ca + dy * sa;
        sy = cy - dx * sa + dy * ca;
    });
}

Plane fxWave(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    const double amplitude = v[0] / 100.0 * 0.03 * longSide;
    if (amplitude < 0.05)
        return src;
    const double wavelength = std::max(4.0, longSide * (0.02 + v[1] / 100.0 * 0.40));
    const double direction = v[2] * kPi / 180.0;
    const double ca = std::cos(direction), sa = std::sin(direction);
    return remap(job, src, [&](int x, int y, double &sx, double &sy) {
        const double along = -x * sa + y * ca;
        const double shift = amplitude * std::sin(2.0 * kPi * along / wavelength);
        sx = x + shift * ca;
        sy = y + shift * sa;
    });
}

Plane fxFrosted(const Job &job, const Plane &src, const EffectValues &v, double longSide)
{
    const double spread = 1.0 + v[0] / 100.0 * 0.008 * longSide;
    if (v[0] <= 0.0)
        return src;
    return remap(job, src, [&](int x, int y, double &sx, double &sy) {
        sx = x + (hashUnit(hash2(x, y, 11)) - 0.5) * 2.0 * spread;
        sy = y + (hashUnit(hash2(x, y, 12)) - 0.5) * 2.0 * spread;
    });
}

// --- Negativo, Viñeta, Grano y ruido (straight colour; the alpha is never touched) ---

QImage fxNegative(const Job &job, const QImage &src)
{
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            d[x * 4] = 255 - s[x * 4];
            d[x * 4 + 1] = 255 - s[x * 4 + 1];
            d[x * 4 + 2] = 255 - s[x * 4 + 2];
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

inline double smoothStep01(double t)
{
    t = clampd(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Cantidad (- lightens / + darkens the edges), Tamaño (how far the clear middle reaches),
// Suavidad (how gradually it fades), Redondez (- toward a rectangle, 0 follows the picture's
// shape, + toward a circle) and the Centro X / Y it is drawn around.
QImage fxVignette(const Job &job, const QImage &src, const EffectValues &v)
{
    const double amount = v[0] / 100.0;
    if (amount == 0.0)
        return src;
    const double r0 = v[1] / 100.0 * 1.1;
    const double r1 = r0 + 0.05 + v[2] / 100.0;
    const double round = v[3] / 100.0;
    const int w = src.width(), h = src.height();
    const double hw = std::max(1.0, w * 0.5), hh = std::max(1.0, h * 0.5);
    const double cx = hw + v[4] / 100.0 * hw;
    const double cy = hh + v[5] / 100.0 * hh;
    const double cornerDist = std::sqrt(hw * hw + hh * hh);

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, h, [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        const double ny = (y + 0.5 - cy) / hh;
        for (int x = 0; x < w; ++x) {
            const double nx = (x + 0.5 - cx) / hw;
            const double ellipse = std::sqrt(nx * nx + ny * ny) * 0.7071067811865476;
            double dist = ellipse;
            if (round > 0.0) {
                const double circle = std::sqrt((nx * hw) * (nx * hw) + (ny * hh) * (ny * hh)) / cornerDist;
                dist = ellipse + (circle - ellipse) * round;
            } else if (round < 0.0) {
                const double box = std::max(std::abs(nx), std::abs(ny));
                dist = ellipse + (box - ellipse) * -round;
            }
            const double mask = smoothStep01((dist - r0) / (r1 - r0));
            for (int c = 0; c < 3; ++c) {
                const double p = s[x * 4 + c];
                d[x * 4 + c] = toByte(amount >= 0.0 ? p * (1.0 - amount * mask) : p + (255.0 - p) * -amount * mask);
            }
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// Cantidad, Tamaño (the grain gets coarser: every dot covers a block of pixels, a block
// size that follows the picture's long side), Tipo (see NoiseType) and Monocromático.
QImage fxGrain(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const double amount = v[0] / 100.0;
    if (amount <= 0.0)
        return src;
    const int cell = 1 + static_cast<int>(std::lround(v[1] / 100.0 * longSide * 0.004));
    const int type = clampi(static_cast<int>(std::lround(v[2])), 0, 3);
    const bool mono = v[3] >= 0.5;

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            double c[3] = {s[x * 4] / 255.0, s[x * 4 + 1] / 255.0, s[x * 4 + 2] / 255.0};
            applyUserNoise(x / cell, y / cell, amount, type, mono, c);
            for (int i = 0; i < 3; ++i)
                d[x * 4 + i] = toByte(c[i] * 255.0);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// --- Catalogue ----------------------------------------------------------------

EffectParam slider(const char *label, double lo, double hi, double def, const char *suffix = "%", bool integer = false)
{
    EffectParam p;
    p.label = QString::fromUtf8(label);
    p.min = lo;
    p.max = hi;
    p.def = def;
    p.suffix = QString::fromUtf8(suffix);
    p.integer = integer;
    return p;
}

EffectParam choice(const char *label, QStringList options, int def)
{
    EffectParam p;
    p.label = QString::fromUtf8(label);
    p.min = 0.0;
    p.max = double(options.size() - 1);
    p.def = def;
    p.integer = true;
    p.options = std::move(options);
    return p;
}

EffectParam toggle(const char *label, bool def = false)
{
    EffectParam p;
    p.label = QString::fromUtf8(label);
    p.min = 0.0;
    p.max = 1.0;
    p.def = def ? 1.0 : 0.0;
    p.integer = true;
    p.toggle = true;
    return p;
}

std::vector<EffectSpec> buildCatalogue()
{
    auto spec = [](const char *id, const char *name, const char *group, std::vector<EffectParam> params) {
        EffectSpec s;
        s.fullSize = std::strcmp(id, "denoise") == 0; // noise is a property of the real pixels
        s.id = QString::fromUtf8(id);
        s.name = QString::fromUtf8(name);
        s.group = QString::fromUtf8(group);
        s.params = std::move(params);
        return s;
    };
    return {
        spec("blur", "Gaussiano", "blur", {slider("Radio", 0, 100, 60)}),
        spec("motion", "Movimiento", "blur",
             {slider("Distancia", 0, 100, 50), slider("Ángulo", -90, 90, 0, "°", true)}),
        spec("zoom", "Zoom", "blur",
             {slider("Cantidad", 0, 100, 60), slider("Centro X", -100, 100, 0), slider("Centro Y", -100, 100, 0)}),
        spec("spin", "Giratorio", "blur",
             {slider("Cantidad", 0, 100, 60), slider("Centro X", -100, 100, 0), slider("Centro Y", -100, 100, 0)}),
        spec("denoise", "Quitar ruido", "blur", {slider("Luminancia", 0, 100, 50), slider("Color", 0, 100, 60)}),
        spec("median", "Mediana", "blur", {slider("Radio", 1, 2, 1, "", true)}),

        spec("posterize", "Posterizar", "style", {slider("Niveles", 2, 16, 4, "", true)}),
        spec("threshold", "Umbral", "style", {slider("Nivel", 0, 255, 128, "", true)}),
        spec("emboss", "Relieve", "style",
             {slider("Profundidad", 0, 100, 50), slider("Ángulo", -180, 180, 135, "°", true), slider("Color", 0, 100, 0)}),
        spec("edges", "Bordes", "style", {slider("Intensidad", 0, 100, 50), toggle("Fondo claro")}),
        spec("sketch", "Lápiz", "style", {slider("Trazo", 0, 100, 30), slider("Color", 0, 100, 0)}),
        spec("halftone", "Semitono", "style",
             {slider("Tamaño", 0, 100, 35), slider("Ángulo", -90, 90, 45, "°", true), toggle("Color")}),
        spec("pixelate", "Mosaico", "style", {slider("Tamaño", 0, 100, 40)}),
        spec("crystallize", "Cristalizar", "style", {slider("Tamaño", 0, 100, 40)}),
        spec("negative", "Negativo", "style", {}),

        spec("fisheye", "Ojo de pez", "distort", {slider("Cantidad", -100, 100, 40)}),
        spec("swirl", "Remolino", "distort",
             {slider("Ángulo", -720, 720, 180, "°", true), slider("Radio", 10, 100, 70)}),
        spec("wave", "Onda", "distort",
             {slider("Amplitud", 0, 100, 40), slider("Longitud", 5, 100, 30), slider("Dirección", 0, 90, 0, "°", true)}),
        spec("frosted", "Vidrio", "distort", {slider("Cantidad", 0, 100, 55)}),

        spec("vignette", "Viñeta", "finish",
             {slider("Cantidad", -100, 100, 50), slider("Tamaño", 0, 100, 50), slider("Suavidad", 0, 100, 50),
              slider("Redondez", -100, 100, 0), slider("Centro X", -100, 100, 0), slider("Centro Y", -100, 100, 0)}),
        spec("grain", "Grano y ruido", "finish",
             {slider("Cantidad", 0, 100, 40), slider("Tamaño", 0, 100, 0),
              choice("Tipo", {"Uniforme", "Gaussiano", "Impulso", "Laplaciano"}, 1), toggle("Monocromático")}),
    };
}

// Effects that mix neighbors (blurs, remaps, cells) work on premultiplied color
// so a transparent pixel's hidden color cannot bleed into its neighbors.
bool usesPremultiplied(const QString &id)
{
    static const char *const ids[] = {"blur", "motion", "zoom", "spin", "median", "pixelate", "crystallize",
                                      "fisheye", "swirl", "wave", "frosted"};
    for (const char *name : ids)
        if (id == QLatin1String(name))
            return true;
    return false;
}

} // namespace

const std::vector<EffectGroup> &effectGroups()
{
    static const std::vector<EffectGroup> groups = {
        {QStringLiteral("blur"), QStringLiteral("Desenfoque")},
        {QStringLiteral("style"), QStringLiteral("Estilo")},
        {QStringLiteral("distort"), QStringLiteral("Distorsión")},
        {QStringLiteral("finish"), QStringLiteral("Acabado")},
    };
    return groups;
}

const std::vector<EffectSpec> &allEffects()
{
    static const std::vector<EffectSpec> catalogue = buildCatalogue();
    return catalogue;
}

const EffectSpec *findEffect(const QString &id)
{
    for (const EffectSpec &spec : allEffects())
        if (spec.id == id)
            return &spec;
    return nullptr;
}

EffectValues defaultEffectValues(const EffectSpec &spec)
{
    EffectValues values{};
    for (size_t i = 0; i < spec.params.size() && i < values.size(); ++i)
        values[i] = spec.params[i].def;
    return values;
}

EffectValues sampleEffectValues(const EffectSpec &spec)
{
    EffectValues v = defaultEffectValues(spec);
    if (spec.id == QLatin1String("blur") || spec.id == QLatin1String("motion") || spec.id == QLatin1String("frosted")
        || spec.id == QLatin1String("crystallize"))
        v[0] = 100.0;
    else if (spec.id == QLatin1String("pixelate"))
        v[0] = 70.0;
    else if (spec.id == QLatin1String("median"))
        v[0] = 2.0;
    else if (spec.id == QLatin1String("wave"))
        v[0] = 100.0;
    return v;
}

QImage applyEffect(const QImage &source, const QString &id, const EffectValues &values, double mix,
                   const std::atomic<bool> *cancel)
{
    const Job job{cancel};
    const EffectSpec *spec = findEffect(id);
    if (!spec || source.isNull() || mix <= 0.0)
        return source;
    if (source.width() < 2 || source.height() < 2)
        return source;

    // Keep every slider inside its documented range, whatever the caller sent.
    EffectValues v = values;
    for (size_t i = 0; i < spec->params.size() && i < v.size(); ++i)
        v[i] = clampd(v[i], spec->params[i].min, spec->params[i].max);

    const QImage straight = source.format() == QImage::Format_RGBA8888
                                ? source
                                : source.convertToFormat(QImage::Format_RGBA8888);
    const double longSide = std::max(straight.width(), straight.height());

    QImage result;
    if (usesPremultiplied(id)) {
        const Plane src = planeFrom(straight.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
        Plane out;
        if (id == QLatin1String("blur")) out = fxGaussian(job, src, v, longSide);
        else if (id == QLatin1String("motion")) out = fxMotion(job, src, v, longSide);
        else if (id == QLatin1String("zoom")) out = fxRadialBlur(job, src, v, false);
        else if (id == QLatin1String("spin")) out = fxRadialBlur(job, src, v, true);
        else if (id == QLatin1String("median")) out = fxMedian(job, src, v);
        else if (id == QLatin1String("pixelate")) out = fxPixelate(job, src, v, longSide);
        else if (id == QLatin1String("crystallize")) out = fxCrystallize(job, src, v, longSide);
        else if (id == QLatin1String("fisheye")) out = fxFisheye(job, src, v);
        else if (id == QLatin1String("swirl")) out = fxSwirl(job, src, v);
        else if (id == QLatin1String("wave")) out = fxWave(job, src, v, longSide);
        else out = fxFrosted(job, src, v, longSide);
        result = imageFrom(out, QImage::Format_RGBA8888_Premultiplied).convertToFormat(QImage::Format_RGBA8888);
    } else {
        if (id == QLatin1String("posterize")) result = fxPosterize(job, straight, v);
        else if (id == QLatin1String("threshold")) result = fxThreshold(job, straight, v);
        else if (id == QLatin1String("emboss")) result = fxEmboss(job, straight, v);
        else if (id == QLatin1String("edges")) result = fxEdges(job, straight, v);
        else if (id == QLatin1String("sketch")) result = fxSketch(job, straight, v, longSide);
        else if (id == QLatin1String("negative")) result = fxNegative(job, straight);
        else if (id == QLatin1String("denoise")) result = denoiseColor(straight, v[0], v[1], cancel);
        else if (id == QLatin1String("vignette")) result = fxVignette(job, straight, v);
        else if (id == QLatin1String("grain")) result = fxGrain(job, straight, v, longSide);
        else result = fxHalftone(job, straight, v, longSide);
    }

    if (mix >= 1.0)
        return result;

    // Blend toward the untouched picture.
    const float t = static_cast<float>(mix);
    result.bits();
    rows(job, result.height(), [&](int y) {
        const uint8_t *s = straight.constScanLine(y);
        uint8_t *d = result.scanLine(y);
        for (int i = 0, n = result.width() * 4; i < n; ++i)
            d[i] = toByte(s[i] + (d[i] - s[i]) * t);
    });
    return result;
}

} // namespace core::edit
