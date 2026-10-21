#!/bin/sh
# Usage: scripts/lint.sh [build-dir]   (default: build)
# Runs clang-tidy on first-party .cpp files using the build's compile_commands.json.
# src/ui and tests/ui are only compiled when the UI is built (SDL2 found), so they are skipped
# (with a note) when the build dir does not contain them; any other file missing from the
# database is an error (stale build dir: re-run cmake).
set -eu
cd "$(dirname "$0")/.."

case ${1:-} in
-*)
    echo "usage: scripts/lint.sh [build-dir]" >&2
    exit 2
    ;;
esac
[ "$#" -le 1 ] || {
    echo "usage: scripts/lint.sh [build-dir]" >&2
    exit 2
}

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
$(git ls-files -co --exclude-standard '*.cpp' ':!:thirdparty/*')
EOF
[ "$missing" -eq 0 ] || exit 1
[ "$#" -gt 0 ] || exit 0

# Files that passed with the same contents, headers, .clang-tidy and clang-tidy version are skipped
# (stamps live in "$build/.lint-cache"; LINT_NO_CACHE=1 ignores and does not update them). The rest run
# in parallel (LINT_JOBS overrides the CPU count).
sha() {
    if command -v sha256sum >/dev/null 2>&1; then sha256sum; else shasum -a 256; fi
}
cache="$build/.lint-cache"
mkdir -p "$cache"
# A header change can affect any file, so it invalidates every stamp; a .cpp change only its own.
key=$(
    {
        clang-tidy --version
        cat .clang-tidy
        git ls-files -co --exclude-standard 'include/*.h' 'src/*.h' 'tests/*.h' |
            while IFS= read -r h; do [ -f "$h" ] && cat "$h"; done
    } | sha | cut -d' ' -f1
)
todo=$(mktemp)
err=$(mktemp -d)
trap 'rm -rf "$todo" "$err"' EXIT
for f in "$@"; do
    stamp="$cache/$(printf '%s\n' "$key" | cat - "$f" | sha | cut -d' ' -f1)"
    if [ -n "${LINT_NO_CACHE:-}" ] || [ ! -f "$stamp" ]; then
        printf '%s\0%s\0' "$f" "$stamp" >>"$todo"
    fi
done
[ -s "$todo" ] || exit 0

# clang-tidy prints "N warnings generated." / "[i/n] Processing file" progress noise to stderr even with
# --quiet (the warnings are suppressed ones from system headers). Keep stderr out of the way on success and
# show it unfiltered on failure.
jobs=${LINT_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)}
export build err LINT_NO_CACHE
# The worker script is single-quoted on purpose: its variables expand in the child shell.
# shellcheck disable=SC2016
if ! xargs -0 -n 2 -P "$jobs" sh -c '
    out="$err/$(basename "$1").log"
    if clang-tidy -p "$build" --quiet "$1" 2>"$out"; then
        [ -n "${LINT_NO_CACHE:-}" ] || : >"$2"
    else
        { echo "== $1"; cat "$out"; } >&2
        exit 1
    fi
' sh <"$todo"; then
    exit 1
fi
