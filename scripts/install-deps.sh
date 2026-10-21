#!/bin/sh
# Installs the development packages (Debian/Ubuntu). Shared by CI and the devcontainer.
set -eu

if [ "$(id -u)" -eq 0 ]; then
    sudo=
else
    sudo=sudo
fi

# $sudo stays unquoted so that an empty value disappears.
# shellcheck disable=SC2086
$sudo apt-get update
# shellcheck disable=SC2086
$sudo apt-get install -y --no-install-recommends \
    build-essential cmake git clang-format clang-tidy shellcheck libsdl2-dev
