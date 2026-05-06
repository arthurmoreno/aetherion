---
description: Aetherion staged-changes → branch → commit → push → PR workflow
---

# Branch / Commit / PR Workflow — Aetherion

Use this when the user asks to "commit and open a PR", "publish staged changes",
or anything that takes locally-staged work all the way through GitHub.

Repo: `arthurmoreno/aetherion` (`git@github.com:arthurmoreno/aetherion.git`).
Issues: <https://github.com/arthurmoreno/aetherion/issues>.

Equivalent to the `/github-staged-publish` skill. Use this rule when reasoning
about the flow inline; invoke the skill when the user explicitly asks for it.

---

## Precondition: staged files

Assume the user has already staged the files they want to ship. **Do not run
`git add` on your own.** Verify with `git diff --cached --name-only`.

- If something is staged → proceed with that exact set.
- If nothing is staged → **stop and ask** the user which files to stage. Do
  not infer from `git status` and do not stage everything.

---

## Steps

### 0. Find or create a GitHub issue

Search existing open and closed issues for one that matches the staged changes:

```bash
gh issue list --repo arthurmoreno/aetherion --search "<keywords>" --state all --limit 20
```

- If a matching issue exists → use its number.
- If none exists → create one with `gh issue create --repo arthurmoreno/aetherion`.

Capture the resulting `<issue-id>` and a kebab-case `<issue-short-name>`
(derived from the issue title) for the next step. Pick the right `<type>`
prefix based on the change:

| Change kind | Type prefix | Branch naming |
|---|---|---|
| New behaviour or capability | `feature` | `feature#<id>-<topic>` |
| Defect / regression with a clear bug | `bug` | `bug#<id>-<topic>` |
| Quick fix following a bug-style report | `fix` | `fix#<id>-<topic>` |
| Tooling / docs / repo hygiene / non-runtime | `chore` | `chore/<id>-<topic>` |

> Note: `chore` branches use a slash (`chore/<id>-<topic>`); the others use
> a hash (`feature#<id>-<topic>`, `bug#<id>-<topic>`, `fix#<id>-<topic>`).
> This matches the existing branches in the repo.

### 1. Sync main and branch off it

```bash
git fetch origin
git checkout main
git pull --ff-only origin main
git checkout -b <branch-name>   # following the table above
```

If the user has uncommitted-but-staged changes on the current branch, carry
them across with `git stash --keep-index` / `git stash pop` rather than losing
them. Verify staging is intact after the branch switch with
`git diff --cached --name-only`.

### 2. Commit using the staged files

Generate the commit message from the staged diff (`git diff --cached`). Match
the style of recent commits (`git log -5 --oneline`). Pass the message via
HEREDOC.

**Title format:** `[<type>#<id>] <Imperative summary>.` — type prefix with
`#`, period at the end of the title. Example: `[chore#45] Ignore
water_debug.jsonl and track docs/CLAUDE.md.`

The issue is auto-linked by the `[type#<id>]` title prefix; **do not** add a
`Refs #<id>` line to the commit body. The PR body's `Closes #<id>` is what
auto-closes the issue on merge.

```bash
git commit -m "$(cat <<'EOF'
[<type>#<id>] <Imperative summary>.

<one-paragraph why, derived from the staged diff>

Co-authored-by: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

The `Co-authored-by:` trailer is the aetherion convention — keep it.

- Never use `--amend` for this flow — always create a new commit.
- Never use `--no-verify` by default. If a hook fails, fix the underlying
  issue and create a **new** commit (the failed commit did not happen, so
  amending would silently rewrite the previous one).

#### Hook-failure carve-out

If a hook fails for a reason **unrelated to the staged change** (pre-existing
breakage on `main` that the staged diff doesn't touch — e.g. a separate test
abort, a clang-format violation in unrelated files), surface this to the user
verbatim and ask whether to override with `--no-verify`. **Do not assume
authorization.** When the user explicitly approves, note it in the PR body.

### 3. Push the branch to origin

```bash
git push -u origin <branch-name>
```

### 4. Open the PR with `gh`

```bash
gh pr create --repo arthurmoreno/aetherion \
  --base main \
  --title "[<type>#<id>] <Same as commit title>." \
  --body "$(cat <<'EOF'
Closes #<issue-id>.

## Summary
<1–3 bullets summarising the staged change>

## Test plan
- [ ] <how to verify>
EOF
)"
```

Return the PR URL in the final message so the user can open it.

GitHub's squash-merge will append the `(#<pr-id>)` suffix to the title
automatically (e.g. `[chore#45] … (#46)`); do not add it manually.

---

## Hard rules

- **Never** stage files yourself. Staging is the user's signal of scope.
- **Never** push to `main` or force-push.
- **Never** skip hooks (`--no-verify`, `--no-gpg-sign`, etc.) without
  explicit user authorization for that PR; see the carve-out above.
- **Never** create the issue without searching first — duplicates are worse
  than waiting.
- **Always** branch from a freshly-pulled `main`, not from whatever was
  checked out.
- **Always** include `Closes #<id>` in the PR body so the issue is linked
  and auto-closes on merge.
- **Always** keep the `Co-authored-by: Claude …` trailer on commits — this
  is the aetherion convention.
