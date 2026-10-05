#include "Blend.h"
#include "EffectsCommon.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// The "Dibujo" family: effects that draw a regular structure over or instead of the picture - the
// print halftone, lines, rings, speed lines, gradient and pattern fills, the border line.

namespace core::edit::fxk {

namespace {

// ---------------------------------------------------------------------------------------------
// Semitono: the picture as it looks printed in a newspaper - ink dots on paper, one screen per ink.
// ---------------------------------------------------------------------------------------------

enum Shape { kRound = 0, kSquare, kDiamond, kLineShape };

// How a dot grows. The "spot function" f(u, v) of a cell (u, v in -0.5..0.5, the cell's centre at
// 0) is small at the middle and large at the edge; a pixel is inked when its rank among all the
// pixels of a cell (0 = first to be inked) is below the ink amount. Ranking, instead of using f
// itself, is what makes the inked AREA equal the ink amount for every shape.
struct SpotTable {
    static constexpr int kBins = 1024;
    float rank[kBins + 1];
    double fmax = 1.0;

    explicit SpotTable(int shape)
    {
        constexpr int kN = 128;
        std::vector<float> values;
        values.reserve(size_t(kN) * kN);
        for (int j = 0; j < kN; ++j)
            for (int i = 0; i < kN; ++i)
                values.push_back(float(spot(shape, (i + 0.5) / kN - 0.5, (j + 0.5) / kN - 0.5)));
        std::sort(values.begin(), values.end());
        fmax = double(values.back());
        for (int b = 0; b <= kBins; ++b) {
            const float f = float(fmax * b / kBins);
            rank[b] = float(std::lower_bound(values.begin(), values.end(), f) - values.begin()) / float(values.size());
        }
    }
    static double spot(int shape, double u, double v)
    {
        switch (shape) {
        case kSquare: return std::max(std::abs(u), std::abs(v));
        case kDiamond: return std::abs(u) + std::abs(v);
        case kLineShape: return std::abs(v);
        default: return u * u + v * v;
        }
    }
    // rank of the pixel at cell position (u, v)
    float at(int shape, double u, double v) const
    {
        const double f = spot(shape, u, v) / fmax * kBins;
        const int i = clampi(int(f), 0, kBins - 1);
        const float t = float(clampd(f - i, 0.0, 1.0));
        return rank[i] + (rank[i + 1] - rank[i]) * t;
    }
};

const SpotTable &spotTable(int shape)
{
    static const SpotTable tables[4] = {SpotTable(kRound), SpotTable(kSquare), SpotTable(kDiamond), SpotTable(kLineShape)};
    return tables[clampi(shape, 0, 3)];
}

// One ink screen: a lattice of cells of `pitch` pixels turned by `angle`.
struct Screen {
    double ca = 1.0, sa = 0.0;
    double pitch = 8.0;
    int shape = kRound;
    const SpotTable *table = nullptr;

    Screen(double angleDeg, double pitchPx, int shapeId)
        : ca(std::cos(angleDeg * kPi / 180.0)), sa(std::sin(angleDeg * kPi / 180.0)), pitch(pitchPx), shape(shapeId),
          table(&spotTable(shapeId)) {}

    // rank at pixel position (x, y), in the cell of this screen
    float rankAt(double x, double y) const
    {
        const double u = (x * ca + y * sa) / pitch;
        const double v = (-x * sa + y * ca) / pitch;
        return table->at(shape, u - std::floor(u) - 0.5, v - std::floor(v) - 0.5);
    }
    // 0..1: how much of the pixel is covered by ink when the ink amount is `amount`
    double coverage(double x, double y, double amount) const
    {
        if (amount <= 0.004)
            return 0.0;
        if (amount >= 0.996)
            return 1.0;
        const float r = rankAt(x, y);
        const float rx = rankAt(x + 1.0, y), ry = rankAt(x, y + 1.0);
        const double width = std::max(1e-3, double(std::abs(rx - r) + std::abs(ry - r)));
        return clampd((amount - r) / width + 0.5, 0.0, 1.0);
    }
};

QImage fxHalftonePrint(const Job &job, const QImage &src, const EffectValues &v, double longSide)
{
    const double pitch = std::max(3.0, longSide * (0.002 + v[0] * 0.000113));
    const int mode = clampi(int(std::lround(v[1])), 0, 2);   // 0 CMY, 1 CMYK, 2 black only
    const int shape = clampi(int(std::lround(v[2])), 0, 3);
    const double turn = v[3];
    const double gain = v[4] / 100.0 * 0.5;                   // dot gain: ink spreads and prints darker
    const Rgb paper = unpackColor(v[5]);

    // The tone each dot has to reproduce: the picture softened over about one cell, so a dot is the
    // average of what is under it and not of a single pixel. Premultiplied, so that transparent
    // pixels' hidden colour does not leak into it.
    const Plane tone = gaussianBlur(job, planeFrom(src.convertToFormat(QImage::Format_RGBA8888_Premultiplied)), pitch * 0.35);

    // The screens (angles are the classic ones of each process; a lone black screen sits at 45 degrees).
    std::vector<Screen> screens;
    if (mode == 0) {
        screens = {Screen(45.0 + turn, pitch, shape), Screen(22.5 + turn, pitch, shape), Screen(0.0 + turn, pitch, shape)};
    } else if (mode == 1) {
        screens = {Screen(15.0 + turn, pitch, shape), Screen(75.0 + turn, pitch, shape), Screen(0.0 + turn, pitch, shape),
                   Screen(45.0 + turn, pitch, shape)};
    } else {
        screens = {Screen(45.0 + turn, pitch, shape)};
    }

    auto gained = [gain](double c) { return clampd(c + gain * c * (1.0 - c) * 2.0, 0.0, 1.0); };

    QImage out(src.size(), QImage::Format_RGBA8888);
    out.bits();
    rows(job, src.height(), [&](int y) {
        const uint8_t *s = src.constScanLine(y);
        const uint8_t *t = tone.row(y);
        uint8_t *d = out.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const double a = t[x * 4 + 3];
            // un-premultiply the softened colour
            const double inv = a > 0.0 ? 255.0 / a : 0.0;
            const double r = clampd(t[x * 4] * inv, 0.0, 255.0) / 255.0;
            const double g = clampd(t[x * 4 + 1] * inv, 0.0, 255.0) / 255.0;
            const double b = clampd(t[x * 4 + 2] * inv, 0.0, 255.0) / 255.0;
            double c = 1.0 - r, m = 1.0 - g, yy = 1.0 - b, k = 0.0;
            if (mode == 1) {
                k = std::min({c, m, yy});
                const double rest = 1.0 - k;
                if (rest > 1e-6) {
                    c = (c - k) / rest;
                    m = (m - k) / rest;
                    yy = (yy - k) / rest;
                } else {
                    c = m = yy = 0.0;
                }
            } else if (mode == 2) {
                k = 1.0 - (0.299 * r + 0.587 * g + 0.114 * b);
            }
            double outR = paper.r, outG = paper.g, outB = paper.b;
            if (mode == 2) {
                const double cov = screens[0].coverage(x, y, gained(k));
                outR *= 1.0 - cov;
                outG *= 1.0 - cov;
                outB *= 1.0 - cov;
            } else {
                outR *= 1.0 - screens[0].coverage(x, y, gained(c));
                outG *= 1.0 - screens[1].coverage(x, y, gained(m));
                outB *= 1.0 - screens[2].coverage(x, y, gained(yy));
                if (mode == 1) {
                    const double cov = screens[3].coverage(x, y, gained(k));
                    outR *= 1.0 - cov;
                    outG *= 1.0 - cov;
                    outB *= 1.0 - cov;
                }
            }
            d[x * 4] = toByte(outR);
            d[x * 4 + 1] = toByte(outG);
            d[x * 4 + 2] = toByte(outB);
            d[x * 4 + 3] = s[x * 4 + 3];
        }
    });
    return out;
}

} // namespace

void addPatternSpecs(std::vector<EffectSpec> &out)
{
    {
        EffectSpec s = makeSpec("halftoneprint", "Semitono", "style",
                                {slider("Tamaño", 0, 100, 80),
                                 choice("Tinta", {"Color (CMY)", "Color (CMYK)", "Negro"}, 0),
                                 choice("Forma del punto", {"Círculo", "Cuadrado", "Rombo", "Línea"}, 0),
                                 slider("Ángulo", -45, 45, 0, "°", true), slider("Ganancia de punto", 0, 100, 0),
                                 colorParam("Papel", 255, 255, 255)});
        s.presets = {{"Color", {{1, 0}, {5, double(packColor(255, 255, 255))}}},
                     {"Imprenta", {{1, 1}, {5, double(packColor(255, 255, 255))}}},
                     {"Periódico", {{1, 2}, {5, double(packColor(236, 230, 214))}, {4, 25}}}};
        s.sample = {{0, 55}};
        out.push_back(std::move(s));
    }
    addDecorSpecs(out);
}

bool renderPattern(const Job &job, const QString &id, const QImage &src, const EffectValues &v, double longSide,
                   QImage &result)
{
    if (id == QLatin1String("halftoneprint")) {
        result = fxHalftonePrint(job, src, v, longSide);
        return true;
    }
    return renderDecor(job, id, src, v, longSide, result);
}

} // namespace core::edit::fxk
