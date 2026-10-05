#include "Blend.h"
#include "EffectsCommon.h"

#include <algorithm>
#include <cmath>

// The "Luz" family: bokeh, lens flares, light leaks, light beams, dust, sparkles, glow and a spotlight,
// all drawn procedurally (no pictures to ship): a layer of light is drawn on a copy of the picture
// shrunk to a manageable size - light is soft by nature - and laid over the real picture with a
// blend mode (Trama by default, the way light adds up).

namespace core::edit::fxk {

namespace {

// ---------------------------------------------------------------------------------------------
// The light layer
// ---------------------------------------------------------------------------------------------

struct Layer {
    int w = 0, h = 0;
    std::vector<float> d; // RGB interleaved, light intensities (can go above 1)
    Layer(int width, int height) : w(width), h(height), d(size_t(width) * height * 3, 0.f) {}
    float *at(int x, int y) { return &d[(size_t(y) * w + x) * 3]; }
    const float *at(int x, int y) const { return &d[(size_t(y) * w + x) * 3]; }
    void add(int x, int y, const float c[3], float k)
    {
        float *p = at(x, y);
        p[0] += c[0] * k;
        p[1] += c[1] * k;
        p[2] += c[2] * k;
    }
    void sample(double fx, double fy, float out[3]) const
    {
        fx = clampd(fx, 0.0, w - 1);
        fy = clampd(fy, 0.0, h - 1);
        const int x0 = int(fx), y0 = int(fy);
        const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
        const float tx = float(fx - x0), ty = float(fy - y0);
        const float *a = at(x0, y0), *b = at(x1, y0), *c = at(x0, y1), *e = at(x1, y1);
        for (int k = 0; k < 3; ++k) {
            const float top = a[k] + (b[k] - a[k]) * tx, bottom = c[k] + (e[k] - c[k]) * tx;
            out[k] = top + (bottom - top) * ty;
        }
    }
};

// The light layer is drawn at this size (long side) however big the picture is.
Layer makeLayer(const QImage &src, int maxLong, double &factor)
{
    const int longSide = std::max(src.width(), src.height());
    factor = longSide > maxLong ? double(longSide) / maxLong : 1.0;
    return Layer(std::max(2, int(std::lround(src.width() / factor))), std::max(2, int(std::lround(src.height() / factor))));
}

// Lays the light over the picture: the more light, the more the blend shows (so Normal fades the
// light in over the picture, and Trama / Añadir add it up exactly).
QImage layLight(const Job &job, const QImage &src, const Layer &layer, double factor, double opacity, BlendMode mode)
{
    QImage out = src;
    out.bits();
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            float c[3];
            layer.sample((x + 0.5) / factor - 0.5, (y + 0.5) / factor - 0.5, c);
            const float m = std::max({c[0], c[1], c[2]});
            if (m <= 0.002f)
                continue;
            const float inv = 255.f / m;
            compositeOver(d + x * 4, c[0] * inv, c[1] * inv, c[2] * inv, std::min(1.0, double(m)) * opacity, mode);
        }
    });
    return out;
}

BlendMode blendFrom(double v) { return BlendMode(clampi(int(std::lround(v)), 0, int(BlendMode::Count) - 1)); }

void colorTriple(const Rgb &c, float out[3])
{
    out[0] = float(c.r / 255.0);
    out[1] = float(c.g / 255.0);
    out[2] = float(c.b / 255.0);
}

// A bright, slightly pastel version of the picture's colour near (fx, fy) (fractions): what a light
// reflected off that part of the scene would look like.
struct SceneTint {
    QImage small;
    explicit SceneTint(const QImage &src)
        : small(src.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888)) {}
    void at(double fx, double fy, float out[3]) const
    {
        const int x = clampi(int(fx * small.width()), 0, small.width() - 1);
        const int y = clampi(int(fy * small.height()), 0, small.height() - 1);
        const uint8_t *p = small.constScanLine(y) + x * 4;
        float c[3] = {p[0] / 255.f, p[1] / 255.f, p[2] / 255.f};
        const float m = std::max({c[0], c[1], c[2], 0.15f});
        for (int k = 0; k < 3; ++k)
            out[k] = std::min(1.f, 0.18f + 0.82f * c[k] / m);
    }
};

// ---------------------------------------------------------------------------------------------
// Shapes for bokeh and flare ghosts
// ---------------------------------------------------------------------------------------------

enum BokehShape { kCircle = 0, kHexagon, kPentagon, kHeart, kStar, kRing };

// The outline of a heart as its distance from a centre point at each direction (tabulated once).
double heartRadius(double theta)
{
    static const std::vector<double> table = [] {
        constexpr int kBins = 720;
        std::vector<double> t(kBins, 0.0);
        const double cx0 = 0.0, cy0 = -1.0;
        for (int i = 0; i < 4000; ++i) {
            const double u = 2.0 * kPi * i / 4000.0;
            const double x = 16.0 * std::pow(std::sin(u), 3.0);
            const double y = 13.0 * std::cos(u) - 5.0 * std::cos(2 * u) - 2.0 * std::cos(3 * u) - std::cos(4 * u);
            const double px = (x - cx0) / 17.0, py = -(y - cy0) / 17.0; // screen y points down
            const double a = std::atan2(py, px);
            const int bin = clampi(int((a + kPi) / (2.0 * kPi) * kBins), 0, kBins - 1);
            t[size_t(bin)] = std::max(t[size_t(bin)], std::hypot(px, py));
        }
        for (int i = 0; i < kBins; ++i) // a bin the curve skipped takes its neighbours' value
            if (t[size_t(i)] <= 0.0)
                t[size_t(i)] = std::max(t[size_t((i + kBins - 1) % kBins)], t[size_t((i + 1) % kBins)]);
        return t;
    }();
    const int n = int(table.size());
    double f = (theta + kPi) / (2.0 * kPi) * n;
    f -= n * std::floor(f / n);
    const int i0 = int(f) % n, i1 = (i0 + 1) % n;
    return lerp(table[size_t(i0)], table[size_t(i1)], f - std::floor(f));
}

// How deep inside the shape (radius 1) the point (dx, dy) is: 0 at the centre, 1 on the edge, more outside.
double shapeDepth(int shape, double dx, double dy)
{
    const double rho = std::sqrt(dx * dx + dy * dy);
    switch (shape) {
    case kHexagon:
    case kPentagon: {
        const int n = shape == kHexagon ? 6 : 5;
        const double k = 2.0 * kPi / n;
        double a = std::atan2(dy, dx) + kPi / 2.0; // a vertex points up
        a = std::fmod(a, k);
        if (a < 0)
            a += k;
        return rho * std::cos(a - k / 2.0) / std::cos(kPi / n);
    }
    case kStar: { // a five-point star: outer tips at radius 1, notches at 0.42
        constexpr int kTips = 5;
        constexpr double kInner = 0.42;
        const double k = 2.0 * kPi / kTips;
        double a = std::atan2(dy, dx) + kPi / 2.0; // a tip points up
        a = std::fmod(a, k);
        if (a < 0)
            a += k;
        const double half = a <= k / 2.0 ? a : k - a; // distance from the nearest tip's axis
        const double p0x = 1.0, p0y = 0.0;
        const double p1x = kInner * std::cos(k / 2.0), p1y = kInner * std::sin(k / 2.0);
        const double mx = -(p1y - p0y), my = p1x - p0x;
        const double edge = (mx * p0x + my * p0y) / (mx * std::cos(half) + my * std::sin(half));
        return rho / std::max(0.2, edge) * 1.05;
    }
    case kHeart: {
        const double r = heartRadius(std::atan2(dy, dx));
        return rho / std::max(0.2, r) * 1.02;
    }
    default: return rho;
    }
}

// Soft-edged disc profile: 1 inside, fading to 0 across `soft` (0..1) of the radius at the edge; `rim`
// adds a brighter ring right at the edge, as real lens bokeh has.
inline double discProfile(double depth, double soft, double rim)
{
    const double inner = 1.0 - std::max(0.02, soft);
    const double body = 1.0 - smoothStep01((depth - inner) / std::max(1e-3, 1.0 - inner));
    const double rimTerm = rim * smoothStep01((depth - 0.62) / 0.3) * (1.0 - smoothStep01((depth - 0.98) / std::max(0.02, soft * 0.6)));
    return clampd(body * (1.0 + rimTerm), 0.0, 3.0);
}

// Draws one shape into the layer, adding `color * intensity * profile`.
void drawShape(Layer &layer, double cx, double cy, double r, int shape, double soft, double rim, const float color[3],
               double intensity, double hollow = 0.0)
{
    const int x0 = std::max(0, int(std::floor(cx - r * 1.25))), x1 = std::min(layer.w - 1, int(std::ceil(cx + r * 1.25)));
    const int y0 = std::max(0, int(std::floor(cy - r * 1.25))), y1 = std::min(layer.h - 1, int(std::ceil(cy + r * 1.25)));
    const double inv = 1.0 / std::max(0.5, r);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const double depth = shapeDepth(shape, (x - cx) * inv, (y - cy) * inv);
            if (depth > 1.2)
                continue;
            double p = discProfile(depth, soft, rim);
            if (hollow > 0.0)
                p *= 1.0 - hollow * (1.0 - smoothStep01((depth - 0.55) / 0.2));
            if (p > 0.0)
                layer.add(x, y, color, float(p * intensity));
        }
}

// ---------------------------------------------------------------------------------------------
// Bokeh
// ---------------------------------------------------------------------------------------------

QImage fxBokeh(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 1600, factor);
    const double shortSide = std::min(layer.w, layer.h);
    const int count = 6 + int(std::lround(v[0] * 2.2));
    const double baseR = shortSide * (0.012 + 0.07 * v[1] / 100.0);
    const double variation = v[2] / 100.0;
    const int shape = clampi(int(std::lround(v[3])), 0, 5);
    const double soft = 0.08 + 0.55 * v[4] / 100.0;
    const double rim = v[5] / 100.0 * 1.2;
    const double bright = v[6] / 100.0;
    const int colorMode = clampi(int(std::lround(v[7])), 0, 2);
    const Rgb tint = unpackColor(v[8]);
    const int spread = clampi(int(std::lround(v[9])), 0, 4);
    Rng rng(uint32_t(std::lround(v[10])) + 31u);
    const SceneTint scene(src);

    for (int i = 0; i < count; ++i) {
        if (job.cancelled())
            break;
        double fx = rng.unit(), fy = rng.unit();
        switch (spread) {
        case 1: { // toward the edges
            const double a = rng.range(0.0, 2.0 * kPi), rr = 0.38 + 0.62 * std::sqrt(rng.unit());
            fx = clampd(0.5 + 0.55 * rr * std::cos(a), 0.0, 1.0);
            fy = clampd(0.5 + 0.55 * rr * std::sin(a), 0.0, 1.0);
            break;
        }
        case 2: fx = clampd(0.5 + 0.22 * rng.gauss(), 0.0, 1.0); fy = clampd(0.5 + 0.22 * rng.gauss(), 0.0, 1.0); break;
        case 3: fy = 1.0 - fy * fy; break;
        case 4: fy = fy * fy; break;
        default: break;
        }
        const double sizeK = 1.0 + variation * (std::pow(rng.unit(), 2.2) * 2.4 - 0.45);
        const double r = std::max(2.0, baseR * sizeK);
        const double alpha = (0.25 + 0.75 * rng.unit()) * (0.35 + 0.65 * bright) * (0.6 + 0.4 * (baseR / r));
        float color[3];
        if (colorMode == 0) {
            scene.at(fx, fy, color);
        } else if (colorMode == 1) {
            colorTriple(tint, color);
        } else {
            colorTriple(hsvToRgb(rng.range(0, 360), 0.55, 1.0), color);
        }
        drawShape(layer, fx * (layer.w - 1), fy * (layer.h - 1), r, shape, soft, shape == kRing ? 0.25 : rim, color,
                  alpha * 0.55, shape == kRing ? 0.7 : 0.0);
    }
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[11]));
}

// ---------------------------------------------------------------------------------------------
// Destello de lente
// ---------------------------------------------------------------------------------------------

QImage fxFlare(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 1400, factor);
    const double w = layer.w, h = layer.h, shortSide = std::min(w, h);
    const double sx = v[0] / 100.0 * (w - 1), sy = v[1] / 100.0 * (h - 1);
    const double r0 = shortSide * (0.06 + 0.22 * v[2] / 100.0);
    const double bright = v[3] / 100.0 * 1.6;
    const int rayStyle = clampi(int(std::lround(v[4])), 0, 3);
    const double rayLength = shortSide * (0.15 + 0.85 * v[5] / 100.0);
    const double ghosts = v[6] / 100.0, halo = v[7] / 100.0, rainbow = v[8] / 100.0;
    float tint[3];
    colorTriple(unpackColor(v[9]), tint);
    const double turn = v[10] * kPi / 180.0;
    const double cx = w * 0.5, cy = h * 0.5;

    // streak directions
    std::vector<double> angles;
    if (rayStyle == 1) angles = {0.0, kPi / 2};
    else if (rayStyle == 2) angles = {0.0, kPi / 6, kPi / 3, kPi / 2, 2 * kPi / 3, 5 * kPi / 6};
    else if (rayStyle == 3) angles = {0.0};
    for (double &a : angles)
        a += turn;

    // ghosts along the line through the light and the picture's centre
    struct Ghost { double s, r, hue, power; int shape; };
    std::vector<Ghost> ghostList;
    if (ghosts > 0.0) {
        Rng rng(77);
        const double along[] = {-0.55, -0.25, 0.35, 0.62, 1.35, 1.9};
        for (int i = 0; i < 6; ++i)
            ghostList.push_back({along[i], r0 * rng.range(0.25, 0.8), rng.range(10, 330), rng.range(0.35, 1.0),
                                 i % 2 ? kHexagon : kCircle});
    }

    // the bright pieces that are drawn per pixel: core, streaks, halo ring
    rows(job, layer.h, [&](int y) {
        for (int x = 0; x < layer.w; ++x) {
            const double dx = x - sx, dy = y - sy;
            const double d = std::sqrt(dx * dx + dy * dy);
            double white = 1.25 * std::exp(-(d * d) / (0.16 * r0 * r0)) + 0.55 / (1.0 + (d * d) / (r0 * r0 * 0.9))
                           + 0.18 / (1.0 + d / (r0 * 2.2));
            double extra = 0.0;
            for (double a : angles) {
                const double ca = std::cos(a), sa = std::sin(a);
                const double perp = std::abs(-sa * dx + ca * dy), along = std::abs(ca * dx + sa * dy);
                const double thin = rayStyle == 3 ? shortSide * 0.006 : shortSide * 0.0035;
                const double len = rayStyle == 3 ? rayLength * 1.6 : rayLength;
                extra += 0.8 * std::exp(-perp / thin) * std::exp(-along / (len * 0.55));
            }
            float c[3] = {float((white + extra) * tint[0]), float((white + extra) * tint[1]), float((white + extra) * tint[2])};
            if (rayStyle == 3) { // anamorphic streaks lean blue
                c[2] += float(extra * 0.35);
                c[0] *= 0.92f;
            }
            if (halo > 0.0) {
                const double ringR = r0 * 2.1;
                const double t = (d - ringR) / (r0 * 0.14);
                const double ring = std::exp(-t * t) * 0.55 * halo;
                const Rgb hue = hsvToRgb(220 + clampd(t, -2.0, 2.0) * 60.0, 0.9, 1.0);
                const double rainbowMix = rainbow;
                c[0] += float(ring * (tint[0] * (1 - rainbowMix) + hue.r / 255.0 * rainbowMix));
                c[1] += float(ring * (tint[1] * (1 - rainbowMix) + hue.g / 255.0 * rainbowMix));
                c[2] += float(ring * (tint[2] * (1 - rainbowMix) + hue.b / 255.0 * rainbowMix));
            }
            float *p = layer.at(x, y);
            p[0] += float(c[0] * bright);
            p[1] += float(c[1] * bright);
            p[2] += float(c[2] * bright);
        }
    });
    for (const Ghost &g : ghostList) {
        const double gx = cx + (sx - cx) * g.s, gy = cy + (sy - cy) * g.s;
        const Rgb col = hsvToRgb(g.hue, 0.25 + 0.55 * rainbow, 1.0);
        float color[3];
        colorTriple(col, color);
        for (int k = 0; k < 3; ++k)
            color[k] = color[k] * 0.7f + tint[k] * 0.3f;
        drawShape(layer, gx, gy, g.r, g.shape, 0.5, 0.9, color, 0.5 * ghosts * g.power * bright, 0.0);
    }
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[11]));
}

// ---------------------------------------------------------------------------------------------
// Fuga de luz
// ---------------------------------------------------------------------------------------------

double valueNoise2(double x, double y, uint32_t salt)
{
    const int xi = int(std::floor(x)), yi = int(std::floor(y));
    const double fx = smoothStep01(x - xi), fy = smoothStep01(y - yi);
    const double a = hashUnit(hash2(xi, yi, salt)), b = hashUnit(hash2(xi + 1, yi, salt));
    const double c = hashUnit(hash2(xi, yi + 1, salt)), d = hashUnit(hash2(xi + 1, yi + 1, salt));
    return lerp(lerp(a, b, fx), lerp(c, d, fx), fy);
}

QImage fxLeak(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 1000, factor);
    const double w = layer.w, h = layer.h, diag = std::hypot(w, h);
    const int style = clampi(int(std::lround(v[0])), 0, 5);
    const double angle = v[1] * kPi / 180.0;
    const double size = 0.25 + 0.75 * v[2] / 100.0;
    const double strength = v[3] / 100.0;
    const double streaks = v[4] / 100.0;
    const uint32_t seed = uint32_t(std::lround(v[5]));
    const bool both = v[6] >= 0.5;

    // palettes: three colours, from the leak's heart to its fringe
    static const double palettes[6][3][3] = {
        {{255, 215, 120}, {255, 120, 40}, {220, 30, 40}},    // warm
        {{255, 210, 235}, {255, 90, 170}, {150, 40, 200}},   // pink
        {{255, 245, 190}, {255, 190, 60}, {230, 110, 20}},   // gold
        {{190, 220, 255}, {90, 80, 255}, {170, 40, 220}},    // blue and violet
        {{220, 255, 230}, {60, 230, 160}, {20, 140, 200}},   // mint
        {{255, 255, 180}, {255, 80, 120}, {60, 120, 255}},   // rainbow
    };
    struct Blob { double x, y, rx, ry, rot, power; int tone; };
    std::vector<Blob> blobs;
    auto addSide = [&](double a, uint32_t salt) {
        Rng rng(seed * 131u + salt);
        const double dirx = std::cos(a), diry = std::sin(a);
        // the leak enters from the edge facing `a`
        const double ex = w * 0.5 + dirx * w * 0.5, ey = h * 0.5 + diry * h * 0.5;
        for (int i = 0; i < 4; ++i) {
            const double slide = rng.range(-0.5, 0.5) * diag * 0.45;
            Blob b;
            b.x = ex - dirx * diag * 0.04 * i * size + (-diry) * slide;
            b.y = ey - diry * diag * 0.04 * i * size + dirx * slide;
            b.rx = diag * size * rng.range(0.16, 0.34);
            b.ry = diag * size * rng.range(0.1, 0.22);
            b.rot = a + rng.range(-0.8, 0.8);
            b.power = rng.range(0.6, 1.0) * (i == 0 ? 1.0 : 0.75);
            b.tone = style == 5 ? i % 3 : (i == 0 ? 0 : 1);
            blobs.push_back(b);
        }
    };
    addSide(angle, 1);
    if (both)
        addSide(angle + kPi, 2);

    rows(job, layer.h, [&](int y) {
        for (int x = 0; x < layer.w; ++x) {
            double r = 0, g = 0, b = 0;
            for (const Blob &bl : blobs) {
                const double dx = x - bl.x, dy = y - bl.y;
                const double ca = std::cos(bl.rot), sa = std::sin(bl.rot);
                const double u = (dx * ca + dy * sa) / bl.rx, t = (-dx * sa + dy * ca) / bl.ry;
                const double q = u * u + t * t;
                if (q > 9.0)
                    continue;
                const double fall = std::exp(-q * 1.1) * bl.power;
                // colour runs from the heart outward
                const double mix = clampd(std::sqrt(q) * 0.8, 0.0, 1.999);
                const int i0 = int(mix);
                const double f = mix - i0;
                const int turn = style == 5 ? bl.tone : 0; // the rainbow leak starts each blob on another colour
                const double *c0 = palettes[style][(i0 + turn) % 3];
                const double *c1 = palettes[style][(std::min(2, i0 + 1) + turn) % 3];
                r += fall * lerp(c0[0], c1[0], f);
                g += fall * lerp(c0[1], c1[1], f);
                b += fall * lerp(c0[2], c1[2], f);
            }
            if (r + g + b <= 0.0)
                continue;
            // film-like streaks across the leak and slow irregularities
            const double band = 1.0 - streaks * 0.65 * (0.5 + 0.5 * std::sin((x * std::cos(angle + 1.57) + y * std::sin(angle + 1.57)) * 0.045 + valueNoise2(x * 0.02, y * 0.02, seed) * 6.0));
            const double blotch = 0.65 + 0.7 * valueNoise2(x * 0.012 + 3, y * 0.012, seed + 5);
            const double k = strength * band * blotch / 255.0;
            float *p = layer.at(x, y);
            p[0] += float(r * k);
            p[1] += float(g * k);
            p[2] += float(b * k);
        }
    });
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[7]));
}

// ---------------------------------------------------------------------------------------------
// Rayos de luz
// ---------------------------------------------------------------------------------------------

QImage fxBeams(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 1200, factor);
    const double w = layer.w, h = layer.h, diag = std::hypot(w, h);
    const double sx = v[0] / 100.0 * (w - 1), sy = v[1] / 100.0 * (h - 1);
    const double dir = v[2] * kPi / 180.0;
    const double spread = v[3] * kPi / 180.0;
    const int count = clampi(int(std::lround(v[4])), 1, 30);
    const double length = diag * (0.15 + 0.95 * v[5] / 100.0);
    const double bright = v[6] / 100.0;
    const double soft = 0.15 + 0.85 * v[7] / 100.0;
    float tint[3];
    colorTriple(unpackColor(v[8]), tint);
    const double variation = v[9] / 100.0;
    Rng rng(uint32_t(std::lround(v[10])) + 5u);

    struct Band { double centre, width, power; };
    std::vector<Band> bands;
    for (int i = 0; i < count; ++i) {
        const double c = (count == 1 ? 0.0 : (double(i) / (count - 1) - 0.5)) * spread * 0.9 + (rng.unit() - 0.5) * variation * spread * 0.25;
        const double wd = spread / std::max(2.5, count * 1.0) * (0.35 + soft * 0.9) * (1.0 - variation * 0.5 + variation * rng.unit());
        bands.push_back({c, std::max(0.004, wd), rng.range(0.45, 1.0) * (1.0 - variation * 0.5 * rng.unit())});
    }
    rows(job, layer.h, [&](int y) {
        for (int x = 0; x < layer.w; ++x) {
            const double dx = x - sx, dy = y - sy;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = std::atan2(dy, dx) - dir;
            a = std::remainder(a, 2.0 * kPi);
            const double envelope = 1.0 - smoothStep01((std::abs(a) - spread * 0.5 * 0.7) / std::max(1e-3, spread * 0.5 * 0.3 + 1e-3));
            if (envelope <= 0.0)
                continue;
            double sum = 0.0;
            for (const Band &b : bands) {
                const double t = (a - b.centre) / b.width;
                sum += b.power * std::exp(-t * t);
            }
            const double haze = 0.12;
            const double radial = std::exp(-d / length) * (1.0 - std::exp(-d / (diag * 0.02 + 1.0)));
            const double k = (sum * 0.9 + haze) * envelope * radial * bright * 1.5;
            float *p = layer.at(x, y);
            p[0] += float(tint[0] * k);
            p[1] += float(tint[1] * k);
            p[2] += float(tint[2] * k);
        }
    });
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[11]));
}

// ---------------------------------------------------------------------------------------------
// Polvo / partículas
// ---------------------------------------------------------------------------------------------

QImage fxDust(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 2000, factor);
    const double shortSide = std::min(layer.w, layer.h);
    const int count = 20 + int(std::lround(v[0] * 20.0));
    const double baseR = shortSide * (0.0015 + 0.006 * v[1] / 100.0);
    const double variation = v[2] / 100.0;
    const double blurred = v[3] / 100.0;
    const double bright = v[4] / 100.0;
    float tint[3];
    colorTriple(unpackColor(v[5]), tint);
    Rng rng(uint32_t(std::lround(v[6])) + 91u);
    for (int i = 0; i < count; ++i) {
        if (job.cancelled())
            break;
        const double fx = rng.unit(), fy = rng.unit();
        const bool defocused = rng.unit() < blurred * 0.6;
        const double sizeK = 1.0 + variation * (std::pow(rng.unit(), 3.0) * 3.0 - 0.3);
        double r = std::max(1.2, baseR * sizeK * (defocused ? 2.6 + rng.unit() * 3.0 : 1.0));
        const double alpha = (defocused ? 0.18 : 0.55) * (0.3 + 0.7 * rng.unit()) * (0.3 + 0.9 * bright);
        const double warm = rng.range(-0.08, 0.08);
        const float color[3] = {float(std::min(1.0, tint[0] + warm)), tint[1], float(std::max(0.0, tint[2] - warm))};
        drawShape(layer, fx * (layer.w - 1), fy * (layer.h - 1), r, kCircle, defocused ? 0.7 : 0.45, defocused ? 0.4 : 0.0, color, alpha);
        if (!defocused && rng.unit() < 0.18) // a few specks catch the light with a small glow
            drawShape(layer, fx * (layer.w - 1), fy * (layer.h - 1), r * 4.5, kCircle, 0.95, 0.0, color, alpha * 0.18);
    }
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[7]));
}

// ---------------------------------------------------------------------------------------------
// Destellos (estrellitas)
// ---------------------------------------------------------------------------------------------

QImage fxSparkles(const Job &job, const QImage &src, const EffectValues &v, double)
{
    double factor;
    Layer layer = makeLayer(src, 1800, factor);
    const double shortSide = std::min(layer.w, layer.h);
    const int count = clampi(int(std::lround(v[0])), 1, 100);
    const double baseSize = shortSide * (0.02 + 0.12 * v[1] / 100.0);
    const int points = v[2] < 0.5 ? 4 : (v[2] < 1.5 ? 6 : 8);
    const double turn = v[3] * kPi / 180.0;
    const double bright = v[4] / 100.0;
    const bool onLights = v[5] >= 0.5;
    float tint[3];
    colorTriple(unpackColor(v[6]), tint);
    Rng rng(uint32_t(std::lround(v[7])) + 11u);

    // candidate positions: the brightest places of the picture (a coarse grid, best first) or anywhere
    std::vector<QPointF> spots;
    if (onLights) {
        const QImage small = src.scaled(96, 96, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
        struct Cell { double luma; int x, y; };
        std::vector<Cell> cells;
        for (int y = 1; y < small.height() - 1; ++y)
            for (int x = 1; x < small.width() - 1; ++x) {
                const uint8_t *p = small.constScanLine(y) + x * 4;
                const double l = lumaOf(p[0], p[1], p[2]);
                bool peak = true;
                for (int dy = -1; dy <= 1 && peak; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const uint8_t *q = small.constScanLine(y + dy) + (x + dx) * 4;
                        if ((dx || dy) && lumaOf(q[0], q[1], q[2]) > l)
                            peak = false;
                    }
                if (peak && l > 150)
                    cells.push_back({l, x, y});
            }
        std::sort(cells.begin(), cells.end(), [](const Cell &a, const Cell &b) { return a.luma > b.luma; });
        for (const Cell &c : cells)
            spots.push_back(QPointF((c.x + 0.5) / small.width(), (c.y + 0.5) / small.height()));
    }

    for (int i = 0; i < count; ++i) {
        double fx, fy;
        if (onLights && i < int(spots.size())) {
            fx = spots[size_t(i)].x();
            fy = spots[size_t(i)].y();
        } else {
            fx = rng.unit();
            fy = rng.unit();
        }
        const double size = baseSize * rng.range(0.45, 1.0) * (onLights && i < int(spots.size()) ? 1.0 - 0.5 * i / std::max(1, count) : 1.0);
        const double px = fx * (layer.w - 1), py = fy * (layer.h - 1);
        const double strength = bright * rng.range(0.7, 1.0) * 1.35;
        const double rot = turn + rng.range(-0.08, 0.08);
        const int x0 = std::max(0, int(px - size * 2.0)), x1 = std::min(layer.w - 1, int(px + size * 2.0));
        const int y0 = std::max(0, int(py - size * 2.0)), y1 = std::min(layer.h - 1, int(py + size * 2.0));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const double dx = x - px, dy = y - py, d = std::sqrt(dx * dx + dy * dy);
                double k = 1.1 * std::exp(-(d * d) / (0.025 * size * size)) + 0.25 * std::exp(-d / (size * 0.18));
                for (int j = 0; j < points; ++j) {
                    const double a = rot + kPi * j / points;
                    const double perp = std::abs(-std::sin(a) * dx + std::cos(a) * dy), along = std::abs(std::cos(a) * dx + std::sin(a) * dy);
                    const double longer = (j % 2 == 0 || points == 4) ? 1.0 : 0.62;
                    k += 1.3 * std::exp(-perp / (size * 0.03)) * std::exp(-along / (size * 0.6 * longer));
                }
                if (k > 0.002)
                    layer.add(x, y, tint, float(k * strength));
            }
    }
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[8]));
}

// ---------------------------------------------------------------------------------------------
// Resplandor (bloom)
// ---------------------------------------------------------------------------------------------

QImage fxGlow(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    double factor;
    Layer layer = makeLayer(src, 1100, factor);
    const double threshold = v[0] / 100.0;
    const double sigma = std::max(2.0, longSide / factor * (0.004 + v[1] / 100.0 * 0.05));
    const double intensity = v[2] / 100.0 * 2.2;
    float tint[3];
    colorTriple(unpackColor(v[3]), tint);

    // the picture's highlights at the layer's size, softened
    const QImage small = src.scaled(layer.w, layer.h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
    Plane plane(layer.w, layer.h, 4);
    for (int y = 0; y < layer.h; ++y) {
        const uint8_t *s = small.constScanLine(y);
        uint8_t *d = plane.row(y);
        for (int x = 0; x < layer.w; ++x) {
            const double l = lumaOf(s[x * 4], s[x * 4 + 1], s[x * 4 + 2]) / 255.0;
            const double k = clampd((l - threshold) / std::max(0.02, 1.0 - threshold), 0.0, 1.0);
            const double a = s[x * 4 + 3] / 255.0;
            d[x * 4] = toByte(s[x * 4] * k * a);
            d[x * 4 + 1] = toByte(s[x * 4 + 1] * k * a);
            d[x * 4 + 2] = toByte(s[x * 4 + 2] * k * a);
            d[x * 4 + 3] = 255;
        }
    }
    const Plane blurred = gaussianBlur(job, plane, sigma);
    const Plane wide = gaussianBlur(job, blurred, sigma * 2.2); // a tight core and a wide veil
    for (int y = 0; y < layer.h; ++y) {
        const uint8_t *a = blurred.row(y), *b = wide.row(y);
        for (int x = 0; x < layer.w; ++x) {
            float *p = layer.at(x, y);
            for (int c = 0; c < 3; ++c)
                p[c] = float((a[x * 4 + c] * 0.65 + b[x * 4 + c] * 0.7) / 255.0 * intensity * tint[c]);
        }
    }
    return layLight(job, src, layer, factor, 1.0, blendFrom(v[4]));
}

// ---------------------------------------------------------------------------------------------
// Foco de luz
// ---------------------------------------------------------------------------------------------

QImage fxSpotlight(const Job &job, const QImage &src, const EffectValues &v)
{
    const double w = src.width(), h = src.height();
    const double cx = v[0] / 100.0 * (w - 1), cy = v[1] / 100.0 * (h - 1);
    const double radius = std::max(2.0, std::hypot(w, h) * 0.5 * (0.1 + 0.9 * v[2] / 100.0));
    const double soft = 0.05 + 0.95 * v[3] / 100.0;
    const double bright = v[4] / 100.0;
    const double dark = v[5] / 100.0;
    float tint[3];
    colorTriple(unpackColor(v[6]), tint);
    const double inner = 1.0 - soft;

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double dist = std::hypot(x - cx, y - cy) / radius;
            const double m = 1.0 - smoothStep01((dist - inner) / std::max(1e-3, 1.0 - inner));
            const double shade = 1.0 - dark * (1.0 - m);
            for (int c = 0; c < 3; ++c) {
                double p = s[x * 4 + c] * shade;
                if (bright > 0.0)
                    p += (255.0 - p) * tint[c] * bright * m * 0.7; // screen the light in
                else if (bright < 0.0)
                    p *= 1.0 + bright * m * 0.7;
                d[x * 4 + c] = toByte(p);
            }
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

} // namespace

void addLightSpecs(std::vector<EffectSpec> &out)
{
    {
        EffectSpec s = makeSpec(
            "bokeh", "Bokeh", "light",
            {slider("Cantidad", 0, 100, 45), slider("Tamaño", 0, 100, 40), slider("Variación de tamaño", 0, 100, 60),
             choice("Forma", {"Círculo", "Hexágono", "Pentágono", "Corazón", "Estrella", "Anillo"}, 0),
             slider("Suavidad del borde", 0, 100, 30), slider("Borde brillante", 0, 100, 35), slider("Brillo", 0, 100, 70),
             choice("Colores", {"De la imagen", "Un tinte", "Arcoíris"}, 0),
             shownWhen(colorParam("Tinte", 255, 220, 170), 7, {1}),
             choice("Distribución", {"Toda la imagen", "Hacia los bordes", "En el centro", "Abajo", "Arriba"}, 0),
             seedParam("Semilla", 4), blendParam(int(BlendMode::Screen))});
        s.presets = {{"Círculos", {{3, 0}, {4, 30}, {5, 35}}},
                     {"Hexágonos", {{3, 1}, {4, 15}, {5, 55}}},
                     {"Corazones", {{3, 3}, {4, 35}, {5, 20}, {1, 55}}},
                     {"Estrellas", {{3, 4}, {4, 25}, {5, 30}}},
                     {"Burbujas", {{3, 5}, {4, 20}, {5, 40}}}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "flare", "Destello de lente", "light",
            {slider("Posición X", 0, 100, 72), slider("Posición Y", 0, 100, 28), slider("Tamaño", 0, 100, 50),
             slider("Brillo", 0, 100, 75), choice("Rayos", {"Ninguno", "Cruz", "Estrella", "Anamórfico"}, 2),
             shownWhen(slider("Largo de los rayos", 0, 100, 55), 4, {1, 2, 3}), slider("Reflejos", 0, 100, 60),
             slider("Halo", 0, 100, 50), slider("Arcoíris", 0, 100, 40), colorParam("Color", 255, 235, 200),
             slider("Giro", -90, 90, 0, "°", true), blendParam(int(BlendMode::Screen))});
        s.overlays = {{EffectOverlay::Point, 0, 1, "Posición de la luz"}};
        s.presets = {{"Sol", {{4, 2}, {5, 60}, {6, 60}, {7, 45}, {9, double(packColor(255, 236, 200))}}},
                     {"Cinematográfico", {{4, 3}, {5, 80}, {6, 35}, {7, 30}, {8, 55}, {9, double(packColor(210, 225, 255))}}},
                     {"Estrella", {{4, 1}, {5, 45}, {6, 20}, {7, 15}, {2, 25}, {9, double(packColor(255, 255, 255))}}},
                     {"Atardecer", {{4, 2}, {5, 50}, {6, 70}, {7, 60}, {9, double(packColor(255, 170, 110))}}}};
        s.sample = {{3, 90}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "leak", "Fuga de luz", "light",
            {choice("Estilo", {"Cálida", "Rosa", "Dorada", "Azul y violeta", "Menta", "Arcoíris"}, 0),
             slider("Origen", 0, 360, 215, "°", true), slider("Tamaño", 0, 100, 55), slider("Intensidad", 0, 100, 75),
             slider("Rayas de película", 0, 100, 35), seedParam("Semilla", 3), toggle("También del lado opuesto"),
             blendParam(int(BlendMode::Screen))});
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "beams", "Rayos de luz", "light",
            {slider("Origen X", 0, 100, 18), slider("Origen Y", 0, 100, 4), slider("Dirección", -180, 180, 52, "°", true),
             slider("Abertura", 5, 180, 70, "°", true), slider("Cantidad de rayos", 1, 30, 9, "", true),
             slider("Longitud", 0, 100, 70), slider("Brillo", 0, 100, 65), slider("Suavidad", 0, 100, 55),
             colorParam("Color", 255, 240, 200), slider("Variación", 0, 100, 60), seedParam("Semilla", 6),
             blendParam(int(BlendMode::Screen))});
        s.overlays = {{EffectOverlay::Point, 0, 1, "Origen de la luz"}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "dust", "Polvo y partículas", "light",
            {slider("Cantidad", 0, 100, 50), slider("Tamaño", 0, 100, 35), slider("Variación", 0, 100, 60),
             slider("Desenfoque", 0, 100, 40), slider("Brillo", 0, 100, 70), colorParam("Color", 255, 244, 220),
             seedParam("Semilla", 9), blendParam(int(BlendMode::Screen))});
        s.presets = {{"Polvo fino", {{0, 75}, {1, 20}, {3, 25}, {4, 60}}},
                     {"Partículas grandes", {{0, 30}, {1, 70}, {3, 65}, {4, 75}}},
                     {"Nieve", {{0, 65}, {1, 45}, {3, 55}, {4, 85}, {5, double(packColor(255, 255, 255))}}},
                     {"Luciérnagas", {{0, 25}, {1, 40}, {3, 45}, {4, 90}, {5, double(packColor(200, 255, 120))}}}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "sparkles", "Destellos", "light",
            {slider("Cantidad", 1, 100, 22, "", true), slider("Tamaño", 0, 100, 45),
             choice("Puntas", {"4 puntas", "6 puntas", "8 puntas"}, 0), slider("Giro", -90, 90, 0, "°", true),
             slider("Brillo", 0, 100, 80), choice("Colocación", {"Al azar", "En las luces"}, 1),
             colorParam("Color", 255, 248, 230), seedParam("Semilla", 2), blendParam(int(BlendMode::Screen))});
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("glow", "Resplandor", "light",
                                {slider("Umbral", 0, 100, 55), slider("Radio", 0, 100, 45), slider("Intensidad", 0, 100, 80),
                                 colorParam("Color", 255, 240, 220), blendParam(int(BlendMode::Screen))});
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("spotlight", "Foco de luz", "light",
                                {slider("Posición X", 0, 100, 50), slider("Posición Y", 0, 100, 42), slider("Radio", 0, 100, 45),
                                 slider("Suavidad", 0, 100, 55), slider("Luz", -100, 100, 35), slider("Oscurecer fuera", 0, 100, 45),
                                 colorParam("Color", 255, 240, 210)});
        s.overlays = {{EffectOverlay::Point, 0, 1, "Centro del foco"}};
        out.push_back(std::move(s));
    }
}

bool renderLight(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result)
{
    if (id == QLatin1String("bokeh"))
        result = fxBokeh(job, src, v, longSide);
    else if (id == QLatin1String("flare"))
        result = fxFlare(job, src, v, longSide);
    else if (id == QLatin1String("leak"))
        result = fxLeak(job, src, v, longSide);
    else if (id == QLatin1String("beams"))
        result = fxBeams(job, src, v, longSide);
    else if (id == QLatin1String("dust"))
        result = fxDust(job, src, v, longSide);
    else if (id == QLatin1String("sparkles"))
        result = fxSparkles(job, src, v, longSide);
    else if (id == QLatin1String("glow"))
        result = fxGlow(job, src, v, longSide);
    else if (id == QLatin1String("spotlight"))
        result = fxSpotlight(job, src, v);
    else
        return false;
    return true;
}

} // namespace core::edit::fxk
