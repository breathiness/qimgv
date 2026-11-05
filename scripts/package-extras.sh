#!/usr/bin/env bash
# Builds the single "extras" archive that goes with a release:
#
#   qimgv-extras-<version>.zip
#     imageformats/   extra image formats (WebP/TIFF/TGA/ICNS/WBMP/MNG/JP2,
#                     HEIC/AVIF/JXL/RAW/PSD from the plugins in this tree)
#     plugins/        player_mpv.dll (video)
#     pdf/            pdfprovider.dll and the poppler libraries it needs
#     README.txt      one paragraph: unzip over the qimgv folder
#
# Everything in it is optional: qimgv starts and works without the archive, and a
# missing piece only removes the formats it provides. Unzipping it into the
# program folder is the whole installation - there is nothing to register.
#
# usage: scripts/package-extras.sh [output-dir] [app-dir]
#
# app-dir is what the archive is cut against: every file it already contains is
# left out, so the pack only carries what the program package does not ship. It
# must therefore be a CLEAN install of the release program package - unzip
# qimgv-<version>-win64.zip into a temporary folder and pass that.
#
# Do not pass a working installation that already had the extras applied: its
# libmpv-2.dll, libheif/jxl/raw, libwebp/tiff and friends would all be treated as
# "already there" and dropped, leaving an archive with plugins that have no
# libraries. That pack installs without an error and then quietly offers no video,
# no HEIC and no WebP. Without app-dir nothing is dropped, which is always correct
# but makes the archive ~60 MB larger.
#
# The script refuses an app-dir that looks like it already has the extras; set
# QIMGV_PACK_FROM_POPULATED=1 to build the delta anyway (for example to update an
# archive for users who already installed a previous one).

set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
BUILD="${QIMGV_BUILD:-$REPO/build6}"
OUT="${1:-$REPO/dist}"
APP_DIR="${2:-}"

# shellcheck source=deploy-lib.sh
. "$HERE/deploy-lib.sh"

VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$REPO/CMakeLists.txt" | head -1)"
[ -n "$VERSION" ] || VERSION="0.0.0"

STAGE="$BUILD/extras-pack"
NAME="qimgv-extras-$VERSION"

if [ ! -f "$BUILD/qimgv/qimgv.exe" ]; then
    echo "no build in $BUILD - run ninja first" >&2
    exit 1
fi

rm -rf "$STAGE"
mkdir -p "$STAGE/imageformats" "$STAGE/plugins" "$STAGE/pdf"

echo "image formats"
for plugin in qwebp qtiff qtga qicns qwbmp qmng qjp2; do
    src="$QT_PLUGINS/imageformats/$plugin.dll"
    [ -f "$src" ] || continue
    cp -f "$src" "$STAGE/imageformats/" && echo "  $plugin.dll"
done
for built in "$BUILD"/plugins/imageformats/*/*.dll; do
    [ -f "$built" ] || continue
    cp -f "$built" "$STAGE/imageformats/" && echo "  $(basename "$built") (built)"
done

echo "video"
if [ -f "$BUILD/plugins/player_mpv/player_mpv.dll" ]; then
    cp -f "$BUILD/plugins/player_mpv/player_mpv.dll" "$STAGE/plugins/" && echo "  player_mpv.dll"
fi
MPV_KIT="${MPV_DIR:-$REPO/3rdparty/mpv}"
for candidate in "$MPV_KIT/bin/libmpv-2.dll" "$MPV_KIT/lib/x86_64/libmpv-2.dll"; do
    if [ -f "$candidate" ]; then
        cp -f "$candidate" "$STAGE/" && echo "  libmpv-2.dll"
        break
    fi
done

echo "pdf"
if [ -f "$BUILD/pdf/pdfprovider.dll" ]; then
    cp -f "$BUILD/pdf/pdfprovider.dll" "$STAGE/pdf/" && echo "  pdf/pdfprovider.dll"
else
    echo "  !! pdfprovider.dll was not built (poppler-qt6 missing?)" >&2
fi

echo "dependencies"
deploy_closure_of "$STAGE" "$STAGE"/imageformats/*.dll
deploy_closure_of "$STAGE" "$STAGE"/plugins/*.dll
deploy_closure_of "$STAGE/pdf" "$STAGE/pdf"/pdfprovider.dll

if [ -n "$APP_DIR" ] && [ -d "$APP_DIR" ]; then
    # A program folder that already carries the extras is the wrong reference: its
    # libraries would be dropped and the archive would ship plugins without them.
    pop=()
    [ -f "$APP_DIR/libmpv-2.dll" ] && pop+=("libmpv-2.dll")
    [ -f "$APP_DIR/pdf/pdfprovider.dll" ] && pop+=("pdf/")
    [ -f "$APP_DIR/plugins/player_mpv.dll" ] && pop+=("plugins/")
    [ -f "$APP_DIR/imageformats/qwebp.dll" ] && pop+=("imageformats/")
    if [ ${#pop[@]} -gt 0 ] && [ "${QIMGV_PACK_FROM_POPULATED:-0}" != "1" ]; then
        echo "error: $APP_DIR already contains the extras (${pop[*]})." >&2
        echo "       Cut the archive against a clean unzip of the program package" >&2
        echo "       instead, or set QIMGV_PACK_FROM_POPULATED=1 to mean it." >&2
        exit 1
    fi

    echo "dropping what the program folder already has"
    dropped=0
    while IFS= read -r f; do
        name="$(basename "$f")"
        if [ -f "$APP_DIR/$name" ]; then
            rm -f "$f"
            dropped=$((dropped + 1))
        fi
    done < <(find "$STAGE" -maxdepth 1 -type f -name '*.dll'
             find "$STAGE/pdf" -maxdepth 1 -type f -name '*.dll')
    echo "  $dropped dll(s) shared with $APP_DIR"
else
    echo "no program folder given: the archive carries every dependency" >&2
fi

cat > "$STAGE/README.txt" <<EOF
qimgv $VERSION - extra formats
==============================

Unzip this archive into the folder that holds qimgv.exe (the same folder as the
progressively numbered subdirectories), keeping the folder names. That is the
whole installation; nothing needs to be registered.

  imageformats/   WebP, TIFF, TGA, ICNS, WBMP, MNG, JPEG 2000 and, when built,
                  HEIC/HEIF, AVIF, JPEG XL, camera raw, PSD, APNG
  plugins/        video playback (player_mpv.dll)
  pdf/            PDF documents, rendered through poppler

Everything here is optional. qimgv starts without it, and a missing folder only
means the formats it provides are not listed; delete a folder to disable it again.

Licences: the extra Qt image plugins follow Qt's licence, poppler is
GPL-2.0-or-later, mpv is LGPL-2.1-or-later with parts under GPL. qimgv itself is
GPLv3; the full texts are unchanged by this archive.
EOF

mkdir -p "$OUT"
ARCHIVE="$OUT/$NAME.zip"
rm -f "$ARCHIVE"
if command -v zip >/dev/null 2>&1; then
    ( cd "$STAGE" && zip -r -q -9 "$ARCHIVE" . )
else
    # msys2 without the zip package: compress with PowerShell. pwsh (7) writes
    # entry names with forward slashes like zip does; Windows PowerShell 5.1 writes
    # backslashes, which other platforms cannot read, so it is the last resort.
    ARCHIVE_WIN="$(cygpath -w "$ARCHIVE")"
    STAGE_WIN="$(cygpath -w "$STAGE")"
    PS_BIN=""
    command -v pwsh >/dev/null 2>&1 && PS_BIN="pwsh"
    [ -z "$PS_BIN" ] && command -v powershell.exe >/dev/null 2>&1 && PS_BIN="powershell.exe"
    [ -n "$PS_BIN" ] && [ "$PS_BIN" = "powershell.exe" ] && \
        echo "  note: using Windows PowerShell 5.1, entry names will use backslashes" >&2
    [ -n "$PS_BIN" ] || { echo "neither zip nor PowerShell is available" >&2; exit 1; }
    "$PS_BIN" -NoProfile -Command "Compress-Archive -Path (Join-Path '$STAGE_WIN' '*') -DestinationPath '$ARCHIVE_WIN' -CompressionLevel Optimal -Force" \
        || { echo "could not create $ARCHIVE" >&2; exit 1; }
fi

SIZE="$(du -h "$ARCHIVE" | cut -f1)"
echo
echo "extras pack: $ARCHIVE ($SIZE)"
echo "contents:"
( cd "$STAGE" && find . -type f | sed 's|^\./|  |' | sort | head -40 )
