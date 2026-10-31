#!/usr/bin/env bash
# Shared helpers for the two deployment scripts:
#
#   scripts/deploy-app.sh      - the program itself (Qt runtime + basic formats)
#   scripts/deploy-formats.sh  - the optional pack (extra formats and video)
#
# Both walk PE imports with objdump, because a missing DLL only shows up much
# later as "this file cannot be read".

set -uo pipefail

QT_BIN="${MINGW_PREFIX:-/d/msys64/mingw64}/bin"
QT_PLUGINS="${MINGW_PREFIX:-/d/msys64/mingw64}/share/qt6/plugins"
WINDIR="${WINDIR:-/c/Windows}"

# Windows ships these (and the MinGW runtime is copied from QT_BIN instead).
is_system_dll() {
    [ -f "$QT_BIN/$1" ] && return 1
    [ -f "$WINDIR/System32/$1" ] && return 0
    case "${1,,}" in
        api-ms-win-*.dll|ext-ms-win-*.dll) return 0 ;;
        msvcp*.dll|vcruntime*.dll|ucrtbase.dll) return 0 ;;
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

# breadth first over the dependency graph, copying into $TARGET
copy_closure() {
    local target="$1"
    shift
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
            if [ -f "$target/$dll" ]; then
                src="$target/$dll"
            else
                echo "  !! $dll not found in $QT_BIN" >&2
                continue
            fi
        fi
        if [ ! -f "$target/$dll" ] || [ "$src" -nt "$target/$dll" ]; then
            cp -f "$src" "$target/$dll"
            echo "  $dll"
        fi
        while read -r dep; do
            [ -n "$dep" ] && queue+=("$dep")
        done < <(deps_of "$src")
    done
}

# deploy_closure_of <target> <file>... : dependencies of the given local files
deploy_closure_of() {
    local target="$1"
    shift
    local file
    for file in "$@"; do
        [ -f "$file" ] || continue
        local deps=()
        while read -r dep; do
            [ -n "$dep" ] && deps+=("$dep")
        done < <(deps_of "$file")
        [ ${#deps[@]} -gt 0 ] && copy_closure "$target" "${deps[@]}"
    done
}
