#pragma once

// Windows only (elsewhere it does nothing): the "run at sign-in" entry of the current user
// (HKCU\Software\Microsoft\Windows\CurrentVersion\Run). `hidden` adds --background, which starts without a window.
namespace Autostart {
void apply(bool enabled, bool hidden);
}
