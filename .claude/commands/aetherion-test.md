# Aetherion Test

Run the test suite when the user wants to verify the current installed package without rebuilding.

## Command

Run from the repo root:

```bash
make test
```

## What this does

- Runs `pytest tests` inside conda env `aetherion-312`.
- Tests the currently installed wheel — does NOT rebuild or reinstall.

## Workflow

1. Confirm you are in the repo root.
2. Run `make test`.
3. If tests fail, report the failing test name, the first traceback, and the relevant assertion message.
4. If tests pass, report the count and confirm everything is green.

## Notes

- Use this when the package is already installed and you only want a test pass.
- Use `/aetherion-build-install-test` instead when you also need to rebuild and reinstall first.
- Do NOT run `pytest` directly or use `conda run` manually — always go through `make test` so the environment and flags stay aligned with the Makefile.
