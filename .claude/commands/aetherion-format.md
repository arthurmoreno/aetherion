# Aetherion Format

Run the canonical formatting workflow when the user wants repo-standard formatting for both C++ and Python code before review, testing, or committing.

## Command

Run from the repo root:

```bash
make format
```

## What this does

- Runs the repo's canonical formatting target.
- Applies `clang-format` to supported files under `src/` and `webclient/wasm`.
- Applies `ruff format .` and `ruff check --fix .` inside conda env `aetherion-312`.

## Workflow

1. Confirm you are in the repo root.
2. Run `make format`.
3. Review the changed files before any follow-up build or test step if the user asked for more than formatting.
4. If formatting fails, report the blocking tool and message.

## Notes

- Prefer this target over invoking `clang-format` or `ruff` directly so formatting stays aligned with the Makefile.
- Expect source edits; check `git diff --stat` or targeted diffs afterward when a summary is useful.
