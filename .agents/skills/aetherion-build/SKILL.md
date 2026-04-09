---
name: aetherion-build
description: Run the canonical `make build` workflow for the Aetherion engine when the user wants to build the package, refresh wheel artifacts, or verify that the project still compiles through the standard Makefile target.
---

# Aetherion Build

Use this skill when the user wants the standard package build for this repository.

## Command

Run from the repo root:

```bash
make build
```

## What this does

- Executes the repo's canonical build target.
- Uses `conda run --no-capture-output -n aetherion-312 python -m build` through the Makefile.
- Produces wheel and sdist artifacts in `dist/`.

## Workflow

1. Confirm you are in the repo root.
2. If the request is specifically to build, run `make build` directly.
3. If the build fails, report the first actionable failure with the relevant file or tool.
4. If the build succeeds, mention that artifacts were refreshed in `dist/`.

## Notes

- Do not replace this with ad hoc build commands unless the user explicitly asks.
- The Makefile already selects the correct conda environment, so a separate `conda activate` step is usually unnecessary for this workflow.
