#!/usr/bin/env bash
# Deploys the program itself: qimgv.exe, the Qt runtime and the basic formats
# (JPEG, PNG, GIF, BMP, ICO, SVG). Everything else - WebP/TIFF/JP2, HEIC/AVIF,
# JPEG XL, camera raw, PSD, APNG and video - lives in the optional pack, see
# scripts/deploy-formats.sh.
#
# usage: scripts/deploy-app.sh [target-dir]
#
# The target defaults to D:/qimgv-test. The build directory can be overridden
# with QIMGV_BUILD.

set -uo pipefail

TARGET="${1:-D:/qimgv-test}"
HERE="$(cd "$(dirname "$0")" && pwd)"

# the most recent build directory that actually has an executable, so an old
# "build6" default cannot break a release; QIMGV_BUILD always wins
if [ -z "${QIMGV_BUILD:-}" ]; then
    for candidate in build9 build8 build7 build6; do
        [ -f "$HERE/../$candidate/qimgv/qimgv.exe" ] && { BUILD="$HERE/../$candidate"; break; }
    done
fi
BUILD="${BUILD:-${QIMGV_BUILD:-$HERE/../build8}}"

# shellcheck source=deploy-lib.sh
. "$HERE/deploy-lib.sh"

EXE="$BUILD/qimgv/qimgv.exe"
if [ ! -f "$EXE" ]; then
    echo "no executable at $EXE - build first, or set QIMGV_BUILD to the build directory" >&2
    exit 1
fi

mkdir -p "$TARGET"
cp -f "$EXE" "$TARGET/qimgv.exe"
echo "qimgv.exe"

# translations
if [ -d "$BUILD/qimgv/translations" ]; then
    mkdir -p "$TARGET/translations"
    cp -f "$BUILD/qimgv/translations"/*.qm "$TARGET/translations/" 2>/dev/null || true
fi

# license texts: the executable embeds Roboto, Glow Sans and the icon sets, and
# qimgv itself is GPLv3, so every distribution has to carry them
mkdir -p "$TARGET/licenses"
cp -f "$HERE/../LICENSE" "$TARGET/licenses/GPL-3.0.txt" 2>/dev/null || true
cp -f "$HERE/../LICENSES/"*.txt "$TARGET/licenses/" 2>/dev/null || true
cp -f "$HERE/../LICENSES/README.md" "$TARGET/licenses/" 2>/dev/null || true

# Qt runtime and the plugins a plain image viewer needs. windeployqt copies
# every image format plugin it knows, so the optional ones are pruned below.
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw \
    "$TARGET/qimgv.exe" >/dev/null 2>&1 || echo "windeployqt reported a problem" >&2

# the pack owns these; keep them out of the base installation
for plugin in qwebp qtiff qtga qicns qwbmp qmng qjp2 qpdf; do
    rm -f "$TARGET/imageformats/$plugin.dll"
done

# windeployqt also deploys the GLib networkinfo backend, which is a Unix plugin:
# Windows uses qnetworklistmanager, and libgio/libgobject are not part of the
# package, so this plugin could never load. Drop it instead of shipping a plugin
# that fails on every start.
rm -f "$TARGET/networkinformation/qglib.dll"

echo "dependencies"
deploy_closure_of "$TARGET" "$TARGET/qimgv.exe"
deploy_closure_of "$TARGET" "$TARGET"/platforms/*.dll "$TARGET"/imageformats/*.dll \
    "$TARGET"/styles/*.dll "$TARGET"/iconengines/*.dll 2>/dev/null

echo "base installation -> $TARGET"
