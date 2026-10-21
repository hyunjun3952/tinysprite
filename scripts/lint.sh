#!/bin/sh
# Usage: scripts/lint.sh [build-dir]
# Runs clang-tidy on first-party .c files using the build's compile_commands.json.
set -eu
cd "$(dirname "$0")/.."

build=${1:-build}
[ -f "$build/compile_commands.json" ] || cmake -S . -B "$build" >/dev/null

files=$(git ls-files -co --exclude-standard '*.c' ':!:third_party/*')
# Skip files the build does not compile (e.g. src/ui when TINYSPRITE_BUILD_UI is off).
listed=
for f in $files; do
    grep -q "\"$PWD/$f\"" "$build/compile_commands.json" && listed="$listed $f"
done
files=$listed
[ -n "$files" ] || exit 0

# shellcheck disable=SC2086
clang-tidy -p "$build" --quiet $files
