#!/bin/sh
# Fails if the editor settings name a different clangd than scripts/clang-version.
set -eu
cd "$(dirname "$0")/.."
v=$(cat scripts/clang-version)
status=0
for f in .vscode/settings.json .devcontainer/devcontainer.json; do
    grep -q "\"clangd.path\": \"clangd-${v}\"" "$f" || {
        echo "check-clang-version: $f does not use clangd-$v" >&2
        status=1
    }
done
exit "$status"
