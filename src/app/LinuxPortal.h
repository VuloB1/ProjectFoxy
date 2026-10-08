#pragma once

#include <QString>

// Linux only. The desktop portals (org.freedesktop.portal.*, DBus) are how a sandboxed program - Flatpak above all -
// asks the desktop to do what it may not do itself: put a file in the REAL trash and change the wallpaper. They also
// work outside a sandbox on any desktop that runs xdg-desktop-portal (GNOME, KDE, Cinnamon...), and when they are
// missing the callers fall back to what they did before.
namespace LinuxPortal {

// True when running inside a Flatpak sandbox (the runtime creates /.flatpak-info).
bool inFlatpak();

// Moves the file to the user's trash through the Trash portal. False when the portal is not there or refused.
bool trashFile(const QString &path);

// Sets the picture as the wallpaper (desktop and lock screen where the desktop allows it) through the Wallpaper
// portal. The file is handed over as an open descriptor, so it works for a path the portal cannot see.
// False when the portal is not there.
bool setWallpaper(const QString &path);

} // namespace LinuxPortal
