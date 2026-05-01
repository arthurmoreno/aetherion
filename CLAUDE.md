# Aetherion — Claude Code Context

C++/Python game engine. Provides the foundation and Python bindings for the `lifesim` simulation layer.

## Key Directories

- `src/` — C++ source code
- `include/` — C++ headers
- `bindings/` — Python bindings (nanobind)
- `tests/` — Python test suite (pytest)
- `.claude/commands/` — Custom slash commands / agent skills
- `.claude/docs/` — Architecture and planning documents for Claude

## Environment

Conda env: `aetherion-312`

The Makefile selects this automatically for all `make` targets. Only activate it manually if running commands outside of make.

## Commands

| Task | Command |
|------|---------|
| Build package | `make build` |
| Build + reinstall + test | `make build-install-test` |
| Run tests only | `make test` |
| Run tests with coverage | `make coverage` |
| Format (C++ + Python) | `make format` |
| Check C++ formatting | `make clang-format-check` |
| C++ unit tests | `make cpp-tests` |

## Code Style

**C++ (src/, include/):**
- Classes, Structs, Enums: `PascalCase`
- Public methods: `PascalCase`
- Free functions, accessors, mutators: `snake_case`
- Variables: `snake_case`
- Constants: `UPPER_CASE`
- Members: `m_variableName`

**C++ (bindings/):**
- Internal implementation: follow C++ core conventions
- Names exposed to Python via nanobind: follow Python/PEP 8 conventions

**Python:** `PascalCase` classes, `snake_case` functions and variables, `UPPER_CASE` constants

Formatter: `clang-format` (Google Style) for C++, `ruff` for Python.

## Engineering Priorities

- Performance is the top concern — this is the engine layer
- Use smart pointers (`unique_ptr`, `shared_ptr`) to avoid leaks
- Keep Python bindings minimal: expose only what `lifesim` needs
- Always translate C++ exceptions to Python exceptions in bindings

## GitHub Workflow

- Repo: `arthurmoreno/aetherion`
- Base branch: `main`
- Branch format: `feature#<id>-<topic>` / `bug#<id>-<topic>` / `chore/<id>-<topic>`
- Commit format: `[feature#<id>] Summary.` / `[bug#<id>] Summary.` / `[chore#<id>] Summary.`
- PR title = commit title

Use `/github-staged-publish` to commit staged changes and open a PR.
Use `/github-project-mcp` for issue/PR management.

## Planning

All technical plans live in `.claude/docs/epics-plans/`. Follow the structure defined in
`.claude/docs/PLANNING_RULES.md` — required sections are: metadata, executive summary,
task tracking table (with status column), and known blockers.

## Available Skills

- `/aetherion-build` — `make build`
- `/aetherion-build-install-test` — full build + reinstall + pytest
- `/aetherion-format` — format all C++ and Python sources
- `/github-project-mcp` — GitHub issue and PR workflow
- `/github-staged-publish` — commit staged changes → push → open PR
