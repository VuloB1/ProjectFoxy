#pragma once

#include "Operations.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <array>
#include <atomic>
#include <utility>
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
    bool color = false;    // the value is a colour packed as 0xRRGGBB (shown as a swatch with a picker)
    bool seed = false;     // an integer slider with a dice button that rolls a new random value
    // Shown only while another parameter (`dependsOn`, a choice or an on/off switch) has one of the
    // values whose bit is set in `dependsMask` (bit k = value k). -1 = always shown.
    int dependsOn = -1;
    unsigned dependsMask = 0;
    QString hint;          // a sentence for the tooltip (optional)
};

// A named starting point inside an effect: sets some of its parameters (index, value) at once.
struct EffectPreset {
    QString name;
    std::vector<std::pair<int, double>> sets;
};

// A handle the canvas draws over the picture so a parameter can be dragged instead of typed: a
// point (two parameters = its X and Y) or a guide line. The parameter's range maps linearly to the
// width/height of the picture.
struct EffectOverlay {
    enum Kind { Point, VLine, HLine } kind = Point;
    int x = -1; // Point: X parameter; VLine: the parameter; HLine unused
    int y = -1; // Point: Y parameter; HLine: the parameter
    QString label;
    // How the parameters map to a place on the picture as it is shown: empty = linearly over the
    // parameter's range; "stretchX" / "stretchY" = the guides of the stretch effect, which sit on the
    // original picture while the stretched one is on show (see qml/EffectHandles.qml).
    QString mapping;
};

struct EffectSpec {
    QString id;
    QString name;
    QString group;
    std::vector<EffectParam> params; // 0..kMaxEffectParams entries
    std::vector<EffectPreset> presets;
    std::vector<EffectOverlay> overlays;
    // Overrides (index, value) of the defaults that make the 128 px thumbnail in the effects grid
    // show the effect clearly (see sampleEffectValues).
    std::vector<std::pair<int, double>> sample;
    // The result depends on the real pixels (noise reduction), so a quick preview computed on a
    // shrunken copy would not show what the full-size picture gets: it is always calculated at full size.
    bool fullSize = false;
    // The result is not the same size as the picture (stretch, perspective...): see effectOutputSize().
    // Such an effect has no "Mezcla" (there is nothing to mix a different-sized picture with).
    bool changesSize = false;
    // Left out of the effects grid: it exists to be driven by a dedicated tool (the lens correction).
    bool hidden = false;
    // The effect carries a text beside its sliders (the meme caption): see applyEffect's `text`.
    bool usesText = false;
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

// `values` with the preset's settings applied.
EffectValues applyEffectPreset(const EffectSpec &spec, const EffectPreset &preset, EffectValues values);

// Settings that show an effect clearly on a tiny (128 px) preview: the defaults,
// pushed harder for the effects whose strength is a distance (on a thumbnail a
// "normal" blur is a fraction of a pixel and would look like no effect at all).
EffectValues sampleEffectValues(const EffectSpec &spec);

// `source` with the effect `id` applied using `values` (see the parameter list
// of its EffectSpec for what each slot means), then blended `mix` (0..1) of the
// way from the original toward the result. An unknown id, or mix <= 0, returns
// the source unchanged. The result is Format_RGBA8888 and, unless the effect
// is flagged changesSize, the same size.
//
// `cancel` (optional) is polled while working: once another thread sets it the
// call returns quickly with an UNFINISHED image that the caller must discard.
QImage applyEffect(const QImage &source, const QString &id, const EffectValues &values, double mix = 1.0,
                   const std::atomic<bool> *cancel = nullptr, const QString &text = QString());

// The size applyEffect() will return for a picture of `input` size (always `input` unless the effect
// is flagged changesSize). It is what lets a preview computed on a shrunken copy be put at the
// right size.
QSize effectOutputSize(const QString &id, const EffectValues &values, QSize input, const QString &text = QString());

} // namespace core::edit
