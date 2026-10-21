#!/bin/sh
# Points git at the tracked hooks in .githooks/.
set -eu
cd "$(dirname "$0")/.."
git config core.hooksPath .githooks
echo "git hooks enabled (.githooks)"
