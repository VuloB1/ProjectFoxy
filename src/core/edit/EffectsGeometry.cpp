#include "Blend.h"
#include "EffectsCommon.h"

#include <algorithm>
#include <cmath>

// Effects that move pixels around and change the size of the picture: stretch and perspective (and,
// in a file of its own, the lens correction).

namespace core::edit::fxk {

namespace {

// ---------------------------------------------------------------------------------------------
// Estirar: the band between two guides is scaled, the rest is left as it is.
// ---------------------------------------------------------------------------------------------

// Where every source pixel of one axis lands in the output: pixel i occupies [cum[i], cum[i + 1]).
struct AxisMap {
    std::vector<double> cum;
    double total = 0.0;
    int outLength() const { return std::max(2, int(std::lround(total))); }
};

// `g1`, `g2` are the guides (0..1 along the axis), `scale` how much the band between them is scaled,
// `smooth` (0..1) how gradually the scale changes at the guides and `keep` squeezes the parts outside
// the band so the whole axis keeps its length.
AxisMap buildAxis(int n, double g1, double g2, double scale, double smooth, bool keep)
{
    g2 = std::max(g2, g1 + 0.02);
    const double a1 = g1 * n, a2 = g2 * n;
    const double halfBand = (a2 - a1) * 0.5;
    const double t = smooth * halfBand; // half-width of each transition
    std::vector<double> s(static_cast<size_t>(n));
    std::vector<double> inside(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double x = i + 0.5;
        double m;
        if (t < 0.5) {
            m = x >= a1 && x < a2 ? 1.0 : 0.0;
        } else {
            m = smoothStep01((x - (a1 - t)) / (2.0 * t)) * (1.0 - smoothStep01((x - (a2 - t)) / (2.0 * t)));
        }
        inside[size_t(i)] = m;
        s[size_t(i)] = 1.0 + (scale - 1.0) * m;
    }
    double f = 1.0;
    if (keep) {
        double inSum = 0.0, outSum = 0.0;
        for (int i = 0; i < n; ++i) {
            inSum += s[size_t(i)] * inside[size_t(i)];
            outSum += s[size_t(i)] * (1.0 - inside[size_t(i)]);
        }
        if (outSum > 1e-6)
            f = clampd((n - inSum) / outSum, 0.05, 20.0);
    }
    AxisMap map;
    map.cum.assign(size_t(n) + 1, 0.0);
    for (int i = 0; i < n; ++i)
        map.cum[size_t(i) + 1] = map.cum[size_t(i)] + s[size_t(i)] * lerp(f, 1.0, inside[size_t(i)]);
    map.total = map.cum[size_t(n)];
    return map;
}

// For each output pixel boundary 0..L: the source coordinate it comes from (in source pixel units,
// 0 = the left edge of the first pixel).
std::vector<double> sourceOfOutput(const AxisMap &map, int n)
{
    const int L = map.outLength();
    std::vector<double> src(size_t(L) + 1);
    for (int o = 0; o <= L; ++o) {
        const double pos = double(o) * map.total / L;
        const auto it = std::upper_bound(map.cum.begin(), map.cum.end(), pos);
        const int i = clampi(int(it - map.cum.begin()) - 1, 0, n - 1);
        const double span = map.cum[size_t(i) + 1] - map.cum[size_t(i)];
        src[size_t(o)] = i + (span > 1e-12 ? (pos - map.cum[size_t(i)]) / span : 0.0);
    }
    return src;
}

// Averages the source over the footprint of an output pixel (up to `taps` samples along each axis)
// so that a band that is being squeezed does not alias.
QImage fxStretch(const Job &job, const QImage &src, const EffectValues &v)
{
    const int w = src.width(), h = src.height();
    const AxisMap mx = buildAxis(w, v[0] / 100.0, v[1] / 100.0, v[4] / 100.0, v[6] / 100.0, v[7] >= 0.5);
    const AxisMap my = buildAxis(h, v[2] / 100.0, v[3] / 100.0, v[5] / 100.0, v[6] / 100.0, v[7] >= 0.5);
    const int W = mx.outLength(), H = my.outLength();
    const std::vector<double> sx = sourceOfOutput(mx, w), sy = sourceOfOutput(my, h);

    const Plane p = planeFrom(src.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    QImage out(W, H, QImage::Format_RGBA8888_Premultiplied);
    out.bits();
    rows(job, H, [&](int y) {
        uint8_t *d = out.scanLine(y);
        const double y0 = sy[size_t(y)], y1 = sy[size_t(y) + 1];
        const int ty = clampi(int(std::ceil(std::abs(y1 - y0))), 1, 6);
        for (int x = 0; x < W; ++x) {
            const double x0 = sx[size_t(x)], x1 = sx[size_t(x) + 1];
            const int tx = clampi(int(std::ceil(std::abs(x1 - x0))), 1, 6);
            float acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < ty; ++j) {
                const double py = lerp(y0, y1, (j + 0.5) / ty) - 0.5;
                for (int i = 0; i < tx; ++i) {
                    const double px = lerp(x0, x1, (i + 0.5) / tx) - 0.5;
                    accumulate(p, px, py, acc);
                }
            }
            const float inv = 1.f / float(tx * ty);
            for (int c = 0; c < 4; ++c)
                d[x * 4 + c] = toByte(acc[c] * inv);
        }
    });
    return out.convertToFormat(QImage::Format_RGBA8888);
}

// ---------------------------------------------------------------------------------------------
// Perspectiva: a keystone correction along one axis at a time.
// ---------------------------------------------------------------------------------------------

// How strong the keystone is for a slider value: PhotoScape's maximum (slider 50) shrinks the far
// edge to 0.708 of the near one and a 1000x800 picture to 1000x565, i.e. kappa = 0.416.
inline double keystoneKappa(double slider) { return 0.416 * std::abs(slider) / 50.0; }

// One keystone stage. `vertical`: the top edge keeps its width and the bottom narrows (kappa > 0);
// otherwise the left edge keeps its height and the right one shortens. `flip` reverses which end keeps its size.
// `crop` cuts the picture down to the part that is still a full rectangle.
QImage keystone(const Job &job, const QImage &src, double kappa, bool vertical, bool flip, bool crop, const Rgb *background)
{
    if (kappa <= 1e-9)
        return src;
    QImage image = src;
    if (flip)
        image = image.mirrored(!vertical, vertical);
    // work vertically; a horizontal stage is the same thing turned by a quarter
    if (!vertical)
        image = image.transformed(QTransform().rotate(90));
    const int w = image.width(), h = image.height();
    const int outH = std::max(2, int(std::lround(h / (1.0 + kappa))));
    const int outW = crop ? std::max(2, int(std::lround(w / (1.0 + kappa)))) : w;
    const Plane p = planeFrom(image.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    QImage out(outW, outH, QImage::Format_RGBA8888_Premultiplied);
    out.bits();
    // Output row y' (in pixels) shows the source row y with y' = y / (1 + kappa * y / h) - the near
    // edge keeps its width and each row further away is squeezed a little more.
    rows(job, outH, [&](int y) {
        uint8_t *d = out.scanLine(y);
        const double yo = y + 0.5;
        const double ys = yo / (1.0 - kappa * yo / h);        // the source row it comes from (pixel-edge coordinate)
        const double stretch = 1.0 + kappa * ys / h;           // how much narrower that row became
        for (int x = 0; x < outW; ++x) {
            const double xo = (x + 0.5) - outW * 0.5;           // from the middle, in output pixels
            const double xs = w * 0.5 + xo * stretch;           // the source column (pixel-edge coordinate)
            // a soft one-pixel edge where the source point leaves the picture
            const double inside = std::min(xs, w - xs);
            const double cov = clampd(inside + 0.5, 0.0, 1.0);
            float c[4] = {0, 0, 0, 0};
            if (cov > 0.0) {
                accumulate(p, xs - 0.5, ys - 0.5, c);
                for (int k = 0; k < 4; ++k)
                    c[k] *= float(cov);
            }
            if (background) {
                const float a = c[3] / 255.f;
                d[x * 4] = toByte(c[0] + background->r * (1.f - a));
                d[x * 4 + 1] = toByte(c[1] + background->g * (1.f - a));
                d[x * 4 + 2] = toByte(c[2] + background->b * (1.f - a));
                d[x * 4 + 3] = 255;
            } else {
                for (int k = 0; k < 4; ++k)
                    d[x * 4 + k] = toByte(c[k]);
            }
        }
    });
    QImage result = out.convertToFormat(QImage::Format_RGBA8888);
    if (!vertical)
        result = result.transformed(QTransform().rotate(-90));
    if (flip)
        result = result.mirrored(!vertical, vertical);
    return result;
}

QImage fxPerspective(const Job &job, const QImage &src, const EffectValues &v)
{
    const bool crop = v[2] >= 0.5;
    const bool useBackground = v[3] >= 0.5;
    const Rgb bg = unpackColor(v[4]);
    const Rgb *background = useBackground ? &bg : nullptr;
    QImage image = src.format() == QImage::Format_RGBA8888 ? src : src.convertToFormat(QImage::Format_RGBA8888);
    image = keystone(job, image, keystoneKappa(v[0]), true, v[0] < 0, crop, background);
    image = keystone(job, image, keystoneKappa(v[1]), false, v[1] < 0, crop, background);
    return image;
}

QSize perspectiveOutputSize(const EffectValues &v, QSize in)
{
    int w = in.width(), h = in.height();
    const double kv = keystoneKappa(v[0]), kh = keystoneKappa(v[1]);
    const bool crop = v[2] >= 0.5;
    if (kv > 1e-9) {
        h = std::max(2, int(std::lround(h / (1.0 + kv))));
        if (crop)
            w = std::max(2, int(std::lround(w / (1.0 + kv))));
    }
    if (kh > 1e-9) {
        w = std::max(2, int(std::lround(w / (1.0 + kh))));
        if (crop)
            h = std::max(2, int(std::lround(h / (1.0 + kh))));
    }
    return {w, h};
}

} // namespace

void addGeometrySpecs(std::vector<EffectSpec> &out)
{
    {
        EffectSpec s = makeSpec("stretch", "Estirar", "distort",
                                {slider("Guía izquierda", 0, 100, 35), slider("Guía derecha", 0, 100, 65),
                                 slider("Guía superior", 0, 100, 35), slider("Guía inferior", 0, 100, 65),
                                 slider("Horizontal", 10, 300, 150), slider("Vertical", 10, 300, 100),
                                 slider("Transición", 0, 100, 0), toggle("Mantener el tamaño")});
        s.changesSize = true;
        s.overlays = {{EffectOverlay::VLine, 0, -1, "Guía izquierda", "stretchX"},
                      {EffectOverlay::VLine, 1, -1, "Guía derecha", "stretchX"},
                      {EffectOverlay::HLine, -1, 2, "Guía superior", "stretchY"},
                      {EffectOverlay::HLine, -1, 3, "Guía inferior", "stretchY"}};
        s.presets = {{"Ensanchar el centro", {{4, 160}, {5, 100}}},
                     {"Alargar el centro", {{4, 100}, {5, 160}}},
                     {"Adelgazar el centro", {{4, 65}, {5, 100}}},
                     {"Alargar piernas", {{2, 55}, {3, 80}, {4, 100}, {5, 130}, {6, 40}}}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("perspective", "Perspectiva", "distort",
                                {slider("Vertical", -100, 100, 35), slider("Horizontal", -100, 100, 0), toggle("Recortar bordes"),
                                 choice("Fondo", {"Transparente", "Color"}, 0),
                                 shownWhen(colorParam("Color de fondo", 255, 255, 255), 3, {1})});
        s.changesSize = true;
        out.push_back(std::move(s));
    }
    out.push_back(lensSpec());
    out.push_back(frameSpec());
}

bool renderGeometry(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double,
                    QImage &result)
{
    if (id == QLatin1String("stretch"))
        result = fxStretch(job, src, v);
    else if (id == QLatin1String("perspective"))
        result = fxPerspective(job, src, v);
    else if (id == QLatin1String("lens"))
        result = fxLens(job, src, v);
    else if (id == QLatin1String("frame"))
        result = fxFrameRender(job, src, v);
    else
        return false;
    return true;
}

QSize geometryOutputSize(const QString &id, const EffectValues &v, QSize input)
{
    if (id == QLatin1String("stretch")) {
        const AxisMap mx = buildAxis(input.width(), v[0] / 100.0, v[1] / 100.0, v[4] / 100.0, v[6] / 100.0, v[7] >= 0.5);
        const AxisMap my = buildAxis(input.height(), v[2] / 100.0, v[3] / 100.0, v[5] / 100.0, v[6] / 100.0, v[7] >= 0.5);
        return {mx.outLength(), my.outLength()};
    }
    if (id == QLatin1String("perspective"))
        return perspectiveOutputSize(v, input);
    if (id == QLatin1String("frame"))
        return frameOutputSize(v, input);
    return input;
}

} // namespace core::edit::fxk
