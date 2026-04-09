# Repo Mapping

This skill is scoped to the current Aetherion repository only.

## Defaults

- PR base branch: `main`
- Branch pattern: `{type}/{issue_id}-{topic}`
- Commit pattern: `{scope}: {message} (refs #{issue_id})`

## Repository

- GitHub repo: `arthurmoreno/aetherion`
- Use for engine, voxel, rendering, physics, bindings, and repository workflow changes in this repo
- Expected work types: `feature`, `bug`, `chore`
- Branch prefixes:
  - `feature` -> `feature`
  - `bug` -> `fix`
  - `chore` -> `chore`
- Checks:
  - Build: `make build`
  - Test: `make test`
  - Full verification: `make build-install-test`
