#pragma once

#include "edit/AdjustMath.h"
#include "edit/Operations.h"

#include <QImage>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

// The vocabulary of the Ajustes panel: what each slider is called, how far it may go, and how a request to change
// one turns the current `AdjustOp` into the next. Everything here is a function of its arguments (no state, nothing
// shared, nothing Qt Quick): the live state itself, the history entry it becomes and the signals that follow are
// AppController's.
namespace AdjustModel {

// A change the caller applies: the new state, and the key that lets consecutive moves of the same control merge into
// one undo step (empty = never merges).
struct Change {
    core::edit::AdjustOp op;
    QString key;
};

// The flat map QML reads (`appController.adjust`): every slider name -> value, plus "levels" and "curves".
QVariantMap toMap(const core::edit::AdjustOp &op);

// Each of these returns nothing when the request changes nothing (unknown name or channel, same value).
std::optional<Change> setParam(const core::edit::AdjustOp &op, const QString &name, double value); // clamped to the slider's range
std::optional<Change> setGrainType(const core::edit::AdjustOp &op, int type);                      // 0..3
std::optional<Change> setGrainMono(const core::edit::AdjustOp &op, bool mono);
std::optional<Change> setNegative(const core::edit::AdjustOp &op, bool on);
// Levels of one channel (0 = RGB, 1..3 = R, G, B): black below white, gamma 0.1..9.99.
std::optional<Change> setLevels(const core::edit::AdjustOp &op, int channel, double inBlack, double gamma, double inWhite);
std::optional<Change> setOutputLevels(const core::edit::AdjustOp &op, double black, double white);
// `points`: what QML hands over ({x, y} objects or Qt.point()). Sorted, clamped to 0..1, at most 16; the straight
// line is stored as "no curve".
std::optional<Change> setCurvePoints(const core::edit::AdjustOp &op, int channel, const QVariantList &points);
// Levels and output levels back to neutral. Always a change (as it was before this was moved here).
Change resetLevels(const core::edit::AdjustOp &op);

// The 256 samples of the curve through `points`, for the editor to draw.
QVariantList curveSamples(const QVariantList &points);

// "contrast" | "levels" | "color"; nothing for any other text.
std::optional<core::edit::AutoAdjustKind> autoKind(const QString &kind);

// Whether the tone-and-colour stage (Grade) changes anything: clarity, sharpness, vignette and grain belong to the
// Detail stage, which always runs.
bool gradeActive(const core::edit::AdjustOp &op);

} // namespace AdjustModel
