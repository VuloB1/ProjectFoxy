#pragma once

#include "Operations.h"
#include <QImage>
#include <array>

namespace core::edit {

// The whole "Ajustes" pipeline, split into the same three stages the GPU
// preview runs (qml/shaders/Grade.frag, Detail.frag), so what's on screen
// and what gets saved agree:
//
//   A. Per-channel tone LUT - everything that only looks at ONE channel's
//      value: white balance gains, exposure, levels, gamma, blacks/whites,
//      curves, brightness, contrast. Precomputed once into three 256-entry
//      tables; the GPU samples the very same bytes (as a 256x1 texture), so
//      this stage is bit-exact on both sides.
//   B. Per-pixel color mixing - shadows/highlights, saturation, vibrance, hue
//      rotation, negative. Needs the three channels together, so it's plain
//      float math, transcribed into the shader.
//   C. Spatial detail - clarity (big-radius local contrast) and the 5-tap
//      sharpen.

struct AdjustLut {
    std::array<quint8, 256> r{};
    std::array<quint8, 256> g{};
    std::array<quint8, 256> b{};
};

// Stage A tables for `op` (identity tables when op has no tone edits).
AdjustLut buildAdjustLut(const AdjustOp &op);

// The tables as a 256x1 RGBA8888 image (R,G,B = the three tables, A = 255),
// ready to be handed to the GPU as a lookup texture.
QImage lutToImage(const AdjustLut &lut);

// The curve through `pts` (monotone cubic - never overshoots between points)
// sampled at 256 evenly spaced inputs. Used for both drawing the curve editor
// and building stage A, so they can't disagree.
std::array<double, 256> sampleCurve(const CurvePoints &pts);

// Extra color stages that only the Filtros looks use, run after stages A and B
// (see Looks.h). Same math as Grade.frag.
struct GradeExtras {
    // Gradient map: the pixel's brightness picks a color along three stops
    // (dark, middle, light) - duotone, sepia, cyanotype.
    bool gradient = false;
    std::array<std::array<double, 3>, 3> stops{};
    // Split toning: colors pushed into the shadows / the highlights, already
    // scaled by their strength and stripped of brightness so they tint
    // without making anything lighter or darker.
    std::array<double, 3> shadowOffset{};
    std::array<double, 3> highlightOffset{};
    double vignette = 0.0; // - lightens the corners / + darkens them
    double grain = 0.0;    // 0..1
    int grainType = static_cast<int>(NoiseType::Gaussian);
    bool grainMono = true; // film grain is brightness noise

    bool isActive() const
    {
        if (gradient || vignette != 0.0 || grain != 0.0)
            return true;
        for (int i = 0; i < 3; ++i) {
            if (shadowOffset[i] != 0.0 || highlightOffset[i] != 0.0)
                return true;
        }
        return false;
    }
};

// Stages A and B of `tone` (its clarity/sharpness/vignette/grain are ignored)
// followed by `extras`, then blended `amount` (0..1) of the way from the
// original toward that result. This is what a look is; the Ajustes tab is the
// same call with no extras and amount 1.
QImage applyGrade(const QImage &source, const AdjustOp &tone, const GradeExtras &extras, double amount);

// One pixel of the Grano y ruido effect: the same random distributions the Ajustes grain
// slider used (see applyNoise in AdjustMath.cpp). `c` is straight RGB in 0..1 and is
// changed in place (the caller clamps); `amount` is 0..1 and `type` a NoiseType.
void applyUserNoise(int x, int y, double amount, int type, bool mono, double c[3]);

// Runs all three stages. Returns `source` (as RGBA8888) untouched when `op`
// is the identity.
QImage applyAdjustOp(const QImage &source, const AdjustOp &op);

enum class AutoAdjustKind {
    Contrast, // stretch overall brightness to the full range, colors untouched
    Levels,   // stretch each channel on its own (also evens out color casts)
    Color,    // neutralize an overall color cast (gray-world white balance)
};

// `base` with its levels replaced by the ones that `kind` computes from
// `image`'s histogram (all other sliders are left as they were).
AdjustOp autoAdjusted(AdjustOp base, const QImage &image, AutoAdjustKind kind);

} // namespace core::edit
