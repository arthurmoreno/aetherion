---
name: aetherion-build-install-test
description: Run the canonical `make build-install-test` workflow for the Aetherion engine when the user wants a full local verification pass that rebuilds the package, reinstalls the latest wheel, and runs the Python test suite.
---

# Aetherion Build Install Test

Use this skill when the user wants the full local verification path for this repository.

## Command

Run from the repo root:

```bash
make build-install-test
```

## What this does

- Runs the Makefile's canonical end-to-end verification target.
- Expands to `build`, `install`, and `test`.
- Builds the package, reinstalls the newest wheel from `dist/`, and runs `pytest tests` in conda env `aetherion-312`.

## Workflow

1. Confirm you are in the repo root.
2. Run `make build-install-test` unless the user asked for a narrower step.
3. If it fails, identify whether the failure happened during build, install, or test and summarize that stage first.
4. If it succeeds, report that build, reinstall, and tests all passed.

## Notes

- Prefer this skill over separate manual build/install/test invocations when the user asks for broad validation.
- Do not skip the install stage unless the user explicitly asks to avoid reinstalling the wheel.
