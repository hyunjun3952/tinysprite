#!/bin/sh
# Points git at the tracked hooks in .githooks/.
set -eu
cd "$(dirname "$0")/.."
git rev-parse --git-dir >/dev/null 2>&1 || {
    echo "setup-hooks: not inside a git repository" >&2
    exit 1
}
git config core.hooksPath .githooks
echo "git hooks enabled (.githooks)"
