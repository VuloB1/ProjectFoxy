#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>

void Autostart::apply(bool enabled, bool hidden)
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enabled) {
        QString command = QLatin1Char('"') + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + QLatin1Char('"');
        if (hidden)
            command += QStringLiteral(" --background");
        RegSetValueExW(key, L"ProjectFoxy", 0, REG_SZ, reinterpret_cast<const BYTE *>(command.utf16()), DWORD((command.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, L"ProjectFoxy");
    }
    RegCloseKey(key);
}
#else
void Autostart::apply(bool, bool) {}
#endif
