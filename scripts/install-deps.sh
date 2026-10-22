#!/bin/sh
# Installs the development packages (Debian/Ubuntu). Shared by CI and the devcontainer.
set -eu

if [ "$(id -u)" -eq 0 ]; then
    sudo=
else
    sudo=sudo
fi

# The clang tools are pinned to scripts/clang-version (what CI uses), so a local clang-tidy newer
# than CI's cannot pass what CI then rejects.
clang=$(cat "$(dirname "$0")/clang-version")

# $sudo stays unquoted so that an empty value disappears.
# shellcheck disable=SC2086
$sudo apt-get update
# shellcheck disable=SC2086
$sudo apt-get install -y --no-install-recommends \
    build-essential cmake ccache git "clang-format-$clang" "clang-tidy-$clang" shellcheck libsdl2-dev
