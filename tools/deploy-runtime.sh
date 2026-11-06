#!/usr/bin/env bash
# Lay out a runnable qimgv next to a build directory.
#
# A stripped deployment is what made PDF and video thumbnails disappear once:
# the exe alone has no poppler and no mpv grabber. Note the plugin layout - the
# grabber is resolved as "<app>/plugins/player_mpv" (a file), so the DLL goes
# directly into plugins/, not into a plugins/player_mpv/ directory.
#
# Usage: tools/deploy-runtime.sh <build-dir> <target-dir>

set -euo pipefail

BUILD=${1:?build dir, e.g. build6}
TARGET=${2:?target dir, e.g. /d/qimgv-run}

mkdir -p "$TARGET/plugins" "$TARGET/imageformats"

cp -f "$BUILD/qimgv/qimgv.exe" "$TARGET/"

# Image codecs (HEIF/JXL/AVIF) and their dependencies.
cp -f "$BUILD"/extras-pack/*.dll "$TARGET/" 2>/dev/null || true

# PDF support (poppler).
cp -f "$BUILD"/pdf/*.dll "$TARGET/" 2>/dev/null || true

# Qt plugins.
cp -f "$BUILD"/plugins/imageformats/* "$TARGET/imageformats/" 2>/dev/null || true

# Video frame grabber: flat, this exact name.
cp -f "$BUILD"/plugins/player_mpv/player_mpv.dll "$TARGET/plugins/player_mpv.dll" 2>/dev/null || true

echo "deployed to $TARGET"
echo "  dlls:      $(ls "$TARGET"/*.dll 2>/dev/null | wc -l)"
echo "  grabber:   $(test -f "$TARGET/plugins/player_mpv.dll" && echo yes || echo NO)"
