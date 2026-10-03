#include "AdjustMath.h"
#include "ParallelRows.h"
#include "../Histogram.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace core::edit {

namespace {

constexpr double kLumaR = 0.299;
constexpr double kLumaG = 0.587;
constexpr double kLumaB = 0.114;

double clamp01(double v)
{
    return std::clamp(v, 0.0, 1.0);
}

quint8 toByte(double v)
{
    return static_cast<quint8>(clamp01(v) * 255.0 + 0.5);
}

// --- Curves ---------------------------------------------------------------

// Monotone cubic Hermite through the control points (the classic PCHIP slope
// rule): it follows the points smoothly but, unlike a plain cubic spline,
// never overshoots between two of them, so dragging one point can't make the
// curve dip below the next.
class Spline {
public:
    explicit Spline(const CurvePoints &pts)
    {
        if (isIdentityCurve(pts))
            return;

        std::vector<QPointF> sorted(pts.begin(), pts.end());
        std::sort(sorted.begin(), sorted.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
        for (const QPointF &p : sorted) {
            if (!m_x.empty() && p.x() - m_x.back() < 1e-4)
                continue; // two points on the same x: keep the first
            m_x.push_back(clamp01(p.x()));
            m_y.push_back(clamp01(p.y()));
        }
        if (m_x.size() < 2) {
            m_x.clear();
            m_y.clear();
            return;
        }

        const size_t n = m_x.size();
        std::vector<double> h(n - 1), d(n - 1);
        for (size_t i = 0; i + 1 < n; ++i) {
            h[i] = m_x[i + 1] - m_x[i];
            d[i] = (m_y[i + 1] - m_y[i]) / h[i];
        }
        m_m.assign(n, 0.0);
        m_m[0] = d[0];
        m_m[n - 1] = d[n - 2];
        for (size_t i = 1; i + 1 < n; ++i) {
            if (d[i - 1] * d[i] > 0.0) {
                const double w1 = 2.0 * h[i] + h[i - 1];
                const double w2 = h[i] + 2.0 * h[i - 1];
                m_m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i]);
            }
        }
        m_identity = false;
    }

    bool isIdentity() const { return m_identity; }

    double eval(double v) const
    {
        if (m_identity)
            return v;
        if (v <= m_x.front())
            return m_y.front();
        if (v >= m_x.back())
            return m_y.back();

        const size_t i = static_cast<size_t>(std::upper_bound(m_x.begin(), m_x.end(), v) - m_x.begin()) - 1;
        const double h = m_x[i + 1] - m_x[i];
        const double t = (v - m_x[i]) / h;
        const double t2 = t * t;
        const double t3 = t2 * t;
        const double result = (2 * t3 - 3 * t2 + 1) * m_y[i] + (t3 - 2 * t2 + t) * h * m_m[i]
            + (-2 * t3 + 3 * t2) * m_y[i + 1] + (t3 - t2) * h * m_m[i + 1];
        return clamp01(result);
    }

private:
    std::vector<double> m_x, m_y, m_m;
    bool m_identity = true;
};

double applyLevels(double v, const LevelsChannel &lv)
{
    if (lv.isIdentity())
        return v;
    const double range = std::max(lv.inWhite - lv.inBlack, 1e-4);
    const double t = clamp01((v - lv.inBlack) / range);
    return std::pow(t, 1.0 / std::max(lv.gamma, 0.01));
}

// --- Stage B: per-pixel color mixing --------------------------------------

class ColorMix {
public:
    explicit ColorMix(const AdjustOp &op)
        : m_shadows(op.shadows)
        , m_highlights(op.highlights)
        , m_saturation(op.saturation)
        , m_vibrance(op.vibrance)
        , m_hue(op.hue)
        , m_negative(op.negative)
    {
        if (m_hue != 0.0) {
            // The standard hue-rotate matrix (as in the CSS `hue-rotate()`
            // filter): a rotation about the gray axis that keeps luminance.
            const double a = m_hue * std::numbers::pi;
            const double c = std::cos(a);
            const double s = std::sin(a);
            m_hueM = {
                0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928,
                0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.140, 0.072 - c * 0.072 - s * 0.283,
                0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072,
            };
        }
    }

    bool active() const
    {
        return m_shadows != 0.0 || m_highlights != 0.0 || m_saturation != 0.0 || m_vibrance != 0.0
            || m_hue != 0.0 || m_negative;
    }

    // r, g, b in 0..1.
    void apply(double &r, double &g, double &b) const
    {
        if (m_shadows != 0.0 || m_highlights != 0.0) {
            const double l = kLumaR * r + kLumaG * g + kLumaB * b;
            const double ts = clamp01(1.0 - 2.0 * l);
            const double th = clamp01(2.0 * l - 1.0);
            const double add = m_shadows * 0.40 * (ts * std::sqrt(ts)) + m_highlights * 0.40 * (th * std::sqrt(th));
            r += add;
            g += add;
            b += add;
        }
        if (m_saturation != 0.0) {
            const double l = kLumaR * r + kLumaG * g + kLumaB * b;
            const double f = 1.0 + m_saturation;
            r = l + (r - l) * f;
            g = l + (g - l) * f;
            b = l + (b - l) * f;
        }
        if (m_vibrance != 0.0) {
            // Pushes dull colors harder than already-vivid ones, so it
            // livens a photo without blowing out what was saturated.
            const double chroma = clamp01(std::max({r, g, b}) - std::min({r, g, b}));
            const double f = 1.0 + m_vibrance * (1.0 - chroma);
            const double l = kLumaR * r + kLumaG * g + kLumaB * b;
            r = l + (r - l) * f;
            g = l + (g - l) * f;
            b = l + (b - l) * f;
        }
        if (m_hue != 0.0) {
            const double nr = m_hueM[0] * r + m_hueM[1] * g + m_hueM[2] * b;
            const double ng = m_hueM[3] * r + m_hueM[4] * g + m_hueM[5] * b;
            const double nb = m_hueM[6] * r + m_hueM[7] * g + m_hueM[8] * b;
            r = nr;
            g = ng;
            b = nb;
        }
        r = clamp01(r);
        g = clamp01(g);
        b = clamp01(b);
        if (m_negative) {
            r = 1.0 - r;
            g = 1.0 - g;
            b = 1.0 - b;
        }
    }

private:
    double m_shadows, m_highlights, m_saturation, m_vibrance, m_hue;
    bool m_negative;
    std::array<double, 9> m_hueM{};
};

// --- Finishing: vignette and grain ----------------------------------------

// How hard the pixel whose CENTER is at (px, py) is pulled into the vignette:
// 0 in the middle, growing with the square of the distance to the corners
// (0.85 at the very corner).
double vignetteMask(double px, double py, int width, int height)
{
    const double cx = width * 0.5;
    const double cy = height * 0.5;
    const double maxDist = std::max(std::sqrt(cx * cx + cy * cy), 1.0);
    const double d = clamp01(std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy)) / maxDist);
    return d * d * 0.85;
}

// amount > 0 darkens toward the corners, < 0 lightens them.
void applyVignette(double amount, double mask, double c[3])
{
    for (int i = 0; i < 3; ++i)
        c[i] = amount >= 0.0 ? c[i] * (1.0 - amount * mask) : c[i] + (1.0 - c[i]) * (-amount) * mask;
}

// Noise / grain. Every pixel gets its own random value(s), drawn from the
// chosen distribution and added on top of the picture:
//   Uniform / Gaussian / Laplacian - a zero-mean deviation scaled by the amount,
//     weaker in the deepest shadows and brightest highlights (where it would
//     only be clipped away and drag the average brightness with it);
//   Impulse - the given share of pixels is forced to pure black or pure white.
// Monochrome noise uses one value for red, green and blue; otherwise each
// channel draws its own, which reads as color noise.
//
// The random numbers come from an integer hash of (x, y, channel) on purpose:
// Detail.frag and Grade.frag spell out the identical hash, so the preview and
// the saved file get exactly the same noise (a float hash would drift between
// CPU and GPU). The distribution transforms use plain float math on both sides.
constexpr double kNoiseScaleUser = 0.20; // deviation at amount 1, as a share of full scale
constexpr double kNoiseScaleLook = 0.09; // the looks' film grain is a lot subtler
constexpr double kImpulseShare = 0.12;   // share of pixels hit at amount 1

uint32_t noiseHash(int x, int y, uint32_t channel)
{
    uint32_t h = static_cast<uint32_t>(x) * 0x9E3779B1u + static_cast<uint32_t>(y) * 0x85EBCA77u
                 + channel * 0xC2B2AE3Du;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// Zero mean, unit variance draw from `h` (Impulse is handled separately).
double noiseSample(int type, uint32_t h)
{
    switch (type) {
    case static_cast<int>(NoiseType::Uniform): {
        const double u = double(h & 0xFFFFu) / 65535.0;
        return (2.0 * u - 1.0) * 1.7320508075688772;
    }
    case static_cast<int>(NoiseType::Laplacian): {
        const double v = (double(h & 0xFFFFu) + 0.5) / 65536.0 - 0.5;
        const double a = 1.0 - 2.0 * std::abs(v);
        return (v < 0.0 ? 1.0 : -1.0) * std::log(a) * 0.7071067811865476;
    }
    default: { // Gaussian, by the Box-Muller transform
        const double u1 = (double(h & 0xFFFFu) + 1.0) / 65536.0;
        const double u2 = double(h >> 16) / 65536.0;
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }
    }
}

void applyNoise(int x, int y, double amount, double scale, int type, bool mono, double c[3])
{
    if (type == static_cast<int>(NoiseType::Impulse)) {
        const double share = amount * kImpulseShare;
        for (int i = 0; i < 3; ++i) {
            const double u = (double(noiseHash(x, y, mono ? 0u : uint32_t(i)) & 0xFFFFu) + 0.5) / 65536.0;
            if (u < share * 0.5)
                c[i] = 0.0;
            else if (u < share)
                c[i] = 1.0;
        }
        return;
    }
    const double l = kLumaR * c[0] + kLumaG * c[1] + kLumaB * c[2];
    const double sigma = amount * scale * (0.6 + 0.4 * (1.0 - (2.0 * l - 1.0) * (2.0 * l - 1.0)));
    if (mono) {
        const double n = noiseSample(type, noiseHash(x, y, 0u)) * sigma;
        for (int i = 0; i < 3; ++i)
            c[i] += n;
    } else {
        for (int i = 0; i < 3; ++i)
            c[i] += noiseSample(type, noiseHash(x, y, uint32_t(i))) * sigma;
    }
}

// --- Stage C: clarity + sharpen -------------------------------------------

// Clarity's blur radius scales with the image so the look is the same on a
// thumbnail and on the full-size file: 2^lod pixels across, ~2% of the long
// side. The shader computes this exact expression, and reads the blur from the
// matching mip level of the adjusted texture.
int clarityLod(int width, int height)
{
    const double d = std::max(width, height) / 48.0;
    if (d <= 1.0)
        return 2;
    return std::clamp(static_cast<int>(std::floor(std::log2(d) + 0.5)), 2, 7);
}

// The image averaged down by 2^lod in each direction - what the GPU's
// mipmap chain holds at that level - so clarity can compare each pixel with
// its neighborhood.
struct BlurGrid {
    int width = 1;
    int height = 1;
    std::vector<float> rgb; // width * height * 3, 0..1

    void sample(double u, double v, double out[3]) const
    {
        // Bilinear between texel centers, clamped at the edges - the same
        // sampling the GPU does for a mip level.
        const double gx = std::clamp(u * width - 0.5, 0.0, double(width - 1));
        const double gy = std::clamp(v * height - 0.5, 0.0, double(height - 1));
        const int x0 = static_cast<int>(gx);
        const int y0 = static_cast<int>(gy);
        const int x1 = std::min(x0 + 1, width - 1);
        const int y1 = std::min(y0 + 1, height - 1);
        const double fx = gx - x0;
        const double fy = gy - y0;
        for (int c = 0; c < 3; ++c) {
            const double top = rgb[(size_t(y0) * width + x0) * 3 + c] * (1.0 - fx) + rgb[(size_t(y0) * width + x1) * 3 + c] * fx;
            const double bottom = rgb[(size_t(y1) * width + x0) * 3 + c] * (1.0 - fx) + rgb[(size_t(y1) * width + x1) * 3 + c] * fx;
            out[c] = top * (1.0 - fy) + bottom * fy;
        }
    }
};

BlurGrid buildBlurGrid(const QImage &img, int lod)
{
    BlurGrid grid;
    grid.width = std::max(1, img.width() >> lod);
    grid.height = std::max(1, img.height() >> lod);
    grid.rgb.assign(size_t(grid.width) * grid.height * 3, 0.0f);

    // Each cell averages the slice of the image it covers when the whole
    // picture is squeezed into width x height cells. For non-power-of-two
    // sizes that is what the GPU's mip generation on this machine turned out
    // to match best (measured against the live preview) - a plain "aligned
    // 2^lod blocks" layout drifted noticeably further from it.
    forEachRowParallel(grid.height, [&](int gy) {
        const int y0 = static_cast<int>(int64_t(gy) * img.height() / grid.height);
        const int y1 = std::max(y0 + 1, static_cast<int>(int64_t(gy + 1) * img.height() / grid.height));
        for (int gx = 0; gx < grid.width; ++gx) {
            const int x0 = static_cast<int>(int64_t(gx) * img.width() / grid.width);
            const int x1 = std::max(x0 + 1, static_cast<int>(int64_t(gx + 1) * img.width() / grid.width));
            // Premultiplied (colour weighted by its own alpha), like the GPU's mip
            // levels of a Qt Quick texture: a transparent pixel adds nothing.
            double sum[3] = {0.0, 0.0, 0.0};
            for (int y = y0; y < y1; ++y) {
                const quint8 *line = img.constScanLine(y);
                for (int x = x0; x < x1; ++x) {
                    const double alpha = line[x * 4 + 3];
                    sum[0] += line[x * 4] * alpha;
                    sum[1] += line[x * 4 + 1] * alpha;
                    sum[2] += line[x * 4 + 2] * alpha;
                }
            }
            const double n = double(y1 - y0) * double(x1 - x0) * 255.0 * 255.0;
            float *dst = &grid.rgb[(size_t(gy) * grid.width + gx) * 3];
            dst[0] = static_cast<float>(sum[0] / n);
            dst[1] = static_cast<float>(sum[1] / n);
            dst[2] = static_cast<float>(sum[2] / n);
        }
    });
    return grid;
}

// Clarity (local contrast, strongest in the midtones so blacks and whites
// don't halo) + the 5-tap sharpen, both computed from the SAME adjusted pixels
// and added together, then the vignette and the grain on top - exactly what
// Detail.frag does.
//
// Pictures with transparency: the neighbourhood maths (sharpen, clarity) runs on
// PREMULTIPLIED colour - each pixel's colour weighted by its own alpha, so a
// transparent neighbour counts for nothing and no halo of its hidden colour
// bleeds in - and the result goes back to straight colour for the vignette and
// grain, which are per-pixel colour changes. Detail.frag does the same on the
// premultiplied texture Qt Quick gives it. With alpha 255 everywhere (the usual
// case) every value is what it always was.
QImage applyDetail(const QImage &adjusted, double clarity, double sharpness, double vignette, double grain,
                   int grainType, bool grainMono)
{
    const int width = adjusted.width();
    const int height = adjusted.height();
    QImage result(adjusted.size(), QImage::Format_RGBA8888);
    result.bits(); // force allocation before the worker threads touch it

    const double amount = clamp01(sharpness);
    const bool useClarity = clarity != 0.0;
    const bool finishing = vignette != 0.0 || grain > 0.0;
    BlurGrid grid;
    if (useClarity)
        grid = buildBlurGrid(adjusted, clarityLod(width, height));

    forEachRowParallel(height, [&](int y) {
        const quint8 *lineC = adjusted.constScanLine(y);
        const quint8 *lineU = adjusted.constScanLine(std::max(0, y - 1));
        const quint8 *lineD = adjusted.constScanLine(std::min(height - 1, y + 1));
        quint8 *out = result.scanLine(y);
        for (int x = 0; x < width; ++x) {
            const int xc = x * 4;
            const int xl = std::max(0, x - 1) * 4;
            const int xr = std::min(width - 1, x + 1) * 4;

            out[xc + 3] = lineC[xc + 3]; // alpha is never touched
            if (lineC[xc + 3] == 0) { // invisible: nothing to improve, keep the colour it had
                out[xc] = lineC[xc];
                out[xc + 1] = lineC[xc + 1];
                out[xc + 2] = lineC[xc + 2];
                continue;
            }

            // premultiplied colour of a pixel of `line` at byte offset `o`
            auto premultiplied = [](const quint8 *line, int o, int i) { return line[o + i] * line[o + 3] / 65025.0; };

            const double alpha = lineC[xc + 3] / 255.0;
            double own[3]; // straight colour
            double c[3];   // premultiplied colour
            for (int i = 0; i < 3; ++i) {
                own[i] = lineC[xc + i] / 255.0;
                c[i] = premultiplied(lineC, xc, i);
            }
            double value[3] = {c[0], c[1], c[2]};

            if (amount > 0.0) {
                for (int i = 0; i < 3; ++i) {
                    const double sum = premultiplied(lineC, xl, i) + premultiplied(lineC, xr, i)
                        + premultiplied(lineU, xc, i) + premultiplied(lineD, xc, i);
                    value[i] += amount * (c[i] * 4.0 - sum);
                }
            }
            if (useClarity) {
                double blur[3];
                grid.sample((x + 0.5) / width, (y + 0.5) / height, blur);
                const double l = kLumaR * own[0] + kLumaG * own[1] + kLumaB * own[2];
                const double midtones = 1.0 - (2.0 * l - 1.0) * (2.0 * l - 1.0);
                for (int i = 0; i < 3; ++i)
                    value[i] += clarity * 1.2 * midtones * (c[i] - blur[i]);
            }
            // back to straight colour (a premultiplied value cannot exceed its alpha)
            for (double &v : value)
                v = clamp01(v / alpha);
            if (finishing) {
                if (vignette != 0.0)
                    applyVignette(vignette, vignetteMask(x + 0.5, y + 0.5, width, height), value);
                if (grain > 0.0)
                    applyNoise(x, y, grain, kNoiseScaleUser, grainType, grainMono, value);
            }

            out[xc] = toByte(value[0]);
            out[xc + 1] = toByte(value[1]);
            out[xc + 2] = toByte(value[2]);
        }
    });
    return result;
}

// --- Look extras ----------------------------------------------------------

void applyExtras(const GradeExtras &e, int x, int y, int width, int height, double c[3])
{
    if (e.gradient) {
        const double l = kLumaR * c[0] + kLumaG * c[1] + kLumaB * c[2];
        const auto &from = l < 0.5 ? e.stops[0] : e.stops[1];
        const auto &to = l < 0.5 ? e.stops[1] : e.stops[2];
        const double t = l < 0.5 ? l * 2.0 : (l - 0.5) * 2.0;
        for (int i = 0; i < 3; ++i)
            c[i] = from[i] + (to[i] - from[i]) * t;
    }
    {
        const double l = kLumaR * c[0] + kLumaG * c[1] + kLumaB * c[2];
        const double shadowWeight = (1.0 - l) * (1.0 - l);
        const double highlightWeight = l * l;
        for (int i = 0; i < 3; ++i)
            c[i] = clamp01(c[i] + e.shadowOffset[i] * shadowWeight + e.highlightOffset[i] * highlightWeight);
    }
    if (e.vignette != 0.0)
        applyVignette(e.vignette, vignetteMask(x + 0.5, y + 0.5, width, height), c);
    if (e.grain > 0.0)
        applyNoise(x, y, e.grain, kNoiseScaleLook, e.grainType, e.grainMono, c);
    for (int i = 0; i < 3; ++i)
        c[i] = clamp01(c[i]);
}

// --- Auto adjustments -----------------------------------------------------

// The first and last bucket that still hold something once the outermost
// `clip` fraction of pixels at each end is ignored - a few stray hot pixels
// shouldn't be allowed to veto a stretch.
std::pair<int, int> trimmedRange(const std::array<int, 256> &bins, double clip)
{
    long long total = 0;
    for (int n : bins)
        total += n;
    if (total <= 0)
        return {0, 255};

    const long long limit = static_cast<long long>(total * clip);
    long long acc = 0;
    int lo = 0;
    for (; lo < 255; ++lo) {
        acc += bins[lo];
        if (acc > limit)
            break;
    }
    acc = 0;
    int hi = 255;
    for (; hi > 0; --hi) {
        acc += bins[hi];
        if (acc > limit)
            break;
    }
    return {lo, hi};
}

double meanOf(const std::array<int, 256> &bins)
{
    double sum = 0.0;
    double count = 0.0;
    for (int i = 0; i < 256; ++i) {
        sum += double(i) * bins[i];
        count += bins[i];
    }
    return count > 0.0 ? sum / count / 255.0 : 0.5;
}

} // namespace

std::array<double, 256> sampleCurve(const CurvePoints &pts)
{
    const Spline spline(pts);
    std::array<double, 256> out{};
    for (int i = 0; i < 256; ++i)
        out[i] = spline.eval(i / 255.0);
    return out;
}

AdjustLut buildAdjustLut(const AdjustOp &op)
{
    // White balance as per-channel gains: warmer pushes red up and blue down,
    // a positive tint pushes green up and the other two slightly down.
    const double wb[3] = {
        (1.0 + 0.30 * op.temperature) * (1.0 - 0.10 * op.tint),
        1.0 + 0.25 * op.tint,
        (1.0 - 0.30 * op.temperature) * (1.0 - 0.10 * op.tint),
    };
    // 0.9 in the exponent: exposure is applied to display-encoded values,
    // where doubling the number is ~2.2 stops of real light.
    const double exposureGain = std::exp2(0.9 * op.exposure);
    const double gammaInverse = 1.0 / std::pow(3.0, op.gamma);
    const double contrastFactor = 1.0 + op.contrast;
    const Spline masterCurve(op.curves[0]);
    const Spline channelCurves[3] = {Spline(op.curves[1]), Spline(op.curves[2]), Spline(op.curves[3])};

    AdjustLut lut;
    std::array<quint8, 256> *tables[3] = {&lut.r, &lut.g, &lut.b};
    for (int ch = 0; ch < 3; ++ch) {
        for (int i = 0; i < 256; ++i) {
            double v = i / 255.0;
            v = clamp01(v * wb[ch]);
            v = clamp01(v * exposureGain);
            v = applyLevels(v, op.levels[1 + ch]);
            v = applyLevels(v, op.levels[0]);
            v = op.outBlack + v * (op.outWhite - op.outBlack);
            v = std::pow(clamp01(v), gammaInverse);
            v = clamp01(v + op.blacks * 0.20 * std::pow(1.0 - v, 3.0));
            v = clamp01(v + op.whites * 0.20 * v * v * v);
            v = masterCurve.eval(v);
            v = channelCurves[ch].eval(v);
            v = v + op.brightness;
            v = (v - 0.5) * contrastFactor + 0.5;
            (*tables[ch])[i] = toByte(v);
        }
    }
    return lut;
}

QImage lutToImage(const AdjustLut &lut)
{
    QImage img(256, 1, QImage::Format_RGBA8888);
    quint8 *line = img.scanLine(0);
    for (int i = 0; i < 256; ++i) {
        line[i * 4] = lut.r[i];
        line[i * 4 + 1] = lut.g[i];
        line[i * 4 + 2] = lut.b[i];
        line[i * 4 + 3] = 255;
    }
    return img;
}

QImage applyGrade(const QImage &source, const AdjustOp &tone, const GradeExtras &extras, double amount)
{
    QImage out = source.format() == QImage::Format_RGBA8888
        ? source
        : source.convertToFormat(QImage::Format_RGBA8888);
    amount = clamp01(amount);

    AdjustOp stagesAB = tone; // the detail/finishing sliders are not part of this pass
    stagesAB.clarity = 0.0;
    stagesAB.sharpness = 0.0;
    stagesAB.vignette = 0.0;
    stagesAB.grain = 0.0;
    if (amount <= 0.0 || (stagesAB.isIdentity() && !extras.isActive()))
        return out;

    out.bits(); // force the one detach from `source` up front (see forEachRowParallel)
    const AdjustLut lut = buildAdjustLut(stagesAB);
    const ColorMix mix(stagesAB);
    const bool mixActive = mix.active();
    const bool extrasActive = extras.isActive();
    const int width = out.width();
    const int height = out.height();

    forEachRowParallel(height, [&](int y) {
        quint8 *line = out.scanLine(y);
        for (int x = 0; x < width; ++x) {
            quint8 *px = line + x * 4;
            const double in[3] = {px[0] / 255.0, px[1] / 255.0, px[2] / 255.0};
            double c[3] = {lut.r[px[0]] / 255.0, lut.g[px[1]] / 255.0, lut.b[px[2]] / 255.0};
            if (mixActive)
                mix.apply(c[0], c[1], c[2]);
            if (extrasActive)
                applyExtras(extras, x, y, width, height, c);
            if (amount < 1.0) {
                for (int i = 0; i < 3; ++i)
                    c[i] = in[i] + (c[i] - in[i]) * amount;
            }
            px[0] = toByte(c[0]);
            px[1] = toByte(c[1]);
            px[2] = toByte(c[2]);
        }
    });
    return out;
}

QImage applyAdjustOp(const QImage &source, const AdjustOp &op)
{
    QImage out = applyGrade(source, op, GradeExtras{}, 1.0);
    if (op.clarity != 0.0 || op.sharpness > 0.0 || op.vignette != 0.0 || op.grain > 0.0)
        out = applyDetail(out, op.clarity, op.sharpness, op.vignette, op.grain, op.grainType, op.grainMono);
    return out;
}

AdjustOp autoAdjusted(AdjustOp base, const QImage &image, AutoAdjustKind kind)
{
    for (auto &lv : base.levels)
        lv = LevelsChannel{};
    if (image.isNull())
        return base;

    const Histogram h = computeHistogram(image);
    constexpr double kClip = 0.005;
    constexpr int kMinSpan = 8; // an already-flat range isn't worth stretching

    switch (kind) {
    case AutoAdjustKind::Contrast: {
        const auto [lo, hi] = trimmedRange(h.luma, kClip);
        if (hi - lo >= kMinSpan)
            base.levels[0] = LevelsChannel{lo / 255.0, 1.0, hi / 255.0};
        break;
    }
    case AutoAdjustKind::Levels: {
        const std::array<int, 256> *bins[3] = {&h.r, &h.g, &h.b};
        for (int c = 0; c < 3; ++c) {
            const auto [lo, hi] = trimmedRange(*bins[c], kClip);
            if (hi - lo >= kMinSpan)
                base.levels[1 + c] = LevelsChannel{lo / 255.0, 1.0, hi / 255.0};
        }
        break;
    }
    case AutoAdjustKind::Color: {
        const double means[3] = {meanOf(h.r), meanOf(h.g), meanOf(h.b)};
        const double target = (means[0] + means[1] + means[2]) / 3.0;
        double gains[3];
        double smallest = 1e9;
        for (int c = 0; c < 3; ++c) {
            gains[c] = std::clamp(target / std::max(means[c], 0.02), 0.6, 1.6);
            smallest = std::min(smallest, gains[c]);
        }
        // Scaled so the weakest channel is left alone and the others are
        // stretched up to meet it: a channel's white point can only move down.
        for (int c = 0; c < 3; ++c)
            base.levels[1 + c].inWhite = 1.0 / std::min(gains[c] / smallest, 1.6);
        break;
    }
    }
    return base;
}

void applyUserNoise(int x, int y, double amount, int type, bool mono, double c[3])
{
    applyNoise(x, y, amount, kNoiseScaleUser, type, mono, c);
}

} // namespace core::edit
