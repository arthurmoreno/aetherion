---
name: github-project-mcp
description: Use the Aetherion-specific GitHub MCP workflow when the user asks to create, update, review, or route GitHub issues and pull requests for `arthurmoreno/aetherion`.
---

# GitHub Project MCP

Use this skill for GitHub issue and pull request work tied to `arthurmoreno/aetherion`.

## Execution Surface

- Prefer the built-in `GitHub` plugin and its MCP/app tools for issue, PR, comment, and metadata operations.
- If the required GitHub connector access is unavailable, stop and report the blocker before using any fallback.

## Workflow

1. Determine the work type: `bug`, `feature`, or `chore`.
2. Read [repo-mapping.md](references/repo-mapping.md) for the target repository and local checks.
3. Read [workflow.md](references/workflow.md) for naming and execution rules.
4. Search for an existing issue or PR before creating a new one.
5. Keep issue, branch, commit, and PR naming aligned with [workflow.md](references/workflow.md).
6. If code changes are involved, run the mapped checks before commit or PR creation.
7. Reuse [pr-template.md](references/pr-template.md) for PR bodies unless the user asks for a different format.

## Rules

- Use issue-first workflow by default for net-new implementation work.
- Always include the issue reference in branch, commit, and PR title when an issue exists.
- Do not include secrets in issues, PRs, comments, or commits.
- If lint or test commands are missing or invalid, stop and report that the mapping needs an update.

## References

- For repo routing and checks: [repo-mapping.md](references/repo-mapping.md)
- For naming rules and end-of-run reporting: [workflow.md](references/workflow.md)
- For the default PR body shape: [pr-template.md](references/pr-template.md)
