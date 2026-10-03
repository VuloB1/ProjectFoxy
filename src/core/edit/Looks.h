#pragma once

#include "AdjustMath.h"
#include <QImage>
#include <QString>
#include <vector>

namespace core::edit {

// The Filtros catalogue. A look is not an image or a lookup file - it is a
// small recipe made of the same stages the Ajustes tab uses (tone/color
// controls, curves) plus the look-only extras in GradeExtras (gradient map,
// split toning, vignette, grain), so it costs no assets, renders identically
// on the GPU preview and in the saved file, and works on any photo.
struct LookSpec {
    QString id;
    QString name;  // display name (Spanish)
    QString group; // one of lookGroups()
    AdjustOp tone;
    GradeExtras extras;
};

struct LookGroup {
    QString id;
    QString name;
};

const std::vector<LookGroup> &lookGroups();
const std::vector<LookSpec> &allLooks();

// nullptr for an unknown id (including "" = no look).
const LookSpec *findLook(const QString &id);

// `source` with look `id` applied at `amount` (0..1). Unknown ids return the
// source unchanged.
QImage applyLook(const QImage &source, const QString &id, double amount);

} // namespace core::edit
