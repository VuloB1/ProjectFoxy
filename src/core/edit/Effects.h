#pragma once

#include "Operations.h"

#include <QImage>
#include <QString>
#include <QStringList>
#include <array>
#include <atomic>
#include <vector>

namespace core::edit {

// The "Efectos" catalogue: one-shot image effects (blurs, stylizing, distortions)
// that are applied to the picture for good - unlike the Ajustes sliders and the
// Filtros looks, which stay live - and end up in the undo stack as an EffectOp.
//
// Every distance a slider controls is stored as a percentage of the image's
// LONG side (never as pixels), so an effect looks the same on the 128px preview
// and on the full-size photo. That is what lets the panel show a fast preview
// computed on a shrunken copy and then the exact result at full size.

struct EffectParam {
    QString label;
    double min = 0.0;
    double max = 100.0;
    double def = 50.0;
    QString suffix;        // shown after the number: "%", "°"
    bool integer = false;  // snaps to whole numbers
    bool toggle = false;   // an on/off switch instead of a slider (value 0 or 1)
    QStringList options;   // when set: a choice among these names (value = the index picked)
};

struct EffectSpec {
    QString id;
    QString name;
    QString group;
    std::vector<EffectParam> params; // 0..kMaxEffectParams entries
};

struct EffectGroup {
    QString id;
    QString name;
};

const std::vector<EffectGroup> &effectGroups();
const std::vector<EffectSpec> &allEffects();
const EffectSpec *findEffect(const QString &id);

// The catalogue's default value for every parameter (unused slots are 0).
EffectValues defaultEffectValues(const EffectSpec &spec);

// Settings that show an effect clearly on a tiny (128 px) preview: the defaults,
// pushed harder for the effects whose strength is a distance (on a thumbnail a
// "normal" blur is a fraction of a pixel and would look like no effect at all).
EffectValues sampleEffectValues(const EffectSpec &spec);

// `source` with the effect `id` applied using `values` (see the parameter list
// of its EffectSpec for what each slot means), then blended `mix` (0..1) of the
// way from the original toward the result. An unknown id, or mix <= 0, returns
// the source unchanged. The result is Format_RGBA8888 and the same size.
//
// `cancel` (optional) is polled while working: once another thread sets it the
// call returns quickly with an UNFINISHED image that the caller must discard.
QImage applyEffect(const QImage &source, const QString &id, const EffectValues &values, double mix = 1.0,
                   const std::atomic<bool> *cancel = nullptr);

} // namespace core::edit
