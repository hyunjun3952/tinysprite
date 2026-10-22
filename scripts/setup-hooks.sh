#!/bin/sh
# Installs the git hooks configured in .pre-commit-config.yaml (pre-commit and pre-push).
set -eu
cd "$(dirname "$0")/.."
git rev-parse --git-dir >/dev/null 2>&1 || {
    echo "setup-hooks: not inside a git repository" >&2
    exit 1
}
if ! command -v pre-commit >/dev/null 2>&1; then
    echo "setup-hooks: installing pre-commit (pip --user)"
    python3 -m pip install --user pre-commit || {
        echo "setup-hooks: install it yourself: https://pre-commit.com/#install" >&2
        exit 1
    }
fi
# the old .githooks setup pointed core.hooksPath away, which pre-commit refuses to install over
git config --local --unset core.hooksPath 2>/dev/null || true
pre-commit install
echo "git hooks enabled (.pre-commit-config.yaml)"
