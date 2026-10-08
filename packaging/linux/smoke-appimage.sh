#!/usr/bin/env bash
# Startup smoke test of the AppImage itself (what tests/smoke_startup.cmake does for the build tree): starts it on a
# photo inside a virtual X server, lets it run 6 seconds and fails if it dies or writes anything to its error output.
# That is what proves the bundle is complete: a missing Qt plugin or QML module shows up here and nowhere else.
#
#   packaging/linux/smoke-appimage.sh dist/ProjectFoxy-0.1.0-x86_64.AppImage
set -uo pipefail

APPIMAGE="$(readlink -f "${1:?usage: smoke-appimage.sh <file.AppImage>}")"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
IMAGE="$ROOT/tests/fixtures/exif_orient_6.jpg"
LOG="$(mktemp)"

export APPIMAGE_EXTRACT_AND_RUN=1      # no FUSE needed
export QT_QPA_PLATFORM=xcb
export LIBGL_ALWAYS_SOFTWARE=1         # no GPU on a build server
export QT_FORCE_STDERR_LOGGING=1

timeout 6 xvfb-run -a -s "-screen 0 1280x1024x24" "$APPIMAGE" "$IMAGE" 2> "$LOG"
code=$?
# 124 = the timeout stopped it: it was still running, which is what a healthy start looks like.
if [ "$code" -ne 124 ]; then
    echo "The AppImage did not stay alive (exit code $code)." >&2
    cat "$LOG" >&2
    exit 1
fi
# Messages that come from the virtual X server or the software GL driver, not from the program.
msgs="$(grep -v -E 'libEGL warning|MESA|glx: failed|DRI|Xlib|XDG_RUNTIME_DIR|propagateSizeHints' "$LOG" || true)"
if [ -n "$msgs" ]; then
    echo "Unexpected messages while starting up:" >&2
    echo "$msgs" >&2
    exit 1
fi
echo "AppImage started, stayed alive for 6 s and wrote nothing to its error output."
