#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

// Where the program keeps what it remembers (settings.ini, the thumbnail cache).
//
// Normally that is the per-user folders of Windows. In PORTABLE MODE - a file called
// "portable.txt" next to ImageViewer.exe - everything goes into a "datos" folder next to the
// executable instead, so a copy on a USB stick or shared with a friend leaves nothing behind
// on the machine it runs on and can be deleted by deleting its folder.
namespace AppPaths {

inline bool portable()
{
    return QFileInfo::exists(QCoreApplication::applicationDirPath() + QStringLiteral("/portable.txt"));
}

inline QString configDir()
{
    return portable() ? QCoreApplication::applicationDirPath() + QStringLiteral("/datos")
                      : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

inline QString cacheDir()
{
    return portable() ? QCoreApplication::applicationDirPath() + QStringLiteral("/datos/cache")
                      : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

} // namespace AppPaths
