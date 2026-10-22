#!/bin/sh
# Fails if the devcontainer settings name a different clangd than scripts/clang-version.
set -eu
cd "$(dirname "$0")/.."
v=$(cat scripts/clang-version)
f=.devcontainer/devcontainer.json
grep -q "\"clangd.path\": \"clangd-${v}\"" "$f" || {
    echo "check-clang-version: $f does not use clangd-$v" >&2
    exit 1
}
