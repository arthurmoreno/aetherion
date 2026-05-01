---
description: Rules for creating new epic plans in Aetherion
---

# Epic Plan Creation Rules

All epic plans must follow the structure defined in
[`.claude/docs/PLANNING_RULES.md`](../docs/PLANNING_RULES.md).

## TDD is mandatory

Before a task can be marked `in-progress`, its test must exist or be explicitly
outlined in the **Test** field of [Section 5 (Detailed Task Descriptions)](../docs/PLANNING_RULES.md#5-detailed-task-descriptions).

- Write the failing test first.
- The task's **Goal** is not met until that test passes.
- A task with no **Test** entry cannot be started — add one or mark it `blocked`.

## Checklist before opening a new plan file

1. Filename follows `YYYY-MM-DD-<short-slug>.md` and lives in `.claude/docs/epics-plans/`.
2. Metadata block is present (title, created date).
3. Executive Summary is one prose paragraph — no bullet lists.
4. Task Tracking Table is populated with at least one row.
5. Every task row has a matching sub-section in Section 5 with **Goal**, **Files**, and **Test**.
6. **Test** field is filled in before setting any task to `in-progress`.
