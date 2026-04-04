#!/usr/bin/env bash
# Runs aetherion checks for git pre-commit when aetherion/ paths are staged.
# Install: from repo root, `pre-commit install`. Skip once: SKIP=aetherion-pre-commit git commit ...

set -euo pipefail

root="$(git rev-parse --show-toplevel)"
cd "$root"

if ! git diff --cached --name-only --diff-filter=ACM | grep -q '^aetherion/'; then
	exit 0
fi

cd "$root/aetherion"
make test
make clang-format-check
make python-format

cd "$root"
if ! git diff --exit-code -- aetherion/; then
	echo >&2
	echo "pre-commit: aetherion/ has unstaged changes after formatters. Stage them and commit again:" >&2
	git diff --name-only -- aetherion/ >&2
	exit 1
fi
