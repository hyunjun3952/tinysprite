#!/bin/sh
# Usage: scripts/format.sh [--check]
# Formats (or with --check, verifies) all first-party C++ sources.
set -eu
cd "$(dirname "$0")/.."

usage() {
    echo "usage: scripts/format.sh [--check]" >&2
    exit 2
}

mode=format
case ${1:-} in
"") ;;
--check) mode=check ;;
*) usage ;;
esac
[ "$#" -le 1 ] || usage

# Positional parameters hold the files, so paths with spaces survive. Files deleted from the
# working tree but still in the index are skipped.
set --
while IFS= read -r f; do
    [ -f "$f" ] && set -- "$@" "$f"
done <<EOF
$(git ls-files -co --exclude-standard '*.h' '*.cpp' ':!:thirdparty/*')
EOF
[ "$#" -gt 0 ] || exit 0

if [ "$mode" = check ]; then
    clang-format --dry-run --Werror "$@"
else
    clang-format -i "$@"
fi
