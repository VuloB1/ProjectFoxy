#include "Blend.h"
#include "EffectsCommon.h"

#include <algorithm>
#include <cmath>
#include <deque>

// The "Color" family: black and white, dehaze, document clean-up, chromatic aberration, cellophane.

namespace core::edit::fxk {

namespace {

// ---------------------------------------------------------------------------------------------
// Small float-plane toolbox for the algorithms that work on a shrunken copy (dehaze, document).
// ---------------------------------------------------------------------------------------------

FloatPlane makeFloat(int w, int h, float fill = 0.f)
{
    FloatPlane p;
    p.w = w;
    p.h = h;
    p.d.assign(size_t(w) * h, fill);
    return p;
}

// Mean over a (2r+1) square, border pixels repeated.
FloatPlane boxMean(const FloatPlane &src, int r)
{
    if (r <= 0)
        return src;
    FloatPlane tmp = makeFloat(src.w, src.h), out = makeFloat(src.w, src.h);
    const double inv = 1.0 / (2 * r + 1);
    for (int y = 0; y < src.h; ++y) {
        const float *s = &src.d[size_t(y) * src.w];
        float *d = &tmp.d[size_t(y) * src.w];
        double sum = 0;
        for (int k = -r; k <= r; ++k)
            sum += s[clampi(k, 0, src.w - 1)];
        for (int x = 0; x < src.w; ++x) {
            d[x] = float(sum * inv);
            sum += s[std::min(x + r + 1, src.w - 1)] - s[std::max(x - r, 0)];
        }
    }
    for (int x = 0; x < src.w; ++x) {
        double sum = 0;
        for (int k = -r; k <= r; ++k)
            sum += tmp.d[size_t(clampi(k, 0, src.h - 1)) * src.w + x];
        for (int y = 0; y < src.h; ++y) {
            out.d[size_t(y) * src.w + x] = float(sum * inv);
            sum += tmp.d[size_t(std::min(y + r + 1, src.h - 1)) * src.w + x] - tmp.d[size_t(std::max(y - r, 0)) * src.w + x];
        }
    }
    return out;
}

// Minimum / maximum over a (2r+1) window along one line, in O(n) (monotonic queue).
void slidingExtreme(const float *in, float *out, int n, int r, int stride, bool wantMax)
{
    std::deque<int> q;
    auto worse = [&](float a, float b) { return wantMax ? a <= b : a >= b; }; // a can be dropped in favour of b
    int next = 0;
    for (int i = 0; i < n; ++i) {
        const int hi = std::min(n - 1, i + r);
        for (; next <= hi; ++next) {
            while (!q.empty() && worse(in[size_t(q.back()) * stride], in[size_t(next) * stride]))
                q.pop_back();
            q.push_back(next);
        }
        while (q.front() < i - r)
            q.pop_front();
        out[size_t(i) * stride] = in[size_t(q.front()) * stride];
    }
}

FloatPlane extremeFilter(const FloatPlane &src, int r, bool wantMax)
{
    if (r <= 0)
        return src;
    FloatPlane tmp = makeFloat(src.w, src.h), out = makeFloat(src.w, src.h);
    for (int y = 0; y < src.h; ++y)
        slidingExtreme(&src.d[size_t(y) * src.w], &tmp.d[size_t(y) * src.w], src.w, r, 1, wantMax);
    for (int x = 0; x < src.w; ++x)
        slidingExtreme(&tmp.d[x], &out.d[x], src.h, r, src.w, wantMax);
    return out;
}

// The picture shrunk so its long side is about `target` pixels (never enlarged); also returns the factor.
QImage shrunk(const QImage &src, int target, double &factor)
{
    const int longSide = std::max(src.width(), src.height());
    if (longSide <= target) {
        factor = 1.0;
        return src;
    }
    factor = double(longSide) / target;
    return src.scaled(std::max(1, int(std::lround(src.width() / factor))), std::max(1, int(std::lround(src.height() / factor))),
                      Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

// The three colour channels of an RGBA8888 image as 0..1 float planes.
void splitChannels(const QImage &img, FloatPlane ch[3])
{
    for (int c = 0; c < 3; ++c)
        ch[c] = makeFloat(img.width(), img.height());
    for (int y = 0; y < img.height(); ++y) {
        const uint8_t *s = img.constScanLine(y);
        for (int x = 0; x < img.width(); ++x)
            for (int c = 0; c < 3; ++c)
                ch[c].d[size_t(y) * img.width() + x] = s[x * 4 + c] * (1.f / 255.f);
    }
}

// Bilinear read of a small plane at a position given in FULL-size pixel coordinates.
inline float sampleSmall(const FloatPlane &p, double fx, double fy, double factor)
{
    return p.bilinear((fx + 0.5) / factor - 0.5, (fy + 0.5) / factor - 0.5);
}

// ---------------------------------------------------------------------------------------------
// Celofán: every colour channel printed a little off register, like a 3D film.
// ---------------------------------------------------------------------------------------------

// Premultiplied bilinear sample (as 0..255 floats + alpha) of channel `c` at (px, py).
inline float chanSample(const Plane &p, double px, double py, int c, float &alpha)
{
    float v[4];
    bilinear(p, px, py, v);
    alpha = v[3];
    return v[c];
}

QImage fxCellophane(const Job &job, const QImage &src, const EffectValues &v)
{
    const double amount = v[0] / 100.0;
    const double angle = v[1] * kPi / 180.0;
    const double m = std::min(src.width(), src.height());
    // At 100: green moves up-left by m/30, blue by (m/46, m/20.5); red stays - PhotoScape's registration
    // (it drops the fraction of a pixel; here the shifts are exact).
    const double gx0 = -m / 30.0, gy0 = -m / 30.0, bx0 = -m / 46.0, by0 = -m / 20.5;
    const double ca = std::cos(angle), sa = std::sin(angle);
    const double gx = (gx0 * ca - gy0 * sa) * amount, gy = (gx0 * sa + gy0 * ca) * amount;
    const double bx = (bx0 * ca - by0 * sa) * amount, by = (bx0 * sa + by0 * ca) * amount;

    const Plane p = planeFrom(src.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            float ar, ag, ab;
            const float r = chanSample(p, x, y, 0, ar);
            const float g = chanSample(p, x - gx, y - gy, 1, ag);
            const float b = chanSample(p, x - bx, y - by, 2, ab);
            const float a = std::max({ar, ag, ab});
            if (a <= 0.f) {
                d[x * 4] = d[x * 4 + 1] = d[x * 4 + 2] = d[x * 4 + 3] = 0;
                continue;
            }
            const float k = 255.f / a;
            d[x * 4] = toByte(r * k);
            d[x * 4 + 1] = toByte(g * k);
            d[x * 4 + 2] = toByte(b * k);
            d[x * 4 + 3] = toByte(a);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Aberración cromática: the colour channels magnified a little differently, more toward the
// corners - how a lens' lateral colour error looks (and what a correction slider removes).
// ---------------------------------------------------------------------------------------------

QImage fxChromatic(const Job &job, const QImage &src, const EffectValues &v)
{
    const double rc = v[0] / 100.0, by = v[1] / 100.0;
    const int w = src.width(), h = src.height();
    const double cx = (w - 1) * 0.5 * (1.0 + v[2] / 100.0);
    const double cy = (h - 1) * 0.5 * (1.0 + v[3] / 100.0);
    const double reach = std::max({std::hypot(cx, cy), std::hypot(w - 1 - cx, cy), std::hypot(cx, h - 1 - cy),
                                   std::hypot(w - 1 - cx, h - 1 - cy), 1.0});
    const double amp = 0.0114 * reach; // PhotoScape: 7.3 px at the corners' half-distance of 640

    const Plane p = planeFrom(src.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, h, [&](int y) {
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            const double dx = x - cx, dy = y - cy;
            const double r = std::sqrt(dx * dx + dy * dy);
            const double u = r / reach;
            const double bump = 4.0 * u * (1.0 - u) * amp; // 0 in the middle and at the far corner
            // Cyan (green + blue) moves outward by rc*bump, blue moves back inward by by*bump.
            const double kg = r > 1e-6 ? 1.0 - rc * bump / r : 1.0;
            const double kb = r > 1e-6 ? 1.0 - (rc - by) * bump / r : 1.0;
            float ar, ag, ab;
            const float cr = chanSample(p, x, y, 0, ar);
            const float cg = chanSample(p, cx + dx * kg, cy + dy * kg, 1, ag);
            const float cb = chanSample(p, cx + dx * kb, cy + dy * kb, 2, ab);
            const float a = std::max({ar, ag, ab});
            if (a <= 0.f) {
                d[x * 4] = d[x * 4 + 1] = d[x * 4 + 2] = d[x * 4 + 3] = 0;
                continue;
            }
            const float k = 255.f / a;
            d[x * 4] = toByte(cr * k);
            d[x * 4 + 1] = toByte(cg * k);
            d[x * 4 + 2] = toByte(cb * k);
            d[x * 4 + 3] = toByte(a);
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Blanco y negro: a six-colour channel mixer, with an optional tint.
// ---------------------------------------------------------------------------------------------

QImage fxBlackWhite(const Job &job, const QImage &src, const EffectValues &v)
{
    // weights for red, yellow, green, cyan, blue, magenta - the hue wheel in order
    const double wr = v[0] / 100.0, wy = v[1] / 100.0, wg = v[2] / 100.0, wc = v[3] / 100.0, wb = v[4] / 100.0,
                 wm = v[5] / 100.0;
    const bool tint = v[6] >= 0.5;
    const double tintAmount = v[8] / 100.0;
    const Rgb tc = unpackColor(v[7]);
    // The tint adds a chroma offset that does not change the grey's luma (T minus its own luma).
    const double ty = lumaOf(tc.r, tc.g, tc.b);
    const double off[3] = {(tc.r - ty) * tintAmount, (tc.g - ty) * tintAmount, (tc.b - ty) * tintAmount};

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double r = s[x * 4], g = s[x * 4 + 1], b = s[x * 4 + 2];
            const double mx = std::max({r, g, b}), mn = std::min({r, g, b});
            const double mid = r + g + b - mx - mn;
            // Which primary leads, and which secondary lies between it and the middle value.
            double lead, between;
            if (mx == mn) {
                lead = between = 0.0;
            } else if (r >= g && r >= b) { // red leads; the neighbour is yellow (g > b) or magenta
                lead = wr;
                between = g >= b ? wy : wm;
            } else if (g >= r && g >= b) { // green leads: yellow (r > b) or cyan
                lead = wg;
                between = r >= b ? wy : wc;
            } else { // blue leads: cyan (g > r) or magenta
                lead = wb;
                between = g >= r ? wc : wm;
            }
            double gray = mn + (mx - mid) * lead + (mid - mn) * between;
            gray = clampd(gray, 0.0, 255.0);
            double o[3] = {gray, gray, gray};
            if (tint) {
                // As much of the offset as keeps every channel inside 0..255.
                double scale = 1.0;
                for (int c = 0; c < 3; ++c) {
                    if (off[c] > 1e-9)
                        scale = std::min(scale, (255.0 - gray) / off[c]);
                    else if (off[c] < -1e-9)
                        scale = std::min(scale, gray / -off[c]);
                }
                scale = clampd(scale, 0.0, 1.0);
                for (int c = 0; c < 3; ++c)
                    o[c] = gray + off[c] * scale;
            }
            d[x * 4] = toByte(o[0]);
            d[x * 4 + 1] = toByte(o[1]);
            d[x * 4 + 2] = toByte(o[2]);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Borrar niebla: dark-channel prior dehazing (He, Sun and Tang), refined with a fast guided filter.
// ---------------------------------------------------------------------------------------------

// Fast guided filter: smooths `p` following the edges of the guide `I` (all at the small size), and
// returns the coefficients of q = a*I + b so that it can be applied to the full-size guide.
void guidedCoefficients(const FloatPlane &I, const FloatPlane &p, int r, double eps, FloatPlane &a, FloatPlane &b)
{
    const FloatPlane meanI = boxMean(I, r), meanP = boxMean(p, r);
    FloatPlane II = I, IP = I;
    for (size_t i = 0; i < I.d.size(); ++i) {
        II.d[i] = I.d[i] * I.d[i];
        IP.d[i] = I.d[i] * p.d[i];
    }
    const FloatPlane corrI = boxMean(II, r), corrIP = boxMean(IP, r);
    a = makeFloat(I.w, I.h);
    b = makeFloat(I.w, I.h);
    for (size_t i = 0; i < I.d.size(); ++i) {
        const double varI = corrI.d[i] - double(meanI.d[i]) * meanI.d[i];
        const double covIP = corrIP.d[i] - double(meanI.d[i]) * meanP.d[i];
        const double ai = covIP / (varI + eps);
        a.d[i] = float(ai);
        b.d[i] = float(meanP.d[i] - ai * meanI.d[i]);
    }
    a = boxMean(a, r);
    b = boxMean(b, r);
}

QImage fxDehaze(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const double amount = v[0];
    if (amount <= 0.0 && v[1] <= 0.0)
        return src;
    const double omega = amount <= 0.0 ? 0.0 : 0.65 * std::pow(amount / 100.0, 2.0 / 3.0); // PhotoScape's curve
    const double lift = v[1] / 100.0 * 0.6;
    const double zone = v[2] / 100.0;
    const bool whiteLight = v[3] >= 0.5;

    // Everything about the haze is smooth, so it is worked out on a shrunken copy.
    double factor = 1.0;
    const QImage small = shrunk(src, 720, factor);
    FloatPlane ch[3];
    splitChannels(small, ch);
    const int ww = small.width(), wh = small.height();
    const int radius = std::max(2, int(std::lround((longSide * (0.0015 + zone * 0.012)) / factor)));

    // The haze colour: the brightest of the haziest pixels (those whose darkest channel, over a window, is highest).
    double airlight[3] = {1.0, 1.0, 1.0};
    if (!whiteLight) {
        FloatPlane minC = makeFloat(ww, wh);
        for (size_t i = 0; i < minC.d.size(); ++i)
            minC.d[i] = std::min({ch[0].d[i], ch[1].d[i], ch[2].d[i]});
        const FloatPlane dark = extremeFilter(minC, radius, false);
        std::vector<size_t> order(dark.d.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        const size_t top = std::max<size_t>(1, order.size() / 1000);
        std::partial_sort(order.begin(), order.begin() + long(top), order.end(),
                          [&](size_t a, size_t b) { return dark.d[a] > dark.d[b]; });
        double best = -1;
        for (size_t k = 0; k < top; ++k) {
            const size_t i = order[k];
            const double sum = double(ch[0].d[i]) + ch[1].d[i] + ch[2].d[i];
            if (sum > best) {
                best = sum;
                for (int c = 0; c < 3; ++c)
                    airlight[c] = std::max(0.35, double(ch[c].d[i]));
            }
        }
    }

    // t(x) = 1 - omega * (dark channel of I/A)
    FloatPlane norm = makeFloat(ww, wh);
    for (size_t i = 0; i < norm.d.size(); ++i)
        norm.d[i] = std::min({ch[0].d[i] / float(airlight[0]), ch[1].d[i] / float(airlight[1]), ch[2].d[i] / float(airlight[2])});
    const FloatPlane darkNorm = extremeFilter(norm, radius, false);
    FloatPlane t = makeFloat(ww, wh), guide = makeFloat(ww, wh);
    for (size_t i = 0; i < t.d.size(); ++i) {
        t.d[i] = float(1.0 - omega * darkNorm.d[i]);
        guide.d[i] = float(lumaOf(ch[0].d[i], ch[1].d[i], ch[2].d[i]));
    }
    FloatPlane ca, cb;
    guidedCoefficients(guide, t, std::max(2, radius * 3), 1e-3, ca, cb);

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double r = s[x * 4] / 255.0, g = s[x * 4 + 1] / 255.0, b = s[x * 4 + 2] / 255.0;
            const double gl = lumaOf(r, g, b);
            const double tx = clampd(sampleSmall(ca, x, y, factor) * gl + sampleSmall(cb, x, y, factor), 0.1, 1.0);
            double o[3] = {r, g, b};
            for (int c = 0; c < 3; ++c) {
                double j = (o[c] - airlight[c]) / tx + airlight[c];
                j = clampd(j, 0.0, 1.0);
                if (lift > 0.0)
                    j = 1.0 - std::pow(1.0 - j, 1.0 + lift); // brighten the shadows the dehaze darkened
                o[c] = j;
            }
            d[x * 4] = toByte(o[0] * 255.0);
            d[x * 4 + 1] = toByte(o[1] * 255.0);
            d[x * 4 + 2] = toByte(o[2] * 255.0);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

// ---------------------------------------------------------------------------------------------
// Mejorar documento: even out the lighting of a photographed page, then whiten the paper.
// ---------------------------------------------------------------------------------------------

QImage fxDocument(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const int type = clampi(int(std::lround(v[0])), 0, 3);
    const double radiusPct = v[1] / 100.0;
    const double strength = v[2] / 100.0;
    const double shadow = v[3] / 100.0;
    const bool keepColor = v[4] >= 0.5;

    double factor = 1.0;
    const QImage small = shrunk(src, 640, factor);
    FloatPlane ch[3];
    splitChannels(small, ch);
    const int ww = small.width(), wh = small.height();
    // The paper's brightness: the biggest value in a window wider than any stroke of ink, softened.
    const int reach = std::max(3, int(std::lround((longSide * (0.008 + radiusPct * 0.05)) / factor)));
    FloatPlane bg[3];
    for (int c = 0; c < 3; ++c)
        bg[c] = boxMean(boxMean(extremeFilter(ch[c], reach, true), reach / 2 + 1), reach / 2 + 1);
    FloatPlane bgLuma = makeFloat(ww, wh);
    double bgMean = 0.0;
    for (size_t i = 0; i < bgLuma.d.size(); ++i) {
        bgLuma.d[i] = float(lumaOf(bg[0].d[i], bg[1].d[i], bg[2].d[i]));
        bgMean += bgLuma.d[i];
    }
    bgMean = std::max(0.05, bgMean / double(bgLuma.d.size()));

    const double lo = type == 2 ? strength * 0.3 : strength * 0.5;
    const double hi = type == 2 ? 1.0 - strength * 0.2 : 1.0 - strength * 0.25;
    const double thresholdK = 0.5 - 0.45 * strength; // text: how far below the paper a pixel must be to count as ink

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double in[3] = {s[x * 4] / 255.0, s[x * 4 + 1] / 255.0, s[x * 4 + 2] / 255.0};
            const double bl = std::max(0.04, double(sampleSmall(bgLuma, x, y, factor)));
            const double lightLuma = lerp(bgMean, bl, shadow); // how much of the uneven lighting is removed
            double o[3];
            if (type == 0) { // only the shadows
                for (int c = 0; c < 3; ++c) {
                    const double b = keepColor ? bl : std::max(0.04, double(sampleSmall(bg[c], x, y, factor)));
                    o[c] = clampd(in[c] / b, 0.0, 1.0);
                }
            } else if (type == 1) { // black and white page
                const double g = clampd(lumaOf(in[0], in[1], in[2]) / std::max(0.04, lightLuma), 0.0, 1.0);
                const double e = clampd((g - lo) / (hi - lo), 0.0, 1.0);
                o[0] = o[1] = o[2] = e;
            } else if (type == 2) { // colour page
                for (int c = 0; c < 3; ++c) {
                    const double b = std::max(0.04, lerp(bgMean, double(sampleSmall(bg[c], x, y, factor)), shadow));
                    o[c] = clampd((clampd(in[c] / b, 0.0, 1.0) - lo) / (hi - lo), 0.0, 1.0);
                }
            } else { // text: ink or paper
                const double l = lumaOf(in[0], in[1], in[2]);
                const double cut = bl * (1.0 - thresholdK);
                const double e = smoothStep01((l - cut * 0.92) / std::max(1e-3, cut * 0.16));
                o[0] = o[1] = o[2] = e;
            }
            d[x * 4] = toByte(o[0] * 255.0);
            d[x * 4 + 1] = toByte(o[1] * 255.0);
            d[x * 4 + 2] = toByte(o[2] * 255.0);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

} // namespace

void addColorSpecs(std::vector<EffectSpec> &out)
{
    {
        EffectSpec s = makeSpec("bw", "Blanco y negro", "color",
                                {slider("Rojos", -200, 300, 40), slider("Amarillos", -200, 300, 60),
                                 slider("Verdes", -200, 300, 40), slider("Cianes", -200, 300, 60),
                                 slider("Azules", -200, 300, 20), slider("Magentas", -200, 300, 80),
                                 toggle("Tinte"), shownWhen(colorParam("Color del tinte", 150, 115, 75), 6, {1}),
                                 shownWhen(slider("Fuerza del tinte", 0, 100, 100), 6, {1})});
        s.presets = {
            {"Predeterminada", {{0, 40}, {1, 60}, {2, 40}, {3, 60}, {4, 20}, {5, 80}}},
            {"Neutro", {{0, 30}, {1, 89}, {2, 59}, {3, 70}, {4, 11}, {5, 41}}},
            {"Filtro rojo", {{0, 120}, {1, 110}, {2, -10}, {3, -50}, {4, 0}, {5, 120}}},
            {"Filtro amarillo", {{0, 120}, {1, 110}, {2, 40}, {3, -30}, {4, 0}, {5, 70}}},
            {"Filtro verde", {{0, 50}, {1, 120}, {2, 90}, {3, 50}, {4, 0}, {5, 0}}},
            {"Filtro azul", {{0, 0}, {1, 0}, {2, 0}, {3, 110}, {4, 110}, {5, 110}}},
            {"Infrarrojo", {{0, 60}, {1, 190}, {2, 240}, {3, 100}, {4, -60}, {5, 20}}},
            {"Contraste alto", {{0, 90}, {1, 120}, {2, 10}, {3, 40}, {4, -30}, {5, 100}}},
        };
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("dehaze", "Borrar niebla", "color",
                                {slider("Cantidad", 0, 150, 50), slider("Aclarar sombras", 0, 100, 0),
                                 slider("Zona", 0, 100, 40),
                                 choice("Luz ambiente", {"Automática", "Blanca"}, 0)});
        s.sample = {{0, 90}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec(
            "document", "Mejorar documento", "color",
            {choice("Tipo", {"Quitar sombras", "Blanco y negro", "Color", "Texto"}, 0), slider("Radio", 0, 100, 50),
             shownWhen(slider("Intensidad", 0, 100, 50), 0, {1, 2, 3}), shownWhen(slider("Sombra", 0, 100, 100), 0, {1, 2}),
             shownWhen(toggle("Mantener color"), 0, {0})});
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("chromatic", "Aberración cromática", "color",
                                {slider("Rojo / Cian", -100, 100, 60), slider("Azul / Amarillo", -100, 100, 0),
                                 slider("Centro X", -100, 100, 0), slider("Centro Y", -100, 100, 0)});
        s.overlays = {{EffectOverlay::Point, 2, 3, "Centro"}};
        out.push_back(std::move(s));
    }
    {
        EffectSpec s = makeSpec("cellophane", "Celofán", "color", {slider("Cantidad", 0, 100, 50), slider("Ángulo", -180, 180, 0, "°", true)});
        out.push_back(std::move(s));
    }
}

bool renderColor(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                 QImage &result)
{
    if (id == QLatin1String("cellophane"))
        result = fxCellophane(job, src, v);
    else if (id == QLatin1String("chromatic"))
        result = fxChromatic(job, src, v);
    else if (id == QLatin1String("bw"))
        result = fxBlackWhite(job, src, v);
    else if (id == QLatin1String("dehaze"))
        result = fxDehaze(job, src, v, longSide);
    else if (id == QLatin1String("document"))
        result = fxDocument(job, src, v, longSide);
    else
        return false;
    return true;
}

} // namespace core::edit::fxk
