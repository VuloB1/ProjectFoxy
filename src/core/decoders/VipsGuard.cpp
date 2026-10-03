// libvips and GLib first: GLib has a struct member called `signals`, which Qt
// defines as a macro once QObject's headers are in.
#include <vips/vips8>

#include "VipsGuard.h"

#include <QtGlobal>
#include <mutex>

namespace core {

namespace {
bool g_vipsReady = false;
std::string g_vipsError;
} // namespace

std::string vipsInitError()
{
    return g_vipsError;
}

bool ensureVipsInitialized()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        if (VIPS_INIT("ImageViewer")) {
            g_vipsError = vips_error_buffer();
            qWarning("libvips initialization failed: %s", g_vipsError.c_str());
            return;
        }
        g_vipsReady = true;
        // The app does its own caching (decoded documents, thumbnails). libvips'
        // operation cache would also keep recent files open - on Windows that
        // blocks deleting, renaming or replacing a picture that was merely viewed -
        // and could return a stale result for a file that has been overwritten.
        vips_cache_set_max(0);
        vips_cache_set_max_mem(0);
        vips_cache_set_max_files(0);
    });
    return g_vipsReady;
}

} // namespace core