#pragma once

#include <mutex>
#include <string>

namespace core {

// libvips (and the GLib/GObject/GIO stack it's built on) has produced
// intermittent STATUS_HEAP_CORRUPTION crashes in this app - always inside
// glib/gio/gobject on the faulting thread's stack, at an identical crash
// address across every occurrence. That signature points at a race in some
// one-time lazy initialization inside GIO (most likely its extension-module
// scanning) that isn't safe to enter from two threads simultaneously - which
// this app's own architecture invites, since image decoding and thumbnail
// generation both run vips operations from a QThreadPool, quite possibly two
// of them the very first time at once (e.g. opening a folder immediately
// requests a burst of filmstrip thumbnails in parallel).
//
// Every entry point into libvips's public API from this codebase takes this
// lock first, so vips is never entered by more than one of OUR threads at a
// time. This does NOT stop vips from using ITS OWN internal worker threads
// for pipeline evaluation (write_to_memory/write_to_file etc. spawn those
// internally) - those run *inside* an already-held call, not as a second
// concurrent entry, so vips' own per-image parallelism is unaffected. What's
// serialized is just our own top-level calls relative to each other.
inline std::mutex &vipsEntryMutex()
{
    static std::mutex m;
    return m;
}

// libvips needs exactly one vips_init() per process. Decoders and the image
// writer can be reached first from any thread (or from a test with no decode
// before it), so every user calls this instead of relying on main() having done
// it. Thread-safe and cheap after the first call. (Defined in VipsGuard.cpp so
// this header does not pull the libvips/GLib headers in: GLib has a struct member
// called `signals`, which breaks when Qt's `signals` macro is already defined.)
//
// Returns false when libvips could not start; the callers turn that into an error
// result (vipsInitError() says why) instead of going on to call into a library that
// is not there.
bool ensureVipsInitialized();
std::string vipsInitError();
} // namespace core
