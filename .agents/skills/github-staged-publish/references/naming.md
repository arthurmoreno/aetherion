# Naming

These patterns are derived from the repository's existing branch and commit history.

## Work Type Detection

Use one of:

- `feature`
- `bug`
- `chore`

## Branch Naming

Prefer these branch formats:

- Feature: `feature#<issue-id>-<short-kebab-topic>`
- Bug: `bug#<issue-id>-<short-kebab-topic>`
- Chore: `chore/<issue-id>-<short-kebab-topic>`

Examples seen in the repo:

- `feature#211-get-entity-command`
- `feature#204-water-sim-configurable`
- `bug#183`
- `chore/13-item-config-layout-test-gate`

When deriving `<short-kebab-topic>`, keep it brief and action-oriented.

## Commit Titles

Prefer the dominant commit pattern:

- `[feature#209] Create option to toggle water auto-balancing.`
- `[feature#208] Create Left-to-Right Mountain side for testing and ref.`
- `[chore#20] Attempts to sync subrepos.`

Standard format:

- `[{type}#{issue_id}] {summary}`

For chores, keep the commit tag as `chore#{issue_id}` even though the branch uses `chore/{issue_id}-...`.

## PR Titles

- Use exactly the same title as the commit title chosen for the publish.

