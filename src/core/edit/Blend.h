#pragma once

#include <QStringList>
#include <cstdint>

namespace core::edit {

// The blend modes the decorative effects (lines, patterns, gradients, light...) offer in their
// "Fusión" choice. The order is the order of blendModeNames(), and the number is what is stored in
// an effect's value slot, so keep it stable.
enum class BlendMode : int {
    Normal = 0,
    Darken,
    Multiply,
    ColorBurn,
    LinearBurn,
    Lighten,
    Screen,
    ColorDodge,
    LinearDodge, // "Añadir"
    Overlay,
    SoftLight,
    HardLight,
    VividLight,
    PinLight,
    HardMix,
    Difference,
    Exclusion,
    Xor,
    Hue,
    Saturation,
    Color,
    Luminosity,
    Count
};

// Spanish names, one per BlendMode, in enum order.
const QStringList &blendModeNames();

// Blends one colour: `base` is what is already there and `top` what is being laid over it, both as
// 0..1 straight RGB; the result (before opacity / alpha) goes to `out`.
void blendRgb(BlendMode mode, const float base[3], const float top[3], float out[3]);

// Lays a layer colour (0..255 per channel) of coverage `layerAlpha` (0..1, opacity already in)
// over the straight-alpha RGBA pixel at `dst`, with the usual alpha compositing. A pixel that is
// transparent just takes the layer's colour; one that is opaque is mixed by the blend mode.
void compositeOver(uint8_t *dst, double r, double g, double b, double layerAlpha, BlendMode mode);

} // namespace core::edit
