# GitHub Staged Publish

Use this skill only after the user has already reviewed the code and staged the exact changes that should be published. This skill owns the handoff point after implementation — not coding, file selection, or staging.

## Purpose

1. Review staged changes.
2. If no issue was provided, create a related GitHub issue from the staged diff.
3. Create the branch if needed.
4. Commit only the staged changes using the repo's issue-tag pattern.
5. Push the branch.
6. Open a PR against `main` with the same title as the commit message.

## Execution Surface

- Use local `git` for staged-diff inspection, branch creation, commit, and push.
- Prefer the built-in GitHub plugin/app tools for issue and PR creation.
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
   - Create one on GitHub using the matching issue template below.
   - Keep the issue title short and action-oriented.
4. Determine the issue tag and branch name using the naming rules below.
5. Branch handling:
   - If already on the correctly named issue branch, keep it.
   - If on `main`, create the new issue branch from `main`.
   - If on another unrelated branch, stop and ask before rewriting branch context.
6. Create the commit from staged changes only.
7. Push the branch to `origin` with upstream tracking.
8. Open a PR against `main` using the PR body template below.

## Safety Rules

- Never stage extra files.
- Never commit unstaged changes.
- Never use `git add -A` in this workflow.
- If there are no staged changes, stop and report it.
- If branch creation from `main` would be unsafe because work is currently based on another branch, stop and ask.

---

## Naming Rules

**Work types:** `feature`, `bug`, `chore`

**Branch format:**
- Feature: `feature#<issue-id>-<short-kebab-topic>`
- Bug: `bug#<issue-id>-<short-kebab-topic>`
- Chore: `chore/<issue-id>-<short-kebab-topic>`

Examples from repo history:
- `feature#211-get-entity-command`
- `feature#204-water-sim-configurable`
- `bug#183`
- `chore/13-item-config-layout-test-gate`

**Commit title format:** `[{type}#{issue_id}] {summary}`

Examples:
- `[feature#209] Create option to toggle water auto-balancing.`
- `[feature#208] Create Left-to-Right Mountain side for testing and ref.`
- `[chore#20] Attempts to sync subrepos.`

Note: For chores, keep the commit tag as `chore#{issue_id}` even though the branch uses `chore/{issue_id}-...`.

**PR title:** exactly the same as the commit title.

---

## Issue Templates

Keep issue titles short and action-oriented. Fill only the parts supported by the staged diff and user prompt.

### Feature

Title style: `Create ...` / `Add ...` / `Support ...`

Body sections: Problem / motivation, Proposed solution, User story, Acceptance criteria, Non-goals, Technical notes.

### Bug

Title style: `Fix ...` / `Resolve ...` / `Correct ...`

Body sections: Summary, Reproduction, Expected behavior, Actual behavior, Suspected cause, Acceptance criteria.

### Chore

Title style: `Refactor ...` / `Update ...` / `Improve ...` / `Clean up ...`

Body sections: Summary, Scope, Constraints, Plan, Risks, Acceptance criteria.

---

## PR Body Template

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

---

## Output Contract

At the end of the run, report:

- Issue URL
- Branch name
- Commit SHA
- Push status
- PR URL
- Any blocker or manual follow-up
