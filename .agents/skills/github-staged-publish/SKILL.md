---
name: github-staged-publish
description: Publish already-reviewed staged changes for this project by inspecting the staged diff, creating a related GitHub issue when needed, creating or validating the branch from `main`, generating a commit message that includes the issue tag, pushing the branch, and opening a PR against `main` with the same title as the commit.
---

# GitHub Staged Publish

Use this skill only after the user has already reviewed the code and staged the exact changes that should be published.

This skill is for the handoff point after implementation. It does not own coding, file selection, or staging. It assumes the user has already decided what belongs in the publish set.

## Purpose

Run this workflow:

1. Review staged changes.
2. If no issue was provided, create a related GitHub issue from the staged diff.
3. Create the branch if needed.
4. Commit only the staged changes using the repo's issue-tag pattern.
5. Push the branch.
6. Open a PR against `main` with the same title as the commit message.

## Execution Surface

- Use local `git` for staged-diff inspection, branch creation, commit, and push.
- Prefer Codex's GitHub plugin/app tools for issue and PR creation.
- If the GitHub connector is unavailable, stop and report the blocker before falling back.

## Required Inputs

- The staged diff in the current repository.
- Optionally, a GitHub issue number or URL supplied by the user.

## Preconditions

- The user has already reviewed the code and staged the intended changes.
- There is at least one staged change.
- The repository has a `main` branch and a configured `origin`.

## Workflow

1. Inspect the staged diff with `git diff --cached --stat` and `git diff --cached`.
2. Infer the work type from the staged changes and user instruction: `feature`, `bug`, or `chore`.
3. If the user did not provide an issue:
   - create one on GitHub using the matching template in the references below
   - keep the issue title short and action-oriented
4. Determine the issue tag and branch name using [naming.md](references/naming.md).
5. Branch handling:
   - if already on the correctly named issue branch, keep it
   - if on `main`, create the new issue branch from `main`
   - if on another unrelated branch, stop and ask before rewriting branch context
6. Create the commit from staged changes only.
7. Push the branch to `origin` with upstream tracking.
8. Open a PR against `main` with:
   - PR title exactly equal to the commit title
   - PR body using [pr-body.md](references/pr-body.md)

## Branch Rules

- Always branch from `main` when creating a new publish branch.
- Do not silently reuse an unrelated branch.
- Do not rename an existing branch if it already clearly matches the issue.

## Safety Rules

- Never stage extra files.
- Never commit unstaged changes.
- Never use `git add -A` in this workflow.
- If there are no staged changes, stop and report it.
- If branch creation from `main` would be unsafe because work is currently based on another branch, stop and ask.

## References

- Naming and branch rules: [naming.md](references/naming.md)
- Issue templates: [issue-templates.md](references/issue-templates.md)
- PR body format: [pr-body.md](references/pr-body.md)

## Output Contract

At the end of the run, report:

- Issue URL
- Branch name
- Commit SHA
- Push status
- PR URL
- Any blocker or manual follow-up

