#!/usr/bin/env bash
# Deploys the optional format pack into an existing qimgv installation:
#
#   - WebP (animated too), TIFF, TGA, ICNS, WBMP, MNG, JPEG 2000
#   - HEIC/HEIF/AVIF (libheif), JPEG XL (libjxl), camera raw (libraw), PSD/PSB
#   - APNG animations
#   - video: the mpv player plugin and the video frame grabber
#
# Nothing here is required by the executable, so the pack can be installed,
# updated or left out on its own.
#
# usage: scripts/deploy-formats.sh [target-dir]

set -uo pipefail

TARGET="${1:-D:/qimgv-test}"
HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="${QIMGV_BUILD:-$HERE/../build6}"

# shellcheck source=deploy-lib.sh
. "$HERE/deploy-lib.sh"

if [ ! -f "$TARGET/qimgv.exe" ]; then
    echo "no qimgv.exe in $TARGET - run scripts/deploy-app.sh first" >&2
    exit 1
fi

mkdir -p "$TARGET/imageformats" "$TARGET/plugins"

echo "image formats"
# Qt's own extra format plugins
for plugin in qwebp qtiff qtga qicns qwbmp qmng qjp2; do
    src="$QT_PLUGINS/imageformats/$plugin.dll"
    [ -f "$src" ] || continue
    cp -f "$src" "$TARGET/imageformats/" && echo "  $plugin.dll"
done

# plugins built from this tree
for built in "$BUILD"/plugins/imageformats/*/*.dll; do
    [ -f "$built" ] || continue
    cp -f "$built" "$TARGET/imageformats/" && echo "  $(basename "$built") (built)"
done

echo "video"
if [ -f "$BUILD/plugins/player_mpv/player_mpv.dll" ]; then
    cp -f "$BUILD/plugins/player_mpv/player_mpv.dll" "$TARGET/plugins/"
    echo "  player_mpv.dll (built)"
else
    echo "  !! player_mpv.dll was not built (VIDEO_SUPPORT/MPV_DIR?)" >&2
fi
# libmpv itself: the plugin links it, but the dependency walk only looks in the
# Qt runtime directory, so copy it from the mpv kit the build used
MPV_KIT="${MPV_DIR:-$HERE/../3rdparty/mpv}"
for candidate in "$MPV_KIT/bin/libmpv-2.dll" "$MPV_KIT/lib/x86_64/libmpv-2.dll"; do
    if [ -f "$candidate" ]; then
        if [ ! -f "$TARGET/libmpv-2.dll" ] || [ "$candidate" -nt "$TARGET/libmpv-2.dll" ]; then
            cp -f "$candidate" "$TARGET/" && echo "  libmpv-2.dll"
        fi
        break
    fi
done

echo "dependencies"
deploy_closure_of "$TARGET" "$TARGET"/imageformats/*.dll
deploy_closure_of "$TARGET" "$TARGET"/plugins/*.dll

echo "format pack -> $TARGET"
