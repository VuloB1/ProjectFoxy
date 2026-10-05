#include "EffectsCommon.h"

#include <algorithm>
#include <cmath>

// The lens correction: distortion, lateral chromatic aberration and vignetting from calibration
// numbers (the Lensfun models), applied in one pass. The numbers come from the lens tool, which
// looks them up in the database (core/lens/LensDatabase.h); here they are just values.
//
// Radii are measured in "Hugin units" (what the Lensfun models are written in): r = 1 is half the
// sensor's short side, so r_px / (half the diagonal) * kr, where kr folds together the sensor shape
// and the ratio of the calibration's crop factor to the camera's. The tool already expressed the
// chromatic aberration and vignetting terms in these same units.

namespace core::edit::fxk {

namespace {

struct LensMath {
    int model;
    double a, b, c;       // distortion terms, already divided by d^n like Lensfun does
    double vr, vb, cr, cb, br, bb;
    double k1, k2, k3;
    double strength;

    // The distorted (source) radius divided by the corrected one, for a corrected radius r.
    double distortion(double r) const
    {
        double f = 1.0;
        switch (model) {
        case 1: f = 1.0 + a * r * r; break;                          // poly3
        case 2: f = 1.0 + a * r * r + b * r * r * r * r; break;       // poly5
        case 3: f = a * r * r * r + b * r * r + c * r + 1.0; break;   // ptlens
        default: break;
        }
        return 1.0 + strength * (f - 1.0);
    }
    double tcaRed(double g) const { return 1.0 + strength * ((br * g * g + cr * g + vr) - 1.0); }
    double tcaBlue(double g) const { return 1.0 + strength * ((bb * g * g + cb * g + vb) - 1.0); }
    double vignetting(double r) const
    {
        const double r2 = r * r;
        const double c = std::max(0.2, 1.0 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2);
        return 1.0 + strength * (1.0 / c - 1.0);
    }
};

LensMath lensMathFrom(const EffectValues &v)
{
    LensMath m{};
    m.model = clampi(int(std::lround(v[0])), 0, 3);
    m.a = v[1];
    m.b = v[2];
    m.c = v[3];
    // Lensfun keeps the focal length by scaling the corrected radius by 1/d: a' = a/d^4, b' = b/d^3, c' = c/d^2
    if (m.model == 3) {
        const double d = 1.0 - v[1] - v[2] - v[3];
        if (std::abs(d) > 1e-3) {
            m.a = v[1] / std::pow(d, 4.0);
            m.b = v[2] / std::pow(d, 3.0);
            m.c = v[3] / std::pow(d, 2.0);
        }
    } else if (m.model == 1) {
        const double d = 1.0 - v[1];
        if (std::abs(d) > 1e-3)
            m.a = v[1] / std::pow(d, 3.0);
    }
    m.vr = v[4]; m.vb = v[5]; m.cr = v[6]; m.cb = v[7]; m.br = v[8]; m.bb = v[9];
    m.k1 = v[10]; m.k2 = v[11]; m.k3 = v[12];
    m.strength = clampd(v[15] / 100.0, 0.0, 1.0);
    return m;
}

// The smallest zoom (>= 1) that leaves no empty corner or edge after the correction.
double autoScale(const LensMath &m, double w, double h, double toU)
{
    const double halfW = w * 0.5, halfH = h * 0.5;
    auto inside = [&](double scale) {
        constexpr int kSteps = 48;
        for (int side = 0; side < 4; ++side)
            for (int i = 0; i <= kSteps; ++i) {
                const double t = double(i) / kSteps;
                double px, py;
                switch (side) {
                case 0: px = -halfW + t * w; py = -halfH; break;
                case 1: px = -halfW + t * w; py = halfH; break;
                case 2: px = -halfW; py = -halfH + t * h; break;
                default: px = halfW; py = -halfH + t * h; break;
                }
                const double ux = px * toU / scale, uy = py * toU / scale;
                const double f = m.distortion(std::hypot(ux, uy));
                const double gx = ux * f, gy = uy * f;
                const double g = std::hypot(gx, gy);
                for (double tca : {1.0, m.tcaRed(g), m.tcaBlue(g)}) {
                    if (std::abs(gx * tca / toU) > halfW - 0.5 || std::abs(gy * tca / toU) > halfH - 0.5)
                        return false;
                }
            }
        return true;
    };
    if (inside(1.0))
        return 1.0;
    double lo = 1.0, hi = 4.0;
    if (!inside(hi))
        return hi;
    for (int i = 0; i < 30; ++i) {
        const double mid = 0.5 * (lo + hi);
        (inside(mid) ? hi : lo) = mid;
    }
    return hi * 1.001;
}

} // namespace

QImage fxLens(const Job &job, const QImage &src, const EffectValues &v)
{
    const int w = src.width(), h = src.height();
    const LensMath m = lensMathFrom(v);
    const double halfDiag = std::hypot(double(w), double(h)) * 0.5;
    const double toU = v[13] / halfDiag;            // pixels -> Hugin units
    const double scale = v[14] >= 0.5 ? autoScale(m, w, h, toU) : 1.0;
    const bool hasVignette = m.k1 != 0.0 || m.k2 != 0.0 || m.k3 != 0.0;

    const Plane p = planeFrom(src.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    QImage out(w, h, QImage::Format_RGBA8888_Premultiplied);
    out.bits();
    rows(job, h, [&](int y) {
        uint8_t *d = out.scanLine(y);
        const double py = y + 0.5 - h * 0.5;
        for (int x = 0; x < w; ++x) {
            const double px = x + 0.5 - w * 0.5;
            const double ux = px * toU / scale, uy = py * toU / scale;
            const double f = m.distortion(std::hypot(ux, uy));
            const double gx = ux * f, gy = uy * f; // where this pixel is in the source, in Hugin units
            const double g = std::hypot(gx, gy);
            const double fr = m.tcaRed(g), fb = m.tcaBlue(g);
            float cg[4], cr[4], cb[4];
            auto sample = [&](double sx, double sy, float out4[4]) {
                const double xs = w * 0.5 + sx / toU - 0.5, ys = h * 0.5 + sy / toU - 0.5;
                // outside the picture: transparent, with a soft pixel-wide edge
                const double inside = std::min({xs + 0.5, w - 0.5 - xs, ys + 0.5, h - 0.5 - ys});
                const double cov = clampd(inside + 0.5, 0.0, 1.0);
                for (int k = 0; k < 4; ++k)
                    out4[k] = 0.f;
                if (cov > 0.0) {
                    accumulate(p, xs, ys, out4);
                    for (int k = 0; k < 4; ++k)
                        out4[k] *= float(cov);
                }
            };
            sample(gx, gy, cg);
            if (fr == 1.0 && fb == 1.0) {
                cr[0] = cg[0];
                cb[2] = cg[2];
                cr[3] = cb[3] = cg[3];
            } else {
                sample(gx * fr, gy * fr, cr);
                sample(gx * fb, gy * fb, cb);
            }
            const float a = std::max({cr[3], cg[3], cb[3]});
            float rr = cr[0], gg = cg[1], bb = cb[2];
            if (hasVignette) {
                const double mult = m.vignetting(g);
                rr = float(rr * mult);
                gg = float(gg * mult);
                bb = float(bb * mult);
            }
            // premultiplied: a colour cannot be brighter than its alpha
            d[x * 4] = toByte(std::min(rr, a));
            d[x * 4 + 1] = toByte(std::min(gg, a));
            d[x * 4 + 2] = toByte(std::min(bb, a));
            d[x * 4 + 3] = toByte(a);
        }
    });
    QImage result = out.convertToFormat(QImage::Format_RGBA8888);
    return result;
}

EffectSpec lensSpec()
{
    EffectSpec s = makeSpec(
        "lens", "Corrección de lente", "distort",
        {slider("Modelo de distorsión", 0, 3, 1, "", true), slider("Término a", -2, 2, 0.06, ""),
         slider("Término b", -2, 2, 0, ""), slider("Término c", -2, 2, 0, ""), slider("Aberración: vr", 0.9, 1.1, 1.0, ""),
         slider("Aberración: vb", 0.9, 1.1, 1.0, ""), slider("Aberración: cr", -0.5, 0.5, 0, ""),
         slider("Aberración: cb", -0.5, 0.5, 0, ""), slider("Aberración: br", -1, 1, 0, ""),
         slider("Aberración: bb", -1, 1, 0, ""), slider("Viñeta: k1", -10, 10, -0.4, ""),
         slider("Viñeta: k2", -10, 10, 0, ""), slider("Viñeta: k3", -10, 10, 0, ""),
         slider("Escala de radios", 0.05, 10, 1.4, ""), toggle("Ajustar para quitar los bordes vacíos", true),
         slider("Fuerza", 0, 100, 100)});
    s.hidden = true; // driven by the lens tool, not picked from the grid
    return s;
}

} // namespace core::edit::fxk
