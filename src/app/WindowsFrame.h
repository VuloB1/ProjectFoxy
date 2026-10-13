#pragma once

#include <QQuickWindow>

// Windows only. The window has no native frame (Qt::FramelessWindowHint), which also takes away what Windows uses
// for snapping to the screen edges, the snap-layouts flyout of Windows 11 and the system resize. This gives them
// back: the window keeps the resizable / maximisable styles but its client area covers all of it, and the title bar
// and the edges answer Windows' hit tests (caption, borders, the maximise button) so Windows does the work.
// Items looked up by objectName in the QML: "titleBar" (its height is the caption) and "maximizeButton" (a Rectangle
// that gets the boolean property `nativeHover`).
void installWindowsFrame(QQuickWindow *window);
