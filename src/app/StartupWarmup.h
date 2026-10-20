#pragma once

#include <thread>

// Work that the window will need a moment later, done on a helper thread while the main thread builds the interface:
// the first text layout of a program on Windows reads the font files of the interface font and of its fallbacks
// (Segoe UI, Emoji, the CJK families...: ~200 ms), and the first Direct3D device loads the graphics driver (~100 ms).
// Both are process-wide, so what the helper loads is there when the main thread asks for it (and if the main thread
// gets there first it simply waits for the same work instead of redoing it). Make one once QGuiApplication exists.
class StartupWarmup {
public:
    StartupWarmup();
    ~StartupWarmup(); // waits for the helper (it is short), also when main() leaves early

private:
    std::thread m_helper;
};
