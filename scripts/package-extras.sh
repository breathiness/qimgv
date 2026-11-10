#!/usr/bin/env bash
# Builds the optional archives that go with a release:
#
#   qimgv-formats-<version>.zip   imageformats/ + pdf/
#     imageformats/  extra image formats (WebP/TIFF/TGA/ICNS/WBMP/MNG/JP2,
#                    HEIC/AVIF/JXL/RAW/PSD from the plugins in this tree)
#     pdf/           pdfprovider.dll and the poppler libraries it needs
#
#   qimgv-video-<version>.zip     plugins/ + libmpv-2.dll
#     plugins/       player_mpv.dll
#     libmpv-2.dll   the player itself (~80 MB unpacked, which is why it is
#                    a separate download from the formats)
#
# Everything in them is optional: qimgv starts and works without either archive,
# and a missing piece only removes what it provides. Unzipping an archive into the
# program folder is the whole installation - there is nothing to register.
#
# usage: scripts/package-extras.sh [output-dir] [app-dir]
#
# app-dir is what the archives are cut against: every file it already contains is
# left out, so a pack only carries what the program package does not ship. It must
# therefore be a CLEAN install of the release program package - unzip
# qimgv-<version>-win64.zip into a temporary folder and pass that.
#
# Do not pass a working installation that already had the packs applied: its
# libmpv-2.dll, libheif/jxl/raw, libwebp/tiff and friends would all be treated as
# "already there" and dropped, leaving an archive with plugins that have no
# libraries. That pack installs without an error and then quietly offers no video,
# no HEIC and no WebP. Without app-dir nothing is dropped, which is always correct
# but makes the archives ~60 MB larger together.
#
# The script refuses an app-dir that looks like it already has the extras; set
# QIMGV_PACK_FROM_POPULATED=1 to build the delta anyway (for example to update an
# archive for users who already installed a previous one).
#
# Set QIMGV_PACK_KIND=formats or QIMGV_PACK_KIND=video to build just one of them.

set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
# the most recent build directory that exists, so the script still works after an
# older one has been cleaned up
if [ -z "${QIMGV_BUILD:-}" ]; then
    for candidate in build9 build8 build7 build6; do
        [ -f "$REPO/$candidate/qimgv/qimgv.exe" ] && { BUILD="$REPO/$candidate"; break; }
    done
fi
BUILD="${BUILD:-${QIMGV_BUILD:-$REPO/build8}}"
OUT="${1:-$REPO/dist}"
APP_DIR="${2:-}"
KIND="${QIMGV_PACK_KIND:-all}"

# shellcheck source=deploy-lib.sh
. "$HERE/deploy-lib.sh"

VERSION="$(sed -n 's/^ *VERSION \([0-9.]*\)$/\1/p' "$REPO/CMakeLists.txt" | head -1)"
[ -n "$VERSION" ] || VERSION="0.0.0"

STAGE_ROOT="$BUILD/extras-pack"
STAGE_FORMATS="$STAGE_ROOT/formats"
STAGE_VIDEO="$STAGE_ROOT/video"

if [ ! -f "$BUILD/qimgv/qimgv.exe" ]; then
    echo "no build in $BUILD - run ninja first" >&2
    exit 1
fi

if [ -n "$APP_DIR" ] && [ -d "$APP_DIR" ]; then
    # A program folder that already carries the extras is the wrong reference: its
    # libraries would be dropped and the archives would ship plugins without them.
    pop=()
    [ -f "$APP_DIR/libmpv-2.dll" ] && pop+=("libmpv-2.dll")
    [ -f "$APP_DIR/pdf/pdfprovider.dll" ] && pop+=("pdf/")
    [ -f "$APP_DIR/plugins/player_mpv.dll" ] && pop+=("plugins/")
    [ -f "$APP_DIR/imageformats/qwebp.dll" ] && pop+=("imageformats/")
    if [ ${#pop[@]} -gt 0 ] && [ "${QIMGV_PACK_FROM_POPULATED:-0}" != "1" ]; then
        echo "error: $APP_DIR already contains the extras (${pop[*]})." >&2
        echo "       Cut the archives against a clean unzip of the program package" >&2
        echo "       instead, or set QIMGV_PACK_FROM_POPULATED=1 to mean it." >&2
        exit 1
    fi
fi

emitted=()

# drop_already_present <stage>: remove anything the program package already ships
drop_already_present() {
    local stage="$1"
    [ -n "$APP_DIR" ] && [ -d "$APP_DIR" ] || return 0
    [ "${QIMGV_PACK_FROM_POPULATED:-0}" = "1" ] && return 0
    local dropped=0 rel base
    while IFS= read -r f; do
        rel="${f#"$stage"/}"
        base="$(basename "$f")"
        if [ -f "$APP_DIR/$rel" ] || [ -f "$APP_DIR/$base" ]; then
            rm -f "$f"
            dropped=$((dropped + 1))
        fi
    done < <(find "$stage" -type f -name '*.dll')
    echo "  $dropped dll(s) shared with the program package"
}

# make_archive <stage> <name> : zip a stage directory
make_archive() {
    local stage="$1" name="$2" archive
    mkdir -p "$OUT"
    archive="$OUT/$name.zip"
    rm -f "$archive"
    if command -v zip >/dev/null 2>&1; then
        ( cd "$stage" && zip -r -q -9 "$archive" . )
    else
        # msys2 without the zip package: compress with PowerShell. pwsh (7) writes
        # entry names with forward slashes like zip does; Windows PowerShell 5.1
        # writes backslashes, which other platforms cannot read.
        local archive_win stage_win ps_bin=""
        archive_win="$(cygpath -w "$archive")"
        stage_win="$(cygpath -w "$stage")"
        command -v pwsh >/dev/null 2>&1 && ps_bin="pwsh"
        [ -z "$ps_bin" ] && command -v powershell.exe >/dev/null 2>&1 && ps_bin="powershell.exe"
        [ -n "$ps_bin" ] && [ "$ps_bin" = "powershell.exe" ] && \
            echo "  note: using Windows PowerShell 5.1, entry names will use backslashes" >&2
        [ -n "$ps_bin" ] || { echo "neither zip nor PowerShell is available" >&2; exit 1; }
        "$ps_bin" -NoProfile -Command "Compress-Archive -Path (Join-Path '$stage_win' '*') -DestinationPath '$archive_win' -CompressionLevel Optimal -Force" \
            || { echo "could not create $archive" >&2; exit 1; }
    fi
    emitted+=("$archive")
    echo
    echo "pack: $archive ($(du -h "$archive" | cut -f1))"
    echo "contents:"
    ( cd "$stage" && find . -type f | sed 's|^\./|  |' | sort )
}

build_formats() {
    rm -rf "$STAGE_FORMATS"
    mkdir -p "$STAGE_FORMATS/imageformats" "$STAGE_FORMATS/pdf"

    echo "image formats"
    local plugin src built
    for plugin in qwebp qtiff qtga qicns qwbmp qmng qjp2; do
        src="$QT_PLUGINS/imageformats/$plugin.dll"
        [ -f "$src" ] || continue
        cp -f "$src" "$STAGE_FORMATS/imageformats/" && echo "  $plugin.dll"
    done
    for built in "$BUILD"/plugins/imageformats/*/*.dll; do
        [ -f "$built" ] || continue
        cp -f "$built" "$STAGE_FORMATS/imageformats/" && echo "  $(basename "$built") (built)"
    done

    echo "3D models"
    # qimgv loads assimp itself at runtime (LoadLibraryW "libassimp-6.dll"), so
    # the dll has to sit next to qimgv.exe, not in imageformats/. The prefix is
    # taken from the build that is being packaged (ASSIMP_ROOT in its
    # CMakeCache.txt), never from a hard-coded path; the ASSIMP_ROOT environment
    # variable overrides it for a build tree that was configured elsewhere.
    local assimp_prefix="${ASSIMP_ROOT:-}" candidate
    if [ -z "$assimp_prefix" ] && [ -f "$BUILD/CMakeCache.txt" ]; then
        assimp_prefix="$(sed -n 's/^ASSIMP_ROOT:PATH=//p' "$BUILD/CMakeCache.txt" | tr -d '\r' | head -1)"
    fi
    if [ -n "$assimp_prefix" ]; then
        for candidate in "$assimp_prefix/bin/libassimp-6.dll" "$assimp_prefix/lib/libassimp-6.dll"; do
            if [ -f "$candidate" ]; then
                cp -f "$candidate" "$STAGE_FORMATS/" && echo "  libassimp-6.dll"
                break
            fi
        done
    fi
    if [ ! -f "$STAGE_FORMATS/libassimp-6.dll" ]; then
        # A build configured with assimp headers (USE_ASSIMP) has working 3D
        # support, so a pack without the dll would advertise formats that cannot
        # open: that is a release blocker, not a warning.
        if [ -n "$(sed -n 's/^ASSIMP_INCLUDE_DIR:PATH=//p' "$BUILD/CMakeCache.txt" 2>/dev/null | tr -d '\r' | head -1)" ]; then
            echo "error: this build has 3D support but libassimp-6.dll was not found;" >&2
            echo "       refusing to ship a formats pack whose 3D formats cannot open." >&2
            echo "       Set ASSIMP_ROOT to the prefix that has bin/libassimp-6.dll." >&2
            exit 1
        fi
        echo "  note: build has no 3D support (ASSIMP_INCLUDE_DIR not set), skipping assimp" >&2
    fi

    echo "pdf"
    if [ -f "$BUILD/pdf/pdfprovider.dll" ]; then
        cp -f "$BUILD/pdf/pdfprovider.dll" "$STAGE_FORMATS/pdf/" && echo "  pdf/pdfprovider.dll"
    else
        echo "  !! pdfprovider.dll was not built (poppler-qt6 missing?)" >&2
    fi

    echo "dependencies (formats)"
    deploy_closure_of "$STAGE_FORMATS" "$STAGE_FORMATS"/imageformats/*.dll
    # assimp is loaded by the executable itself, so whatever it imports belongs
    # in the stage root next to qimgv.exe as well
    deploy_closure_of "$STAGE_FORMATS" "$STAGE_FORMATS"/libassimp-6.dll
    deploy_closure_of "$STAGE_FORMATS/pdf" "$STAGE_FORMATS/pdf"/pdfprovider.dll

    cat > "$STAGE_FORMATS/README.txt" <<EOF
qimgv $VERSION - extra image formats
===================================

Unzip this archive into the folder that holds qimgv.exe, keeping the folder names.
That is the whole installation; nothing needs to be registered.

  imageformats/   WebP, TIFF, TGA, ICNS, WBMP, MNG, JPEG 2000 and, when built,
                  HEIC/HEIF, AVIF, JPEG XL, camera raw, PSD, APNG
  pdf/            PDF documents, rendered through poppler
  libassimp-6.dll 3D models (STL, OBJ, PLY, glTF/GLB, FBX, 3DS, 3MF, COLLADA),
                  loaded by qimgv itself when one of those files is opened

Everything here is optional. qimgv starts without it, and a missing folder only
means the formats it provides are not listed; delete a folder to disable it again.

Video playback is a separate download (qimgv-video-$VERSION.zip), because the
player library is much larger than all of these formats together.

Licences: the extra Qt image plugins follow Qt's licence, poppler is
GPL-2.0-or-later, assimp is BSD-3-Clause. qimgv itself is GPLv3; the full texts
are unchanged by this archive.
EOF

    drop_already_present "$STAGE_FORMATS"
    make_archive "$STAGE_FORMATS" "qimgv-formats-$VERSION"
}

build_video() {
    rm -rf "$STAGE_VIDEO"
    mkdir -p "$STAGE_VIDEO/plugins"

    echo "video"
    if [ -f "$BUILD/plugins/player_mpv/player_mpv.dll" ]; then
        cp -f "$BUILD/plugins/player_mpv/player_mpv.dll" "$STAGE_VIDEO/plugins/" && echo "  plugins/player_mpv.dll"
    else
        echo "  !! player_mpv.dll was not built (libmpv missing?)" >&2
    fi
    local mpv_kit="${MPV_DIR:-$REPO/3rdparty/mpv}" candidate
    for candidate in "$mpv_kit/bin/libmpv-2.dll" "$mpv_kit/lib/x86_64/libmpv-2.dll"; do
        if [ -f "$candidate" ]; then
            cp -f "$candidate" "$STAGE_VIDEO/" && echo "  libmpv-2.dll"
            break
        fi
    done

    echo "dependencies (video)"
    deploy_closure_of "$STAGE_VIDEO" "$STAGE_VIDEO"/plugins/*.dll
    deploy_closure_of "$STAGE_VIDEO" "$STAGE_VIDEO"/libmpv-2.dll

    cat > "$STAGE_VIDEO/README.txt" <<EOF
qimgv $VERSION - video playback
===============================

Unzip this archive into the folder that holds qimgv.exe, keeping the folder names.
That is the whole installation; nothing needs to be registered.

  plugins/player_mpv.dll   the mpv player plugin
  libmpv-2.dll             the player library

Without this archive qimgv still works: video files are simply not listed, and
the thumbnail strip shows no frames for them. Delete plugins/ to disable it again.

The extra image formats and PDF support are a separate download
(qimgv-formats-$VERSION.zip).

Licences: mpv is LGPL-2.1-or-later with parts under GPL. qimgv itself is GPLv3;
the full texts are unchanged by this archive.
EOF

    drop_already_present "$STAGE_VIDEO"
    make_archive "$STAGE_VIDEO" "qimgv-video-$VERSION"
}

case "$KIND" in
    formats) build_formats ;;
    video)   build_video ;;
    all)     build_formats; build_video ;;
    *) echo "QIMGV_PACK_KIND must be formats, video or all (got '$KIND')" >&2; exit 2 ;;
esac

[ -n "$APP_DIR" ] && [ -d "$APP_DIR" ] || \
    echo "note: no program folder given, the archives carry every dependency" >&2
