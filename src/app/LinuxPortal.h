#pragma once

#include <QString>
#include <functional>

class QObject;

// Linux only. The desktop portals (org.freedesktop.portal.*, DBus) are how a sandboxed program - Flatpak above all -
// asks the desktop to do what it may not do itself: put a file in the REAL trash and change the wallpaper. They also
// work outside a sandbox on any desktop that runs xdg-desktop-portal (GNOME, KDE, Cinnamon...), and when they are
// missing the callers fall back to what they did before.
namespace LinuxPortal {

// True when running inside a Flatpak sandbox (the runtime creates /.flatpak-info).
bool inFlatpak();

// Moves the file to the user's trash through the Trash portal. False when the portal is not there or refused.
bool trashFile(const QString &path);

enum class Outcome { Done, Cancelled, Failed };

// Sets the picture as the wallpaper (desktop and lock screen where the desktop allows it) through the Wallpaper
// portal. The file is handed over as an open descriptor, so it works for a path the portal cannot see.
//
// The portal answers in two steps: the call only returns a request handle, and the real result (done, cancelled by the
// person, failed) arrives later as that request's Response signal. This therefore does not block: `done` is called
// once, from the event loop, with the real outcome (Failed also when the portal is not there or says nothing for two
// minutes). `context` bounds the lifetime: if it is destroyed first, `done` is not called. `parentWindow` is the
// portal's window identifier ("x11:<hex id>"), or empty when there is none to give (Wayland).
void setWallpaper(const QString &path, const QString &parentWindow, QObject *context, std::function<void(Outcome)> done);

} // namespace LinuxPortal
