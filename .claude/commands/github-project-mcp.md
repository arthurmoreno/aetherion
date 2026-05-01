# GitHub Project MCP

Use this skill for GitHub issue and pull request work tied to `arthurmoreno/aetherion`.

## Execution Surface

- Prefer the built-in `GitHub` plugin and its MCP/app tools for issue, PR, comment, and metadata operations.
- If the required GitHub connector access is unavailable, stop and report the blocker before using any fallback.

## Workflow

1. Determine the work type: `bug`, `feature`, or `chore`.
2. Consult the repo mapping and local checks below.
3. Consult the naming and execution rules below.
4. Search for an existing issue or PR before creating a new one.
5. Keep issue, branch, commit, and PR naming aligned with the naming rules.
6. If code changes are involved, run the mapped checks before commit or PR creation.
7. Use the PR template below for PR bodies unless the user asks for a different format.

## Rules

- Use issue-first workflow by default for net-new implementation work.
- Always include the issue reference in branch, commit, and PR title when an issue exists.
- Do not include secrets in issues, PRs, comments, or commits.
- If lint or test commands are missing or invalid, stop and report that the mapping needs an update.

---

## Repo Mapping

- GitHub repo: `arthurmoreno/aetherion`
- PR base branch: `main`
- Expected work types: `feature`, `bug`, `chore`
- Branch prefixes: `feature` → `feature`, `bug` → `fix`, `chore` → `chore`
- Checks:
  - Build: `make build`
  - Test: `make test`
  - Full verification: `make build-install-test`

---

## Naming Rules

**Branch format:** `<type>/<issue-id>-<short-kebab-topic>`

**Commit title:** include issue reference — `[{type}#{issue_id}] {summary}`

**PR title:** same as commit title

Examples:
- Branch: `feature/123-add-world-seed-option`
- Commit: `[feature#123] Add deterministic seed loading.`
- PR: `[feature#123] Add deterministic seed loading.`

## Process

1. Identify `bug`, `feature`, or `chore`.
2. Search for duplicates before opening a new issue.
3. Confirm the work belongs in `arthurmoreno/aetherion`.
4. Run the mapped checks before commit or PR creation when the user asks for verification.
5. If checks fail, stop and report instead of silently bypassing them.

## End-Of-Run Report

- Issue URL
- Target repository
- Branch name
- Build result
- Test result
- Commit SHA if created
- Push status
- PR URL if created
- Next action if blocked

---

## PR Template

```
## Summary

- What changed and why?

## Linked issues

- Fixes #
- Relates-to #

## Screenshots / recording (if UI)

## Test plan

- [ ] How to reproduce the bug or validate the feature
- [ ] Automated tests added or updated if applicable
- [ ] Manual sanity checks if applicable

## Risk

- Impact area:
- Worst-case failure mode:
- Mitigation:

## Notes for reviewers

- Key files or tricky parts:
- Follow-ups:
```
