# Workflow

This reference defines the Aetherion GitHub workflow for Codex use.

## Naming

- Branch: `<type>/<issue-id>-<short-kebab-topic>`
- Commit title: include issue reference
- PR title: include issue reference

Examples:

- Branch: `feature/123-add-world-seed-option`
- Commit: `feat(world): add deterministic seed loading (refs #123)`
- PR: `feat(world): deterministic seed loading (#123)`

## Process

1. Identify `bug`, `feature`, or `chore`.
2. Search for duplicates before opening a new issue.
3. Confirm the work belongs in `arthurmoreno/aetherion`.
4. Run the mapped checks before commit or PR creation when the user asks for verification.
5. If checks fail, stop and report instead of silently bypassing them.

## End-Of-Run Report

When this skill is used for real GitHub workflow execution, report:

- Issue URL
- Target repository
- Branch name
- Build result
- Test result
- Commit SHA if created
- Push status
- PR URL if created
- Next action if blocked
