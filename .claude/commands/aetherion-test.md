# Aetherion Test

Run the test suite when the user wants to verify the current installed package without rebuilding.

## Command

Run from the repo root:

```bash
make agent-test
```

## What this does

- Runs `pytest tests` inside conda env `aetherion-312`, with the
  `LD_PRELOAD` workaround that prevents pytest 8.4's
  `_readline_workaround` from segfaulting on the env's broken `readline`
  extension. Background:
  `.claude/docs/analysis/2026-05-02-make-test-segfault.md`.
- Tests the currently installed wheel — does NOT rebuild or reinstall.

## Workflow

1. Confirm you are in the repo root.
2. Run `make agent-test`.
3. If tests fail, report the failing test name, the first traceback, and the relevant assertion message.
4. If tests pass, report the count and confirm everything is green.

## Running a single test or file in isolation

When debugging a specific failure or iterating on one test, use the
focused variant instead of the full suite:

```bash
# Whole file
make agent-test-one TEST=tests/world/test_world_manager_lifecycle.py

# Single test (node-id form)
make agent-test-one TEST=tests/world/test_world_manager_lifecycle.py::test_update_only_runs_when_status_running

# Directory + extra pytest flags
make agent-test-one TEST=tests/world PYTEST_ARGS="-k status"
```

`agent-test-one` adds `-x -v -p no:cacheprovider` for tight focused runs and
keeps the same `LD_PRELOAD` workaround as `agent-test`. It errors out (exit
2) if `TEST=` is missing.

## Notes

- Use `make agent-test`, NOT `make test`. `make test` exits 139 (SIGSEGV)
  under `conda run` until the conda env's `readline`/`ncurses` pair is
  reinstalled. Once that's fixed, both targets behave identically and this
  doc should switch back to `make test`.
- Pass extra pytest flags via `PYTEST_ARGS`, e.g.
  `make agent-test PYTEST_ARGS="-x tests/world"` or
  `make agent-test-one TEST=tests/world PYTEST_ARGS="-k status"`.
- Use this when the package is already installed and you only want a test pass.
- Use `/aetherion-build-install-test` instead when you also need to rebuild and reinstall first.
- Do NOT run `pytest` directly or use `conda run` manually — always go through `make agent-test` (or `make agent-test-one`) so the environment, flags, and workaround stay aligned with the Makefile.
