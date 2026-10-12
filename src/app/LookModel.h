#pragma once

#include "edit/Operations.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>

// The vocabulary of the Filtros panel: the looks that exist, and what a picked look (its id and how strongly it is
// applied) means in the numbers the Grade shader reads. Like AdjustModel, only functions of their arguments: the live
// state, the history entry and the signals are AppController's.
namespace LookModel {

// What is stored when `presetId` is picked at `intensity`: an unknown or empty id means "no look", and the intensity
// is clamped to 0..1.
struct Picked {
    QString id;
    double amount = 1.0;
};
Picked pick(const QString &presetId, double intensity);

// A look is applied when it exists and its amount is above 0.
bool active(const QString &id, double amount);

// The tone table a look is made of (neutral for no look): what its lookup table is built from.
core::edit::AdjustOp tone(const QString &id);

// `lookMap`: the look resolved into the numbers Grade.frag binds to (amount is 0 for no look).
QVariantMap toMap(const QString &id, double amount);

// The lists the panel shows (names go through core::tr).
QVariantList list();
QVariantList groups();

} // namespace LookModel
