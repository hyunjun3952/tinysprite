#!/bin/sh
# Usage: scripts/lint.sh [build-dir]
# Runs clang-tidy on first-party .c files using the build's compile_commands.json.
set -eu
cd "$(dirname "$0")/.."

build=${1:-build}
[ -f "$build/compile_commands.json" ] || cmake -S . -B "$build" >/dev/null

files=$(git ls-files -co --exclude-standard '*.c' ':!:third_party/*')
[ -n "$files" ] || exit 0

# shellcheck disable=SC2086
clang-tidy -p "$build" --quiet $files
