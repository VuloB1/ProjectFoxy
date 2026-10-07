#pragma once

#include <QCoreApplication>
#include <QString>

namespace core {

// The program's own text is written in Spanish and the Spanish text is the key of its translations: the
// application installs a translator (src/app/Translations.cpp) that looks it up in the language chosen in
// Configuración. Without a translator (the tests, a missing entry) the Spanish comes back unchanged.
inline QString tr(const char *spanish)
{
    return QCoreApplication::translate("core", spanish);
}

inline QString tr(const QString &spanish)
{
    return spanish.isEmpty() ? spanish : QCoreApplication::translate("core", spanish.toUtf8().constData());
}

} // namespace core
