#!/bin/sh
# Usage: scripts/lint.sh [build-dir]
# Runs clang-tidy on first-party .cpp files using the build's compile_commands.json.
# src/ui and tests/ui are only compiled with -DTINYSPRITE_BUILD_UI=ON, so it is skipped (with a note) when the
# build dir does not contain it; any other file missing from the database is an error (stale
# build dir: re-run cmake).
set -eu
cd "$(dirname "$0")/.."

build=${1:-build}
[ -f "$build/compile_commands.json" ] || cmake -S . -B "$build" >/dev/null
db="$build/compile_commands.json"
root=$(pwd -P)

# Positional parameters hold the files to lint, so paths with spaces survive.
set --
missing=0
while IFS= read -r f; do
    if [ ! -f "$f" ]; then
        continue
    fi
    if grep -qF "\"$root/$f\"" "$db"; then
        set -- "$@" "$f"
    else
        case $f in
        src/ui/*|tests/ui/*) echo "lint: skipping $f (not in $db)" >&2 ;;
        *)
            echo "lint: $f is not in $db; re-run cmake" >&2
            missing=1
            ;;
        esac
    fi
done <<EOF
$(git ls-files -co --exclude-standard '*.cpp' ':!:third_party/*')
EOF
[ "$missing" -eq 0 ] || exit 1
[ "$#" -gt 0 ] || exit 0

# clang-tidy prints "N warnings generated." / "[i/n] Processing file" progress noise to stderr even with
# --quiet (the warnings are suppressed ones from system headers). Keep stderr out of the way on success and
# show it unfiltered on failure.
err=$(mktemp)
trap 'rm -f "$err"' EXIT
if ! clang-tidy -p "$build" --quiet "$@" 2>"$err"; then
    cat "$err" >&2
    exit 1
fi
