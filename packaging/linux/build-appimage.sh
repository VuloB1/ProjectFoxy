#!/usr/bin/env bash
# Builds ProjectFoxy-<version>-x86_64.AppImage from an already built linux-release tree.
#
#   cmake --preset linux-release -DCMAKE_PREFIX_PATH=<Qt 6.7.3 gcc_64>
#   cmake --build --preset linux-release
#   QT_ROOT_DIR=<Qt 6.7.3 gcc_64> packaging/linux/build-appimage.sh
#
# Build it on the OLDEST distribution you want to support (CI uses Ubuntu 22.04): an AppImage runs on every distribution
# whose glibc is at least as new as the one it was built on. The native libraries (libvips, LibRaw, libheif, exiv2...)
# are static in the vcpkg triplet, Qt is bundled by linuxdeploy; only glibc and the graphics stack (libGL, libEGL,
# X11/Wayland client libraries) come from the host - those must come from the host to match its driver.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/linux-release}"
OUT_DIR="${OUT_DIR:-$ROOT/dist}"
TOOLS_DIR="${TOOLS_DIR:-$HOME/.cache/projectfoxy-tools}"
: "${QT_ROOT_DIR:?set QT_ROOT_DIR to the Qt 6.7.3 gcc_64 folder}"

APP_ID="io.github.vulob1.ProjectFoxy"
VERSION="$(sed -n 's/^project(ImageViewer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
ARCH="x86_64"

# The two tools are pinned by version and checked by hash: they run with the privileges of whoever builds.
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20250213-2/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_SHA256="4648f278ab3ef31f819e67c30d50f462640e5365a77637d7e6f2ad9fd0b4522a"
PLUGIN_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
PLUGIN_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

fetch() { # url sha256 file
    local url="$1" sha="$2" file="$TOOLS_DIR/$3"
    if [ ! -x "$file" ] || ! echo "$sha  $file" | sha256sum -c --status; then
        mkdir -p "$TOOLS_DIR"
        curl -fsSL -o "$file" "$url"
        echo "$sha  $file" | sha256sum -c --status || { echo "checksum mismatch for $3" >&2; exit 1; }
        chmod +x "$file"
    fi
}
fetch "$LINUXDEPLOY_URL" "$LINUXDEPLOY_SHA256" linuxdeploy-x86_64.AppImage
fetch "$PLUGIN_QT_URL" "$PLUGIN_QT_SHA256" linuxdeploy-plugin-qt-x86_64.AppImage

APPDIR="$BUILD_DIR/AppDir"
rm -rf "$APPDIR"
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

# linuxdeploy and its plugins are AppImages themselves: run them extracted, so this works without FUSE (containers, CI).
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$TOOLS_DIR:$PATH"
export QMAKE="$QT_ROOT_DIR/bin/qmake"
export LD_LIBRARY_PATH="$QT_ROOT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# The plugin reads the QML imports of the interface (so QtQuick.Effects, Controls, Dialogs... are bundled).
export QML_SOURCES_PATHS="$ROOT/qml"
# X11 (xcb) is always bundled; Wayland natively as well, and the platform themes (GTK / desktop portal dialogs).
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so"
export DEPLOY_PLATFORM_THEMES=1
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"

# The Wayland platform plugin loads these at run time (window shell, GPU buffers, client-side decorations). Without
# them it fails with "No shell integration named xdg-shell" and Qt silently falls back to X11. linuxdeploy does not
# find them by itself, but it resolves the libraries of every plugin that is already inside the AppDir.
for dir in wayland-shell-integration wayland-graphics-integration-client wayland-decoration-client; do
    mkdir -p "$APPDIR/usr/plugins"
    cp -r "$QT_ROOT_DIR/plugins/$dir" "$APPDIR/usr/plugins/"
done

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"
linuxdeploy-x86_64.AppImage \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/ProjectFoxy" \
    --desktop-file "$APPDIR/usr/share/applications/$APP_ID.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/$APP_ID.png" \
    --plugin qt \
    --output appimage

# The output name comes from the desktop file's Name ("Project_Foxy-...").
produced="$(ls -t Project_Foxy-*"$ARCH".AppImage 2>/dev/null | head -n1 || true)"
final="ProjectFoxy-$VERSION-$ARCH.AppImage"
[ -n "$produced" ] && mv -f "$produced" "$final"
echo "built: $OUT_DIR/$final"
