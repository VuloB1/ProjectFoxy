#pragma once

#include <QDate>
#include <QString>

namespace core {

// The holidays the window is decorated for (qml/FestiveDecor.qml). `None` is every other day.
enum class Festival { None, Halloween, DayOfTheDead, Christmas, NewYear, ThreeKings, Valentine, StPatrick, Easter };

// Easter Sunday of `year` (Gregorian calendar).
QDate easterSunday(int year);

// The holiday `date` falls in:
//   Halloween 1-31 Oct · Día de los Muertos 1-3 Nov · Navidad 1-26 Dec · Año Nuevo 27 Dec-2 Jan ·
//   Noche de Reyes 3-6 Jan · San Valentín 7-15 Feb · San Patricio 12-17 Mar ·
//   Pascua: the 7 days before Easter Sunday and the Monday after it (it wins over San Patricio when both apply).
Festival festivalOn(const QDate &date);

// The name used in settings, file names and QML ("halloween", "muertos", "navidad", "nuevoano", "reyes",
// "sanvalentin", "sanpatricio", "pascua"); empty for `None`.
QString festivalKey(Festival festival);

// The reverse of festivalKey(); `None` for anything that is not a key.
Festival festivalFromKey(const QString &key);

} // namespace core
