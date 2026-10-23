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

# The languages the text recogniser comes with (Tesseract's "fast" models, tag 4.1.0, checked by hash). Others are
# downloaded by the program when the user asks (src/app/OcrLanguages.cpp).
mkdir -p "$APPDIR/usr/share/tessdata"
for entry in "eng 7d4322bd2a7749724879683fc3912cb542f19906c83bcc1a52132556427170b2" \
             "spa 6f2e04d02774a18f01bed44b1111f2cd7f3ba7ac9dc4373cd3f898a40ea6b464"; do
    set -- $entry
    fetch "https://github.com/tesseract-ocr/tessdata_fast/raw/4.1.0/$1.traineddata" "$2" "$1.traineddata"
    install -m644 "$TOOLS_DIR/$1.traineddata" "$APPDIR/usr/share/tessdata/$1.traineddata"
done

# linuxdeploy and its plugins are AppImages themselves: run them extracted, so this works without FUSE (containers, CI).
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$TOOLS_DIR:$PATH"
export LD_LIBRARY_PATH="$QT_ROOT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Qt Multimedia comes with two backends. The program uses FFmpeg (the default; it reads the MP4/H.264, MOV and MKV that
# "video to GIF" is for), so the GStreamer plugin is left out of the bundle: it would pull the whole GStreamer stack in
# (and fail to deploy where its libraries are not installed). linuxdeploy's Qt plugin finds Qt's plugins through qmake, so
# it is given a qmake that points to a copy of the plugin folder without that one file.
FILTERED_PLUGINS="$BUILD_DIR/qt-plugins-for-bundle"
rm -rf "$FILTERED_PLUGINS"
mkdir -p "$FILTERED_PLUGINS"
cp -a "$QT_ROOT_DIR/plugins/." "$FILTERED_PLUGINS/"
rm -f "$FILTERED_PLUGINS/multimedia/libgstreamermediaplugin.so"
cat > "$BUILD_DIR/qmake-for-bundle" <<WRAPPER
#!/usr/bin/env bash
real="$QT_ROOT_DIR/bin/qmake"
if [ "\${1:-}" = "-query" ]; then
    if [ "\${2:-}" = "QT_INSTALL_PLUGINS" ]; then echo "$FILTERED_PLUGINS"; exit 0; fi
    if [ "\$#" -eq 1 ]; then "\$real" -query | sed "s#^QT_INSTALL_PLUGINS:.*#QT_INSTALL_PLUGINS:$FILTERED_PLUGINS#"; exit 0; fi
fi
exec "\$real" "\$@"
WRAPPER
chmod +x "$BUILD_DIR/qmake-for-bundle"
export QMAKE="$BUILD_DIR/qmake-for-bundle"
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
