#include "EffectsCommon.h"
#include "Blend.h"

namespace core::edit::fxk {

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

namespace {

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

} // namespace

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

// --- Catalogue helpers -------------------------------------------------------------

EffectParam slider(const char *label, double lo, double hi, double def, const char *suffix, bool integer)
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

EffectParam toggle(const char *label, bool def)
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

EffectParam colorParam(const char *label, int r, int g, int b)
{
    EffectParam p;
    p.label = QString::fromUtf8(label);
    p.min = 0.0;
    p.max = 16777215.0;
    p.def = packColor(r, g, b);
    p.integer = true;
    p.color = true;
    return p;
}

EffectParam seedParam(const char *label, int def)
{
    EffectParam p = slider(label, 0, 999, def, "", true);
    p.seed = true;
    return p;
}

EffectParam shownWhen(EffectParam p, int on, std::initializer_list<int> values)
{
    p.dependsOn = on;
    p.dependsMask = 0;
    for (int v : values)
        p.dependsMask |= 1u << v;
    return p;
}

EffectParam withHint(EffectParam p, const char *hint)
{
    p.hint = QString::fromUtf8(hint);
    return p;
}

EffectSpec makeSpec(const char *id, const char *name, const char *group, std::vector<EffectParam> params)
{
    EffectSpec s;
    s.id = QString::fromUtf8(id);
    s.name = QString::fromUtf8(name);
    s.group = QString::fromUtf8(group);
    s.params = std::move(params);
    return s;
}

EffectParam blendParam(int def)
{
    return choice("Fusión", blendModeNames(), def);
}

} // namespace core::edit::fxk
