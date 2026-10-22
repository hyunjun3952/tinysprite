#!/bin/sh
# Fails if the editor settings name a different clangd / clang-format than scripts/clang-version.
set -eu
cd "$(dirname "$0")/.."
v=$(cat scripts/clang-version)
status=0
for f in .vscode/settings.json .devcontainer/devcontainer.json; do
    for tool in clangd clang-format; do
        grep -q "\"${tool}[.a-z]*\": \"${tool}-${v}\"" "$f" || {
            echo "check-clang-version: $f does not use $tool-$v" >&2
            status=1
        }
    done
done
exit "$status"
