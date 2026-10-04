#pragma once

#include <QImage>
#include <atomic>

namespace core::edit {

// Colour noise reduction ("Quitar ruido"): non-local means on luma + chroma.
//
// The picture is split into luma (Y) and two colour differences (Co, Cg). The amount of noise in
// each is measured from the picture itself (the larger of a Laplacian estimate and the variance of
// the flattest 8x8 blocks), so the sliders mean the same on a clean low-ISO shot and on a grainy
// night photo. Every pixel is then replaced by the average of the pixels around it whose 5x5
// neighbourhood - compared in all three planes at once - looks like its own (7x7 search window).
// That removes noise from flat areas without softening edges and texture the way a blur or a
// median does, and because the comparison includes colour it does not smear colours across edges.
// Colour noise is blotchier and uglier than luma noise, so the colour planes are averaged more
// strongly (the "chroma" slider) than luma (the "luma" slider).
//
//   luma, chroma: 0..100 (0 leaves that part untouched; both 0 returns the source).
//
// Works on the real pixels: the amount of noise depends on the picture's own resolution, so
// unlike the other effects it does not make sense on a shrunken copy (see EffectSpec::fullSize).
// `source` must be Format_RGBA8888 (straight alpha, kept as it is); the result has the same format
// and size. `cancel` is polled like in applyEffect: a cancelled result must be discarded.
QImage denoiseColor(const QImage &source, double luma, double chroma, const std::atomic<bool> *cancel = nullptr);

} // namespace core::edit
