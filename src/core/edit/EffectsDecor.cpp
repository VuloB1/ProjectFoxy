#include "Blend.h"
#include "EffectsCommon.h"

#include <algorithm>
#include <array>
#include <cmath>

// Decorative overlays of the "Dibujo" family: lines, concentric rings, speed lines, gradient and
// pattern fills, the border line. All of them lay a drawing over the picture with a blend mode and
// an opacity, so they share the helpers below.

namespace core::edit::fxk {

namespace {

// --- colour sources -------------------------------------------------------------------------

// What colour the drawn strokes are: a flat colour, a gradient from A to B across the picture, or a
// rainbow across it.
struct Paint {
    int mode = 0; // 0 flat, 1 gradient, 2 rainbow
    Rgb a, b;
    Rgb at(double fx, double fy) const
    {
        if (mode == 0)
            return a;
        const double t = clampd((fx + fy) * 0.5, 0.0, 1.0);
        if (mode == 1)
            return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t)};
        return hsvToRgb(t * 300.0, 0.85, 1.0);
    }
};

Paint makePaint(double mode, double ca, double cb)
{
    Paint p;
    p.mode = clampi(int(std::lround(mode)), 0, 2);
    p.a = unpackColor(ca);
    p.b = unpackColor(cb);
    return p;
}

BlendMode blendFrom(double v) { return BlendMode(clampi(int(std::lround(v)), 0, int(BlendMode::Count) - 1)); }

// Anti-aliased coverage of a stroke of the given width, `dist` from its centre line.
inline double strokeCover(double dist, double width) { return clampd(width * 0.5 + 0.5 - dist, 0.0, 1.0); }

QImage freshCopy(const QImage &src)
{
    QImage out = src;
    out.bits(); // detach before the rows run in parallel
    return out;
}

// ---------------------------------------------------------------------------------------------
// Líneas
// ---------------------------------------------------------------------------------------------

QImage fxLines(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const int pattern = clampi(int(std::lround(v[0])), 0, 4);
    const double spacing = std::max(2.5, longSide * 0.95 / std::max(1.0, v[1]));
    const double thick = std::max(1.0, spacing * v[2] / 100.0);
    const double psi = v[3] * kPi / 180.0;
    const double phase = v[4] / 100.0 * spacing;
    const Paint paint = makePaint(v[5], v[6], v[7]);
    const double opacity = v[8] / 100.0;
    const BlendMode mode = blendFrom(v[9]);

    // The directions the lines run along; the coordinate that counts is the distance across them.
    double dirs[2];
    int count = 1;
    switch (pattern) {
    case 0: dirs[0] = 0.0; break;
    case 1: dirs[0] = kPi / 2; break;
    case 2: dirs[0] = 0.0; dirs[1] = kPi / 2; count = 2; break;
    case 3: dirs[0] = psi; break;
    default: dirs[0] = psi; dirs[1] = psi + kPi / 2; count = 2; break;
    }
    double nx[2], ny[2];
    for (int i = 0; i < count; ++i) {
        nx[i] = -std::sin(dirs[i]);
        ny[i] = std::cos(dirs[i]);
    }

    QImage out = freshCopy(src);
    const double w = src.width(), h = src.height();
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            double cover = 0.0;
            for (int i = 0; i < count; ++i) {
                const double s = x * nx[i] + y * ny[i] + phase;
                double m = std::fmod(s, spacing);
                if (m < 0)
                    m += spacing;
                const double dist = std::min(m, spacing - m);
                cover = std::max(cover, strokeCover(dist, thick));
            }
            if (cover <= 0.0)
                continue;
            const Rgb c = paint.at(x / w, y / h);
            compositeOver(d + x * 4, c.r, c.g, c.b, cover * opacity, mode);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Círculos concéntricos
// ---------------------------------------------------------------------------------------------

// The "radius" of a point for each shape of ring: circles, squares, diamonds, hexagons.
inline double ringRadius(int shape, double dx, double dy)
{
    switch (shape) {
    case 1: return std::max(std::abs(dx), std::abs(dy));
    case 2: return std::abs(dx) + std::abs(dy);
    case 3: return std::max(std::abs(dx) * 0.8660254 + std::abs(dy) * 0.5, std::abs(dy));
    default: return std::sqrt(dx * dx + dy * dy);
    }
}

QImage fxRings(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const int shape = clampi(int(std::lround(v[0])), 0, 3);
    const double spacing = std::max(2.5, longSide * 0.95 / std::max(1.0, v[1]));
    const double thick = std::max(1.0, spacing * v[2] / 100.0);
    const double w = src.width(), h = src.height();
    const double cx = (w - 1) * v[5] / 100.0, cy = (h - 1) * v[6] / 100.0;
    // The reach: farthest corner, measured the way the shape measures
    double reach = 1.0;
    for (int i = 0; i < 4; ++i)
        reach = std::max(reach, ringRadius(shape, (i & 1 ? w - 1 : 0) - cx, (i & 2 ? h - 1 : 0) - cy));
    const double rin = v[3] / 100.0 * reach, rout = v[4] / 100.0 * reach;
    const Paint paint = makePaint(v[7], v[8], v[9]);
    const double opacity = v[10] / 100.0;
    const BlendMode mode = blendFrom(v[11]);

    QImage out = freshCopy(src);
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double rho = ringRadius(shape, x - cx, y - cy);
            if (rho < rin - thick || rho > rout + thick)
                continue;
            // rings at rin + (k + 0.5) * spacing
            double m = std::fmod(rho - rin, spacing);
            if (m < 0)
                m += spacing;
            const double dist = std::abs(m - spacing * 0.5);
            double cover = strokeCover(dist, thick);
            cover *= clampd(rho - rin + 0.5, 0.0, 1.0) * clampd(rout - rho + 0.5, 0.0, 1.0);
            if (cover <= 0.0)
                continue;
            const Rgb c = paint.at(x / w, y / h);
            compositeOver(d + x * 4, c.r, c.g, c.b, cover * opacity, mode);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Líneas de velocidad radial
// ---------------------------------------------------------------------------------------------

QImage fxSpeedLines(const Job &job, const QImage &src, const EffectValues &v)
{
    const int style = clampi(int(std::lround(v[0])), 0, 3);
    const int count = clampi(int(std::lround(v[1])), 3, 400);
    const double widthK = v[2] / 100.0;
    const double w = src.width(), h = src.height();
    const double cx = (w - 1) * v[7] / 100.0, cy = (h - 1) * v[8] / 100.0;
    double reach = 1.0;
    for (int i = 0; i < 4; ++i)
        reach = std::max(reach, std::hypot((i & 1 ? w - 1 : 0) - cx, (i & 2 ? h - 1 : 0) - cy));
    const double rin = v[3] / 100.0 * reach, rout = std::max(rin + 1.0, v[4] / 100.0 * reach);
    const double variation = v[5] / 100.0;
    const uint32_t seed = uint32_t(std::lround(v[6]));
    const Paint paint = makePaint(v[9], v[10], v[11]);
    const double opacity = v[12] / 100.0;
    const BlendMode mode = blendFrom(v[13]);
    const double sector = 2.0 * kPi / count;

    // Every ray has its own random angle, width and (for the burst style) start radius.
    struct Ray { double angle, halfWidth, start; };
    std::vector<Ray> rays(size_t(count) + 2);
    for (int i = 0; i < count + 2; ++i) {
        Rng rng(seed * 7919u + uint32_t(i) * 104729u + 13u);
        Ray &r = rays[size_t(i)];
        const double jitter = (rng.unit() - 0.5) * variation;
        const double wScale = 1.0 - variation * rng.unit() * 0.85;
        r.angle = (i + 0.5 + jitter * 0.8) * sector;
        r.halfWidth = widthK * sector * 0.5 * (style == 0 ? 1.0 : wScale);
        r.start = style == 3 ? rin + (rout - rin) * variation * rng.unit() * 0.8 : rin;
    }

    QImage out = freshCopy(src);
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double dx = x - cx, dy = y - cy;
            const double r = std::sqrt(dx * dx + dy * dy);
            if (r < rin || r > rout + 1.0)
                continue;
            double theta = std::atan2(dy, dx) + kPi; // 0..2pi
            const int idx = int(theta / sector);
            double cover = 0.0;
            // the ray of this sector and its two neighbours (a jittered ray can lean over the line)
            for (int di = -1; di <= 1; ++di) {
                const int k = (idx + di + count) % count;
                const Ray &ray = rays[size_t(k)];
                if (r < ray.start)
                    continue;
                double dTheta = theta - ray.angle - (idx + di - k) * 0.0;
                dTheta = std::remainder(dTheta, 2.0 * kPi);
                const double dist = std::abs(dTheta) * r; // across the ray, in pixels
                const double reachFrac = clampd((r - ray.start) / std::max(1.0, rout - ray.start), 0.0, 1.0);
                double half;
                if (style == 0)
                    half = ray.halfWidth * r;                          // a constant-angle wedge
                else if (style == 1)
                    half = ray.halfWidth * r * reachFrac;              // a spike, sharp at the inner end
                else if (style == 2)
                    half = std::max(0.5, widthK * 0.012 * std::max(w, h)); // a thin line of constant width
                else
                    half = ray.halfWidth * r * (0.35 + 0.65 * reachFrac); // a burst: starts wide-ish, widens
                cover = std::max(cover, clampd(half + 0.5 - dist, 0.0, 1.0));
            }
            cover *= clampd(r - rin + 0.5, 0.0, 1.0) * clampd(rout - r + 0.5, 0.0, 1.0);
            if (cover <= 0.0)
                continue;
            const Rgb c = paint.at(x / w, y / h);
            compositeOver(d + x * 4, c.r, c.g, c.b, cover * opacity, mode);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Relleno de degradado
// ---------------------------------------------------------------------------------------------

struct GradientPreset {
    const char *name;
    std::array<uint32_t, 4> colors; // 0xRRGGBB; unused tail entries are 0xFFFFFFFF
    int count;
};

const std::array<GradientPreset, 17> &gradientPresets()
{
    static const std::array<GradientPreset, 17> presets = {{
        {"Atardecer", {0xFF512F, 0xF09819, 0xDD2476, 0}, 3},
        {"Océano", {0x2E3192, 0x1BFFFF, 0, 0}, 2},
        {"Bosque", {0x134E5E, 0x71B280, 0, 0}, 2},
        {"Fuego", {0x200122, 0xFF5E00, 0xFFE100, 0}, 3},
        {"Hielo", {0xE6DADA, 0x274046, 0, 0}, 2},
        {"Lavanda", {0xDA22FF, 0x9733EE, 0, 0}, 2},
        {"Aurora", {0x00C9FF, 0x92FE9D, 0, 0}, 2},
        {"Caramelo", {0xFF61D2, 0xFE9090, 0, 0}, 2},
        {"Medianoche", {0x0F0C29, 0x302B63, 0x24243E, 0}, 3},
        {"Oro", {0xF7971E, 0xFFD200, 0, 0}, 2},
        {"Plata", {0xBDC3C7, 0x2C3E50, 0, 0}, 2},
        {"Neón", {0xFF00FF, 0x00FFFF, 0, 0}, 2},
        {"Menta", {0x00B09B, 0x96C93D, 0, 0}, 2},
        {"Rosa", {0xFF9A9E, 0xFECFEF, 0, 0}, 2},
        {"Sepia", {0x704214, 0xE8D5B0, 0, 0}, 2},
        {"Cielo", {0x1E3C72, 0x2A5298, 0xA1C4FD, 0}, 3},
        {"Blanco a negro", {0xFFFFFF, 0x000000, 0, 0}, 2},
    }};
    return presets;
}

Rgb rgbOf(uint32_t c) { return {double((c >> 16) & 255), double((c >> 8) & 255), double(c & 255)}; }

struct Ramp {
    std::array<Rgb, 5> stop;
    int n = 2;
    Rgb at(double t) const
    {
        t = clampd(t, 0.0, 1.0) * (n - 1);
        const int i = std::min(n - 2, int(t));
        const double f = t - i;
        return {lerp(stop[size_t(i)].r, stop[size_t(i + 1)].r, f), lerp(stop[size_t(i)].g, stop[size_t(i + 1)].g, f),
                lerp(stop[size_t(i)].b, stop[size_t(i + 1)].b, f)};
    }
};

QImage fxGradientFill(const Job &job, const QImage &src, const EffectValues &v)
{
    const int type = clampi(int(std::lround(v[0])), 0, 5);
    const int presetIndex = clampi(int(std::lround(v[1])), 0, int(gradientPresets().size())); // 0 = custom
    Ramp ramp;
    if (presetIndex > 0) {
        const GradientPreset &p = gradientPresets()[size_t(presetIndex - 1)];
        ramp.n = p.count;
        for (int i = 0; i < p.count; ++i)
            ramp.stop[size_t(i)] = rgbOf(p.colors[size_t(i)]);
    } else if (v[4] >= 0.5) {
        ramp.n = 3;
        ramp.stop = {unpackColor(v[2]), unpackColor(v[5]), unpackColor(v[3]), {}, {}};
    } else {
        ramp.n = 2;
        ramp.stop = {unpackColor(v[2]), unpackColor(v[3]), {}, {}, {}};
    }
    const bool invert = v[6] >= 0.5;
    const double angle = v[7] * kPi / 180.0;
    const double scale = std::max(0.01, v[8] / 100.0);
    const double w = src.width(), h = src.height();
    const double cx = (w - 1) * v[9] / 100.0, cy = (h - 1) * v[10] / 100.0;
    const double opacity = v[11] / 100.0;
    const BlendMode mode = blendFrom(v[12]);
    const double ca = std::cos(angle), sa = std::sin(angle);
    // The span the gradient covers: for a linear one, the picture's extent along its direction.
    const double extent = (std::abs(w * ca) + std::abs(h * sa)) * scale;
    const double radius = 0.5 * std::hypot(w, h) * scale;

    QImage out = freshCopy(src);
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double dx = x - cx, dy = y - cy;
            double t;
            switch (type) {
            case 0: { // linear, along the angle, centred on the centre point
                t = 0.5 + (dx * ca + dy * sa) / std::max(1.0, extent);
                break;
            }
            case 1: t = std::sqrt(dx * dx + dy * dy) / std::max(1.0, radius); break;                     // radial
            case 2: {                                                                                      // conic
                double a = std::atan2(dy, dx) - angle;
                a = std::fmod(a, 2.0 * kPi);
                if (a < 0)
                    a += 2.0 * kPi;
                t = a / (2.0 * kPi);
                break;
            }
            case 3: t = (std::abs(dx) + std::abs(dy)) / std::max(1.0, radius * 1.2); break;               // diamond
            case 4: {                                                                                      // reflected
                const double l = (dx * ca + dy * sa) / std::max(1.0, extent * 0.5);
                t = std::abs(l);
                break;
            }
            default: t = std::max(std::abs(dx), std::abs(dy)) / std::max(1.0, radius * 0.75); break;     // square
            }
            t = clampd(t, 0.0, 1.0);
            if (invert)
                t = 1.0 - t;
            const Rgb c = ramp.at(t);
            compositeOver(d + x * 4, c.r, c.g, c.b, opacity, mode);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Relleno de motivo
// ---------------------------------------------------------------------------------------------

inline double fract(double x) { return x - std::floor(x); }
// Soft step: 0 below the edge, 1 above it, over a transition `aa` wide.
inline double edgeStep(double d, double aa) { return clampd(d / std::max(aa, 1e-6) + 0.5, 0.0, 1.0); }

double valueNoise(double x, double y, uint32_t salt)
{
    const int xi = int(std::floor(x)), yi = int(std::floor(y));
    const double fx = smoothStep01(x - xi), fy = smoothStep01(y - yi);
    const double a = hashUnit(hash2(xi, yi, salt)), b = hashUnit(hash2(xi + 1, yi, salt));
    const double c = hashUnit(hash2(xi, yi + 1, salt)), d = hashUnit(hash2(xi + 1, yi + 1, salt));
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fy);
}

// The motifs. Each takes a point in cell units (one repeat = 1.0) and returns how much of the
// foreground colour is there (0 = all background); `aa` is the size of a pixel in those units.
double motifValue(int kind, double u, double v, double aa)
{
    switch (kind) {
    case 0: { // dots
        const double dx = fract(u) - 0.5, dy = fract(v) - 0.5;
        return edgeStep(0.28 - std::sqrt(dx * dx + dy * dy), aa);
    }
    case 1: { // staggered dots
        const double row = std::floor(v);
        const double dx = fract(u + (int(row) & 1 ? 0.5 : 0.0)) - 0.5, dy = fract(v) - 0.5;
        return edgeStep(0.26 - std::sqrt(dx * dx + dy * dy), aa);
    }
    case 2: return edgeStep(0.25 - std::abs(fract(v) - 0.5), aa);          // horizontal stripes
    case 3: return edgeStep(0.25 - std::abs(fract(u) - 0.5), aa);          // vertical stripes
    case 4: return edgeStep(0.25 - std::abs(fract(u + v) - 0.5), aa);      // diagonal stripes
    case 5: {                                                             // checkerboard
        const double fu = fract(u), fv = fract(v);
        const int parity = (int(std::floor(u)) + int(std::floor(v))) & 1;
        const double edge = std::min({fu, 1.0 - fu, fv, 1.0 - fv}); // 0 on a cell border: both sides meet at half
        return 0.5 + (parity ? 0.5 : -0.5) * clampd(edge / aa, 0.0, 1.0);
    }
    case 6: {                                                             // tartan
        const double bandH = edgeStep(0.16 - std::abs(fract(v) - 0.5), aa);
        const double bandV = edgeStep(0.16 - std::abs(fract(u) - 0.5), aa);
        return clampd(0.5 * bandH + 0.5 * bandV, 0.0, 1.0);
    }
    case 7: {                                                             // argyle: a checkerboard turned 45 degrees
        const double s = u + v, t = u - v;
        const double fs = fract(s), ft = fract(t);
        const int parity = (int(std::floor(s)) + int(std::floor(t))) & 1;
        const double edge = std::min({fs, 1.0 - fs, ft, 1.0 - ft});
        return 0.5 + (parity ? 0.5 : -0.5) * clampd(edge / (aa * 1.4), 0.0, 1.0);
    }
    case 8: {                                                             // zigzag
        const double tri = std::abs(fract(u) - 0.5) * 2.0; // 0..1 triangle wave
        return edgeStep(0.16 - std::abs(fract(v + tri * 0.5) - 0.5), aa);
    }
    case 9: return edgeStep(0.16 - std::abs(fract(v + 0.18 * std::sin(u * 2.0 * kPi)) - 0.5), aa); // waves
    case 10: {                                                            // bricks
        const double row = std::floor(v);
        const double fu = fract(u + (int(row) & 1 ? 0.5 : 0.0)), fv = fract(v);
        const double edge = std::min({fu, 1.0 - fu, fv, 1.0 - fv});
        return 1.0 - edgeStep(0.06 - edge, aa);                            // bricks in front, mortar behind
    }
    case 11: {                                                            // honeycomb outline
        constexpr double kRh = 1.7320508;
        const double px = u, py = v * kRh;
        auto wrap = [](double a, double b) { return a - b * std::floor(a / b); };
        const double ax = wrap(px, 1.0) - 0.5, ay = wrap(py, kRh) - kRh * 0.5;
        const double bx = wrap(px - 0.5, 1.0) - 0.5, by = wrap(py - kRh * 0.5, kRh) - kRh * 0.5;
        const bool first = ax * ax + ay * ay < bx * bx + by * by;
        const double gx = std::abs(first ? ax : bx), gy = std::abs(first ? ay : by);
        const double hexDist = std::max(gx, gx * 0.5 + gy * 0.8660254);
        return edgeStep(hexDist - 0.43, aa);
    }
    case 12: {                                                            // triangles (a tiling of up and down ones)
        const double ry = v / 0.8660254;
        const double iy = std::floor(ry), fy = fract(ry);
        const double xs = u + ((int(iy) & 1) ? 0.5 : 0.0);
        const double fx = fract(xs);
        // inside the upward triangle: below the two slanted edges that meet at the middle
        const double slant = 1.0 - 2.0 * std::abs(fx - 0.5) - fy;
        return edgeStep(slant * 0.45, aa);
    }
    case 13: {                                                            // four-point stars
        const double dx = std::abs(fract(u) - 0.5), dy = std::abs(fract(v) - 0.5);
        return edgeStep(0.62 - (std::sqrt(dx) + std::sqrt(dy)), aa * 2.0);
    }
    case 14: {                                                            // crosses
        const double dx = std::abs(fract(u) - 0.5), dy = std::abs(fract(v) - 0.5);
        const double armH = edgeStep(0.09 - dy, aa) * edgeStep(0.32 - dx, aa);
        const double armV = edgeStep(0.09 - dx, aa) * edgeStep(0.32 - dy, aa);
        return std::max(armH, armV);
    }
    case 15: {                                                            // scales
        const double row = std::floor(v * 2.0);
        const double fu = fract(u + (int(row) & 1 ? 0.5 : 0.0)) - 0.5, fv = fract(v * 2.0);
        const double dist = std::sqrt(fu * fu + fv * fv);
        return edgeStep(0.07 - std::abs(dist - 0.5), aa * 2.0);
    }
    case 16: {                                                            // herringbone
        const double column = std::floor(u), fu = fract(u);
        const double t = (int(column) & 1) ? fu + v : -fu + v;
        return edgeStep(0.22 - std::abs(fract(t * 2.0) - 0.5), aa * 2.0);
    }
    case 17: {                                                            // camouflage
        const double n = 0.55 * valueNoise(u * 1.2, v * 1.2, 5) + 0.3 * valueNoise(u * 2.6, v * 2.6, 6)
                         + 0.15 * valueNoise(u * 5.0, v * 5.0, 7);
        return n < 0.42 ? 0.0 : (n < 0.58 ? 0.45 : (n < 0.7 ? 0.75 : 1.0));
    }
    case 18: return valueNoise(u * 24.0, v * 24.0, 9) * 0.8 + valueNoise(u * 60.0, v * 60.0, 3) * 0.2; // paper
    case 19: {                                                            // graph paper
        const double lineU = edgeStep(0.03 - std::min(fract(u), 1.0 - fract(u)), aa);
        const double lineV = edgeStep(0.03 - std::min(fract(v), 1.0 - fract(v)), aa);
        return std::max(lineU, lineV);
    }
    case 20: {                                                            // pebbles (cellular)
        const int ci = int(std::floor(u)), cj = int(std::floor(v));
        double best = 1e9, second = 1e9;
        uint32_t id = 0;
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i) {
                const double px = ci + i + 0.15 + 0.7 * hashUnit(hash2(ci + i, cj + j, 21));
                const double py = cj + j + 0.15 + 0.7 * hashUnit(hash2(ci + i, cj + j, 22));
                const double d = std::hypot(px - u, py - v);
                if (d < best) {
                    second = best;
                    best = d;
                    id = hash2(ci + i, cj + j, 23);
                } else if (d < second) {
                    second = d;
                }
            }
        const double border = edgeStep(second - best - 0.05, aa * 2.0);
        return (0.25 + 0.75 * hashUnit(id)) * border;
    }
    default: {                                                            // squares
        const double fu = fract(u) - 0.5, fv = fract(v) - 0.5;
        return edgeStep(0.3 - std::max(std::abs(fu), std::abs(fv)), aa);
    }
    }
}

const QStringList &motifNames()
{
    static const QStringList names = {"Puntos",   "Puntos alternados", "Rayas horizontales", "Rayas verticales",
                                      "Rayas diagonales", "Tablero", "Tartán",   "Rombos",
                                      "Zigzag",   "Olas",              "Ladrillos",          "Panal",
                                      "Triángulos", "Estrellas",       "Cruces",             "Escamas",
                                      "Espiga",   "Camuflaje",         "Papel",              "Cuadrícula fina",
                                      "Guijarros", "Cuadrados"};
    return names;
}

QImage fxPatternFill(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const int kind = clampi(int(std::lround(v[0])), 0, int(motifNames().size()) - 1);
    const Rgb fg = unpackColor(v[1]), bgc = unpackColor(v[2]);
    const double cell = std::max(4.0, longSide * 0.03 * v[3] / 100.0);
    const double angle = v[4] * kPi / 180.0;
    const double ca = std::cos(angle), sa = std::sin(angle);
    const double offX = v[5] / 100.0, offY = v[6] / 100.0;
    const double opacity = v[7] / 100.0;
    const BlendMode mode = blendFrom(v[8]);
    const double aa = 1.0 / cell;
    const double w = src.width(), h = src.height();

    QImage out = freshCopy(src);
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double px = x - w * 0.5, py = y - h * 0.5;
            const double u = (px * ca + py * sa) / cell + offX;
            const double vv = (-px * sa + py * ca) / cell + offY;
            const double m = clampd(motifValue(kind, u, vv, aa), 0.0, 1.0);
            compositeOver(d + x * 4, lerp(bgc.r, fg.r, m), lerp(bgc.g, fg.g, m), lerp(bgc.b, fg.b, m), opacity, mode);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Línea de borde
// ---------------------------------------------------------------------------------------------

// Signed distance to a rounded rectangle centred at (cx, cy) with half sizes (hx, hy) and corner radius r.
inline double roundedRectSdf(double px, double py, double cx, double cy, double hx, double hy, double r)
{
    const double qx = std::abs(px - cx) - (hx - r), qy = std::abs(py - cy) - (hy - r);
    const double ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0) - r;
}

// Distance travelled along the rounded rectangle's outline (clockwise, from the left end of its top
// edge) up to the point of it nearest to (px, py): what the dashes of a dashed line are laid along.
double outlineParam(double px, double py, double cx, double cy, double hx, double hy, double r)
{
    const double sx = hx - r, sy = hy - r; // the straight parts run over +-sx and +-sy
    const double rx = px - cx, ry = py - cy;
    const double A = 0.5 * kPi * r, T = 2.0 * sx, S = 2.0 * sy;
    constexpr double kQuarter = 0.5 * kPi;
    auto quarter = [&](double angle, double first) { return clampd((angle - first) / kQuarter, 0.0, 1.0) * A; };
    if (rx > sx && ry < -sy) // top-right corner
        return T + quarter(std::atan2(ry + sy, rx - sx), -kQuarter);
    if (rx > sx && ry > sy) // bottom-right
        return T + A + S + quarter(std::atan2(ry - sy, rx - sx), 0.0);
    if (rx < -sx && ry > sy) { // bottom-left
        double a = std::atan2(ry - sy, rx + sx);
        if (a < 0)
            a += 2.0 * kPi;
        return 2.0 * T + 2.0 * A + S + quarter(a, kQuarter);
    }
    if (rx < -sx && ry < -sy) { // top-left
        double a = std::atan2(ry + sy, rx + sx);
        if (a < 0)
            a += 2.0 * kPi;
        return 2.0 * T + 3.0 * A + 2.0 * S + quarter(a, kPi);
    }
    if (std::abs(rx) <= sx) // top or bottom edge
        return ry < 0 ? rx + sx : T + 2.0 * A + S + (sx - rx);
    return rx > 0 ? T + A + (ry + sy) : 2.0 * T + 3.0 * A + S + (sy - ry); // right or left edge
}

QImage fxEdgeLine(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const double w = src.width(), h = src.height();
    const double thick = std::max(1.0, longSide * 0.02 * v[0] / 100.0);
    const double margin = std::min(w, h) * 0.2 * v[1] / 100.0 + thick * 0.5;
    const double hx = w * 0.5 - margin, hy = h * 0.5 - margin;
    if (hx < 2.0 || hy < 2.0)
        return src;
    const double r = std::min(hx, hy) * v[2] / 100.0;
    const int style = clampi(int(std::lround(v[3])), 0, 4);
    const Rgb color = unpackColor(v[4]);
    const double opacity = v[5] / 100.0;
    const BlendMode mode = blendFrom(v[6]);
    const double cx = w * 0.5, cy = h * 0.5;
    const double dashLen = thick * 4.0, gap = thick * 2.5;

    QImage out = freshCopy(src);
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double sd = roundedRectSdf(x + 0.5, y + 0.5, cx, cy, hx, hy, r);
            double cover;
            if (style == 4) { // double: two thinner parallel lines
                const double one = thick * 0.38, off = thick * 0.5 - one * 0.5 + one * 0.6;
                cover = std::max(strokeCover(std::abs(sd + off), one), strokeCover(std::abs(sd - off), one));
            } else {
                cover = strokeCover(std::abs(sd), thick);
            }
            if (cover <= 0.0)
                continue;
            if (style == 1 || style == 2 || style == 3) {
                const double s = outlineParam(x + 0.5, y + 0.5, cx, cy, hx, hy, r);
                double pattern;
                if (style == 1) { // dashes
                    const double period = dashLen + gap;
                    const double m = std::fmod(s, period);
                    pattern = clampd(std::min(m, dashLen - m) + 0.5, 0.0, 1.0);
                } else if (style == 2) { // dots
                    const double period = thick * 2.2;
                    const double m = std::fmod(s, period) - period * 0.5;
                    const double dist = std::hypot(m, sd);
                    pattern = strokeCover(dist, thick) / std::max(cover, 1e-6);
                    cover = 1.0;
                    pattern = clampd(pattern, 0.0, 1.0);
                } else { // dash - dot
                    const double period = dashLen + gap * 2.0 + thick;
                    const double m = std::fmod(s, period);
                    const double dashP = clampd(std::min(m, dashLen - m) + 0.5, 0.0, 1.0);
                    const double dotC = dashLen + gap + thick * 0.5;
                    const double dotP = clampd(thick * 0.5 + 0.5 - std::abs(m - dotC), 0.0, 1.0);
                    pattern = std::max(dashP, dotP);
                }
                cover *= pattern;
            }
            if (cover <= 0.0)
                continue;
            compositeOver(d + x * 4, color.r, color.g, color.b, cover * opacity, mode);
        }
    });
    return out;
}

} // namespace

void addDecorSpecs(std::vector<EffectSpec> &out)
{
    const QStringList fills = {"Color", "Degradado", "Arcoíris"};
    const int white = int(packColor(255, 255, 255));
    {
        EffectSpec s = makeSpec(
            "lines", "Líneas", "pattern",
            {choice("Patrón", {"Horizontales", "Verticales", "Cuadrícula", "Diagonales", "Cuadrícula girada"}, 2),
             slider("Densidad", 1, 100, 50), slider("Grosor", 1, 100, 25),
             shownWhen(slider("Ángulo", -90, 90, 45, "°", true), 0, {3, 4}), slider("Desfase", 0, 100, 0),
             choice("Relleno", fills, 0), colorParam("Color", 255, 255, 255),
             shownWhen(colorParam("Segundo color", 40, 120, 255), 5, {1}), slider("Opacidad", 0, 100, 80),
             blendParam(0)});
        (void)white;
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "rings", "Círculos concéntricos", "pattern",
            {choice("Forma", {"Círculos", "Cuadrados", "Rombos", "Hexágonos"}, 0), slider("Densidad", 1, 100, 50),
             slider("Grosor", 1, 100, 25), slider("Radio interior", 0, 100, 30), slider("Radio exterior", 0, 100, 100),
             slider("Centro X", 0, 100, 50), slider("Centro Y", 0, 100, 50), choice("Relleno", fills, 0),
             colorParam("Color", 255, 255, 255), shownWhen(colorParam("Segundo color", 40, 120, 255), 7, {1}),
             slider("Opacidad", 0, 100, 80), blendParam(0)});
        s.overlays = {{EffectOverlay::Point, 5, 6, "Centro"}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "speed", "Líneas de velocidad", "pattern",
            {choice("Estilo", {"Regulares", "Afiladas", "Finas", "Ráfaga"}, 1), slider("Cantidad", 3, 400, 70, "", true),
             slider("Grosor", 1, 100, 45), slider("Radio interior", 0, 100, 30), slider("Radio exterior", 0, 100, 100),
             slider("Variación", 0, 100, 60), seedParam("Semilla", 7), slider("Centro X", 0, 100, 50),
             slider("Centro Y", 0, 100, 50), choice("Relleno", fills, 0), colorParam("Color", 255, 255, 255),
             shownWhen(colorParam("Segundo color", 40, 120, 255), 9, {1}), slider("Opacidad", 0, 100, 90),
             blendParam(0)});
        s.overlays = {{EffectOverlay::Point, 7, 8, "Centro"}};
        out.push_back(std::move(s));
    }
    {
        QStringList presets = {"Personalizado"};
        for (const GradientPreset &p : gradientPresets())
            presets << QString::fromUtf8(p.name);
        EffectSpec s = makeSpec(
            "gradient", "Relleno de degradado", "pattern",
            {choice("Tipo", {"Lineal", "Radial", "Cónico", "Rombo", "Reflejado", "Cuadrado"}, 0),
             choice("Degradado", presets, 1), shownWhen(colorParam("Color inicial", 255, 120, 40), 1, {0}),
             shownWhen(colorParam("Color final", 40, 80, 255), 1, {0}), shownWhen(toggle("Color central"), 1, {0}),
             shownWhen(colorParam("Color del centro", 255, 255, 255), 4, {1}), toggle("Invertir"),
             slider("Ángulo", -180, 180, 0, "°", true), slider("Escala", 5, 400, 100), slider("Centro X", 0, 100, 50),
             slider("Centro Y", 0, 100, 50), slider("Opacidad", 0, 100, 100), blendParam(int(BlendMode::Overlay))});
        // "Color central" is only meaningful for a custom gradient, so its picker follows both
        s.params[5].dependsOn = 4;
        s.params[5].dependsMask = 2;
        s.overlays = {{EffectOverlay::Point, 9, 10, "Centro"}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("pattern", "Relleno de motivo", "pattern",
                                {choice("Motivo", motifNames(), 0), colorParam("Color del motivo", 30, 30, 30),
                                 colorParam("Color de fondo", 255, 255, 255), slider("Escala", 10, 400, 100),
                                 slider("Rotar", -180, 180, 0, "°", true), slider("Desplazamiento X", -100, 100, 0),
                                 slider("Desplazamiento Y", -100, 100, 0), slider("Opacidad", 0, 100, 50), blendParam(0)});
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "edgeline", "Línea de borde", "pattern",
            {slider("Grosor", 1, 100, 25), slider("Margen", 0, 100, 12), slider("Redondez", 0, 100, 0),
             choice("Estilo", {"Continua", "Guiones", "Puntos", "Guion y punto", "Doble"}, 0),
             colorParam("Color", 255, 255, 255), slider("Opacidad", 0, 100, 100), blendParam(0)});
        out.push_back(std::move(s));
    }
}

bool renderDecor(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result)
{
    if (id == QLatin1String("lines"))
        result = fxLines(job, src, v, longSide);
    else if (id == QLatin1String("rings"))
        result = fxRings(job, src, v, longSide);
    else if (id == QLatin1String("speed"))
        result = fxSpeedLines(job, src, v);
    else if (id == QLatin1String("gradient"))
        result = fxGradientFill(job, src, v);
    else if (id == QLatin1String("pattern"))
        result = fxPatternFill(job, src, v, longSide);
    else if (id == QLatin1String("edgeline"))
        result = fxEdgeLine(job, src, v, longSide);
    else
        return false;
    return true;
}

} // namespace core::edit::fxk
