#!/usr/bin/env bash
# Copies Qt's image format plugins and everything they link against into a
# deployment directory.
#
# qimgv relies on Qt plugins for anything that is not one of Qt's built-in
# formats, and a plugin that is missing one shared library fails silently: Qt
# just reports that the file cannot be read. The dependency walk below collects
# the whole closure, so adding a format only means installing the Qt plugin
# package.
#
# usage: scripts/deploy-imageformats.sh [target-dir]
#
# The target defaults to D:/qimgv-test, the directory the test build runs from.
# Run it after copying qimgv.exe there.

set -euo pipefail

TARGET="${1:-D:/qimgv-test}"
QT_BIN="${MINGW_PREFIX:-/d/msys64/mingw64}/bin"
QT_PLUGINS="${MINGW_PREFIX:-/d/msys64/mingw64}/share/qt6/plugins/imageformats"

if [ ! -d "$QT_PLUGINS" ]; then
    echo "no image format plugins at $QT_PLUGINS" >&2
    exit 1
fi

mkdir -p "$TARGET/imageformats"

WINDIR="${WINDIR:-/c/Windows}"

# Windows ships these, so they must not be copied. Anything that resolves to
# System32 and is not part of the MinGW runtime is treated as such.
is_system_dll() {
    [ -f "$QT_BIN/$1" ] && return 1
    [ -f "$WINDIR/System32/$1" ] && return 0
    case "${1,,}" in
        api-ms-win-*.dll|ext-ms-win-*.dll) return 0 ;;
    esac
    return 1
}

# direct, non-system dependencies of a PE image
deps_of() {
    objdump -p "$1" 2>/dev/null \
        | sed -n 's/^[[:space:]]*DLL Name: //p' \
        | tr -d '\r' \
        | while read -r dll; do
            [ -n "$dll" ] || continue
            is_system_dll "$dll" && continue
            printf '%s\n' "$dll"
        done
}

# breadth first over the dependency graph, copying into the target
copy_closure() {
    local queue=("$@")
    local seen=()
    while [ ${#queue[@]} -gt 0 ]; do
        local dll="${queue[0]}"
        queue=("${queue[@]:1}")
        local lower="${dll,,}"
        local known=0
        for s in "${seen[@]:-}"; do
            [ "$s" = "$lower" ] && known=1 && break
        done
        [ $known -eq 1 ] && continue
        seen+=("$lower")

        local src="$QT_BIN/$dll"
        if [ ! -f "$src" ]; then
            # already deployed, or provided by the Qt/runtime set
            if [ -f "$TARGET/$dll" ]; then
                src="$TARGET/$dll"
            else
                echo "  !! $dll not found in $QT_BIN (needed by the chain above)" >&2
                continue
            fi
        fi
        if [ ! -f "$TARGET/$dll" ] || [ "$src" -nt "$TARGET/$dll" ]; then
            cp -f "$src" "$TARGET/$dll"
            echo "  $dll"
        fi
        while read -r dep; do
            [ -n "$dep" ] && queue+=("$dep")
        done < <(deps_of "$src")
    done
}

echo "image format plugins -> $TARGET/imageformats"
for plugin in "$QT_PLUGINS"/*.dll; do
    name="$(basename "$plugin")"
    if [ ! -f "$TARGET/imageformats/$name" ] || [ "$plugin" -nt "$TARGET/imageformats/$name" ]; then
        cp -f "$plugin" "$TARGET/imageformats/$name"
        echo "  $name"
    fi
done

# plugins built from this tree (the APNG reader lives in plugins/imageformats)
BUILT_PLUGINS="$(dirname "$0")/../build6/plugins/imageformats"
if [ -d "$BUILT_PLUGINS" ]; then
    for built in "$BUILT_PLUGINS"/*/*.dll; do
        [ -f "$built" ] || continue
        name="$(basename "$built")"
        if [ ! -f "$TARGET/imageformats/$name" ] || [ "$built" -nt "$TARGET/imageformats/$name" ]; then
            cp -f "$built" "$TARGET/imageformats/$name"
            echo "  $name (built)"
        fi
    done
fi

echo "dependencies"
for plugin in "$QT_PLUGINS"/*.dll; do
    while read -r dep; do
        [ -n "$dep" ] || continue
        copy_closure "$dep"
    done < <(deps_of "$plugin")
done

# libraries the executable links directly (libheif and the codecs behind it)
if [ -f "$TARGET/qimgv.exe" ]; then
    echo "executable dependencies"
    while read -r dep; do
        [ -n "$dep" ] || continue
        copy_closure "$dep"
    done < <(deps_of "$TARGET/qimgv.exe")
fi

echo "done"
