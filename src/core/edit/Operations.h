#pragma once

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <array>
#include <variant>
#include <vector>

namespace core::edit {

// The slider values of an Efectos effect, in the order its catalogue entry lists them
// (unused slots are 0). The most any effect has is kMaxEffectParams.
inline constexpr size_t kMaxEffectParams = 16;
using EffectValues = std::array<double, kMaxEffectParams>;

// Each operation is a plain data record (no behavior) so the stack can be
// serialized (undo history, "edit sidecar" saved alongside the original) and
// so the GPU preview path and the CPU export path can share one definition.

struct CropOp {
    QRectF normalizedRect; // 0..1 relative to source image
    bool operator==(const CropOp &) const = default;
};

struct ResizeOp {
    QSize targetSize;
    bool keepAspectRatio = true;
    QString resampleFilter = "lanczos3"; // or "nearest" (modo pixel)
    // Sharpen edges after ENLARGING (see resizeHighQuality); no effect when
    // the picture is not getting bigger.
    bool enhanceDetail = true;
    bool operator==(const ResizeOp &) const = default;
};

struct RotateOp {
    double degrees = 0.0; // 90/180/270 from the Recortar tab's rotate button,
                           // or any fine-grained value from "Enderezar" (straighten)
    bool smooth = true;    // false: nearest neighbour (modo pixel), no new colours appear
    bool operator==(const RotateOp &) const = default;
};

struct FlipOp {
    bool horizontal = false;
    bool vertical = false;
    bool operator==(const FlipOp &) const = default;
};

// One channel's input-side levels: everything at or below inBlack becomes
// black, at or above inWhite becomes white, and `gamma` bends what's between
// (>1 brightens the midtones, <1 darkens them).
struct LevelsChannel {
    double inBlack = 0.0; // 0..1
    double gamma = 1.0;   // 0.1..9.99
    double inWhite = 1.0; // 0..1

    bool isIdentity() const { return inBlack == 0.0 && gamma == 1.0 && inWhite == 1.0; }
    bool operator==(const LevelsChannel &) const = default;
};

// A tone curve as user-placed control points (x, y in 0..1, sorted by x);
// empty, or just the two corner points, means "no curve".
using CurvePoints = std::vector<QPointF>;

inline bool isIdentityCurve(const CurvePoints &pts)
{
    if (pts.size() < 2)
        return true;
    return pts.size() == 2 && pts[0] == QPointF(0.0, 0.0) && pts[1] == QPointF(1.0, 1.0);
}

// The random distributions the noise/grain control offers (same set as the
// "Añadir ruido" dialog it was modeled on). Numbers are stored and sent to the
// shaders, so keep them stable.
enum class NoiseType : int {
    Uniform = 0,   // every deviation up to a limit is equally likely - flat, even speckle
    Gaussian = 1,  // bell curve - what sensor noise and film grain look like
    Impulse = 2,   // isolated pure white / black dots ("salt and pepper")
    Laplacian = 3, // sharper peak and heavier tails than Gaussian - mostly fine with a few strong specks
};

// Every slider is a signed -1..1 amount (0 = untouched) unless noted. The
// maths behind each one lives in AdjustMath.cpp, and is mirrored line-for-line
// by qml/shaders/Grade.frag + Detail.frag - the GPU shaders draw the live
// preview, this is what actually gets written to disk.
struct AdjustOp {
    // Tone
    double exposure = 0.0;
    double brightness = 0.0;
    double contrast = 0.0;
    double highlights = 0.0;
    double shadows = 0.0;
    double whites = 0.0;
    double blacks = 0.0;
    double gamma = 0.0;
    // Color
    double temperature = 0.0; // - cooler / + warmer
    double tint = 0.0;        // - magenta / + green
    double saturation = 0.0;
    double vibrance = 0.0;
    double hue = 0.0;         // +-1 = +-180 degrees
    // Detail
    double clarity = 0.0;
    double sharpness = 0.0;   // 0..1
    // Finishing
    double vignette = 0.0;    // - lightens the corners / + darkens them
    double grain = 0.0;       // 0..1 amount of noise / grain
    int grainType = static_cast<int>(NoiseType::Gaussian);
    bool grainMono = false;   // true: the same value on R, G and B (brightness noise only)
    bool negative = false;

    // Index 0 = the combined RGB channel, 1..3 = red, green, blue.
    std::array<LevelsChannel, 4> levels;
    double outBlack = 0.0;
    double outWhite = 1.0;
    std::array<CurvePoints, 4> curves;

    bool isIdentity() const
    {
        if (exposure != 0.0 || brightness != 0.0 || contrast != 0.0 || highlights != 0.0
            || shadows != 0.0 || whites != 0.0 || blacks != 0.0 || gamma != 0.0
            || temperature != 0.0 || tint != 0.0 || saturation != 0.0 || vibrance != 0.0
            || hue != 0.0 || clarity != 0.0 || sharpness != 0.0 || vignette != 0.0 || grain != 0.0
            || negative
            || outBlack != 0.0 || outWhite != 1.0)
            return false;
        for (const auto &lv : levels) {
            if (!lv.isIdentity())
                return false;
        }
        for (const auto &curve : curves) {
            if (!isIdentityCurve(curve))
                return false;
        }
        return true;
    }

    bool operator==(const AdjustOp &) const = default;
};

// A "look" from the Filtros tab (see Looks.h for the catalogue): presetId is
// the look's id and `intensity` how far to move from the original toward it.
struct FilterPresetOp {
    QString presetId; // "cine", "sepia", "duo_oceano", ...
    double intensity = 1.0; // 0..1
    bool operator==(const FilterPresetOp &) const = default;
};

// A one-shot effect from the Efectos tab (see Effects.h): `effectId` names it,
// `values` are its slider values in the order the catalogue lists them (unused
// slots are 0) and `mix` is how far the result replaces the original (0..1).
// Unlike Ajustes/Filtros it is baked into the picture and undoable.
struct EffectOp {
    QString effectId;
    EffectValues values{};
    double mix = 1.0;
    bool operator==(const EffectOp &) const = default;
};

// A snapshot of the "live" overlay: the Ajustes sliders and the Filtros look,
// which are drawn on top of the picture by the GPU rather than baked into it. It
// changes no pixels, so replaying the stack skips it; what it does is put those
// settings into the same undo history as everything else, in chronological order.
// The state at any history position is simply the newest LiveOp before it (or
// "nothing" when there is none), so undo and redo restore it for free. One LiveOp
// is one gesture (a whole slider drag), not one slider tick.
struct LiveOp {
    AdjustOp adjust;
    QString lookId;        // "" = no look
    double lookAmount = 1.0;
    bool operator==(const LiveOp &) const = default;
};

using Operation = std::variant<CropOp, ResizeOp, RotateOp, FlipOp, AdjustOp, FilterPresetOp, EffectOp, LiveOp>;

// Everything that decides what the picture looks like, without the route taken to get
// there: the operations that change pixels, in order, plus the Ajustes/Filtros overlay.
// Two equal recipes make the same picture whatever the undo history looks like, which
// is what "is this still what I saved?" has to compare - a history position cannot say
// it (undo, then a different edit, lands on the same position with other content).
struct EditRecipe {
    std::vector<Operation> pixelOps;
    AdjustOp adjust;        // AdjustOp() whenever it changes nothing
    QString lookId;         // "" = no look
    double lookAmount = 1.0; // 1 whenever there is no look
    bool operator==(const EditRecipe &) const = default;
};

} // namespace core::edit
