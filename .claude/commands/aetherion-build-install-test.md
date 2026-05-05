# Aetherion Build Install Test

Run the canonical full verification workflow when the user wants to rebuild, reinstall, and run the test suite in one shot.

## Command

Run from the repo root:

```bash
make agent-build-install-test
```

## What this does

- Runs the agent-safe end-to-end verification target.
- Expands to `build`, `install`, and `agent-test`.
- Builds the package, reinstalls the newest wheel from `dist/`, and runs `pytest tests` in conda env `aetherion-312` with the `LD_PRELOAD` workaround that prevents pytest 8.4's `_readline_workaround` from segfaulting. Background: `.claude/docs/analysis/2026-05-02-make-test-segfault.md`.

## Workflow

1. Confirm you are in the repo root.
2. Run `make agent-build-install-test` unless the user asked for a narrower step.
3. If it fails, identify whether the failure happened during build, install, or test and summarize that stage first.
4. If it succeeds, report that build, reinstall, and tests all passed.

## Notes

- Use `make agent-build-install-test`, NOT `make build-install-test`. The
  latter chains to `make test`, which exits 139 (SIGSEGV) until the conda
  env's `readline`/`ncurses` pair is reinstalled. Once that's fixed, both
  targets behave identically and this doc should switch back to
  `make build-install-test`.
- Prefer this over separate manual build/install/test invocations when the user asks for broad validation.
- Do not skip the install stage unless the user explicitly asks to avoid reinstalling the wheel.
