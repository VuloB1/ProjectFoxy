#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>

// Where the program keeps what it remembers (settings.ini, the thumbnail cache).
//
// Normally that is the per-user folders of Windows. In PORTABLE MODE - a file called
// "portable.txt" next to ProjectFoxy.exe - everything goes into a "datos" folder next to the
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

// Before the program was renamed it kept its files under "ImageViewer". The first time the new
// settings file is missing, take over the old one (copy; the old folder is left alone).
// `oldFile` is a parameter so the rule can be tested.
inline bool migrateSettings(const QString &newFile, const QString &oldFile)
{
    if (QFileInfo::exists(newFile) || !QFileInfo::exists(oldFile))
        return false;
    QDir().mkpath(QFileInfo(newFile).absolutePath());
    return QFile::copy(oldFile, newFile);
}

inline QString settingsFile()
{
    const QString file = configDir() + QStringLiteral("/settings.ini");
    if (!portable() && !QStandardPaths::isTestModeEnabled())
        migrateSettings(file, QDir::cleanPath(configDir() + QStringLiteral("/../../ImageViewer/ImageViewer/settings.ini")));
    return file;
}

} // namespace AppPaths
