#pragma once

#include <algorithm>
#include <cmath>

namespace core::edit {

// Contrast Adaptive Sharpening, from AMD FidelityFX (FSR 1's sharpening pass; MIT licence, copyright AMD).
//
// Each pixel is sharpened with the four neighbours above, below, left and right (a "plus" filter), by an amount that
// depends on how much room the local contrast leaves: where the 3x3 neighbourhood is flat or already contrasty the
// sharpening is small, where there is room to add contrast it is largest. That is what keeps it from drawing the halos
// and from amplifying noise in clean areas the way an unsharp mask does.
//
//   a b c
//   d e f      `n` = { a, b, c, d, e, f, g, h, i } of ONE colour channel, in 0..1
//   g h i
//
// `strength` 0..1 is the slider: 0 returns `e` untouched, and the filter's own "sharpness" (the shape of the plus)
// and the share of the result that is used both grow with it, so the effect starts gently and reaches the full FSR
// setting at 1.
//
// qml/shaders/Detail.frag and qml/shaders/Cas.frag compute exactly this, in the same order, so the preview and a saved
// file agree.
inline float casChannel(const float n[9], float strength)
{
    const float a = n[0], b = n[1], c = n[2], d = n[3], e = n[4], f = n[5], g = n[6], h = n[7], i = n[8];
    // Soft min and max: the plus-shaped neighbourhood plus the whole 3x3 (each counted, so the range is 0..2).
    float mn = std::min(std::min(std::min(d, e), f), std::min(b, h));
    mn += std::min(std::min(std::min(mn, a), c), std::min(g, i));
    float mx = std::max(std::max(std::max(d, e), f), std::max(b, h));
    mx += std::max(std::max(std::max(mx, a), c), std::max(g, i));
    // Smooth distance to the signal limit divided by the smooth max (0 for a black neighbourhood: nothing to scale).
    float amp = mx > 0.0f ? std::clamp(std::min(mn, 2.0f - mx) / mx, 0.0f, 1.0f) : 0.0f;
    amp = std::sqrt(amp);
    // Filter shape: the centre weight 1 and the four neighbours w (negative).
    const float w = -amp / (8.0f + (5.0f - 8.0f) * strength);
    const float sharpened = std::clamp(((b + d + f + h) * w + e) / (1.0f + 4.0f * w), 0.0f, 1.0f);
    return e + strength * (sharpened - e);
}

} // namespace core::edit
