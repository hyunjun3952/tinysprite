#!/bin/sh
# Usage: scripts/format.sh [--check]
# Formats (or with --check, verifies) all first-party C sources.
set -eu
cd "$(dirname "$0")/.."

files=$(git ls-files -co --exclude-standard '*.c' '*.h' ':!:third_party/*')
[ -n "$files" ] || exit 0

if [ "${1:-}" = "--check" ]; then
    # shellcheck disable=SC2086
    clang-format --dry-run --Werror $files
else
    # shellcheck disable=SC2086
    clang-format -i $files
fi
