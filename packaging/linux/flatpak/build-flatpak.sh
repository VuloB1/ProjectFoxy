#!/usr/bin/env bash
# Builds the Flatpak of Project Foxy and a single-file bundle (ProjectFoxy-<version>.flatpak) that can be installed with
#   flatpak install --user ProjectFoxy-<version>.flatpak
#
# Needs flatpak and flatpak-builder, and the Flathub remote (the KDE runtime and SDK 6.10 come from it):
#   flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
#
#   packaging/linux/flatpak/build-flatpak.sh            build the Flatpak and the bundle
#   packaging/linux/flatpak/build-flatpak.sh --install  ...and install it for this user
#   packaging/linux/flatpak/build-flatpak.sh --test     build with the tests and run them against the runtime's Qt and
#                                                       libraries (no display: all but the GPU and startup tests)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
APP_ID="io.github.vulob1.ProjectFoxy"
VERSION="$(sed -n 's/^project(ImageViewer VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
WORK="${WORK_DIR:-$ROOT/build/flatpak}"
OUT="${OUT_DIR:-$ROOT/dist}"

mkdir -p "$WORK" "$OUT"
cd "$HERE"

if [ "${1:-}" = "--test" ]; then
    # A copy of the manifest (next to it, so the relative source path still works) that builds the tests and runs them
    # right after the build, inside the same sandbox the program is built in.
    manifest=".test-$APP_ID.yml"
    trap 'rm -f "$HERE/$manifest"' EXIT
    # (post-install runs in the build directory itself, where CTestTestfile.cmake is)
    test_cmd="${TEST_CMD:-QT_QPA_PLATFORM=offscreen ctest -j4 -E 'test_gpu_parity|smoke_startup' --output-on-failure}"
    awk -v cmd="$test_cmd" '
        $0 == "      - -DIMAGEVIEWER_BUILD_TESTS=OFF" {
            print "      - -DIMAGEVIEWER_BUILD_TESTS=ON"
            print "    post-install:"
            gsub(/\x27/, "\x27\x27", cmd)          # YAML single-quoted scalar
            print "      - \x27" cmd "\x27"
            next
        }
        { print }' "$APP_ID.yml" > "$manifest"
    flatpak-builder --user --force-clean --install-deps-from=flathub \
        --state-dir="$WORK/state" "$WORK/build-test" "$manifest"
    exit 0
fi

install_args=()
[ "${1:-}" = "--install" ] && install_args=(--user --install)

flatpak-builder --user --force-clean --install-deps-from=flathub \
    --state-dir="$WORK/state" --repo="$WORK/repo" "${install_args[@]}" \
    "$WORK/build" "$APP_ID.yml"

flatpak build-bundle "$WORK/repo" "$OUT/ProjectFoxy-$VERSION.flatpak" "$APP_ID"
echo "built: $OUT/ProjectFoxy-$VERSION.flatpak"
