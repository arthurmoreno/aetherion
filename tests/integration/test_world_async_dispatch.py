"""Regression gates for the World async-dispatch migration to TBB.

Plan: .claude/docs/epics-plans/2026-05-09-tbb-task-group-migration.md

Pre-migration: each `World.update()` spawned two fresh OS threads via
`std::async(std::launch::async, ...)` (libstdc++ doesn't pool). At ~30
ticks/sec across two engines that's ~60 thread creations/second, leaving
glibc per-thread arena state in the process for minutes.

Post-migration: dispatch goes through a process-wide `tbb::task_group`
backed by TBB's persistent worker pool. Thread count plateaus after
warmup; the destructor blocks on in-flight tasks so teardown is clean.

These tests are the headline regression gates. The C++-level tests in
the plan's task table are not wired into the active pipeline (`tests/cpp/`
is not run by `make agent-build-install-test` — only Python tests are).
"""

from __future__ import annotations

import gc
import time
from pathlib import Path

import pytest

from aetherion import World

# Tolerance for thread-count delta across many updates. TBB may spin
# additional workers up on demand; we allow a small slack but anything
# beyond this means we've regressed back to per-tick std::async behavior.
THREAD_COUNT_SLACK = 8


def _count_threads() -> int:
    """Snapshot the current process's OS-thread count via /proc/self/task.

    Linux-specific. Tests on this repo run under the conda env on Linux
    (per CLAUDE.md / Makefile), so we don't bother with a Windows
    fallback — the test simply skips elsewhere.
    """
    return sum(1 for _ in Path("/proc/self/task").iterdir())


pytestmark = pytest.mark.skipif(
    not Path("/proc/self/task").exists(),
    reason="/proc/self/task only exists on Linux",
)


@pytest.fixture
def world():
    """Construct a World, yield it, then dispose to break the
    Python<->C++ ref cycle (matches WorldInterface dispose pattern)."""
    w = World(8, 8, 8)
    try:
        yield w
    finally:
        w.release_python_state()
        del w
        gc.collect()


def test_world_update_thread_count_plateau_after_warmup(world):
    """Headline regression gate: 100 successive `World.update()` calls
    must not grow the OS-thread count.

    Pre-migration: this test fails with hundreds of net-new threads
    (since each update spawned a fresh std::async OS thread per engine,
    which then exited but left arena state behind).

    Post-migration: TBB workers are spun up once on the first
    `update()`, then reused for every subsequent tick.
    """
    # Warmup tick — gives TBB time to spin its worker pool to
    # std::thread::hardware_concurrency().
    world.update()
    time.sleep(0.05)
    baseline = _count_threads()

    for _ in range(100):
        world.update()

    # Drain any in-flight workers so we sample after they're parked,
    # not while one is mid-execution (which could transiently look like
    # "+1 thread").
    time.sleep(0.1)
    final = _count_threads()

    delta = final - baseline
    assert abs(delta) <= THREAD_COUNT_SLACK, (
        f"thread count grew by {delta} across 100 updates "
        f"(baseline={baseline}, final={final}); slack={THREAD_COUNT_SLACK}. "
        "regression: per-tick std::async likely re-introduced — see plan "
        "2026-05-09-tbb-task-group-migration.md"
    )


def test_world_destruction_does_not_assert_with_in_flight_task():
    """`tbb::task_group` requires `wait()` before destruction or it
    asserts. `~World()` must call `asyncTasks_.wait()` for the
    destructor to be safe even when tasks are mid-execution.

    This test constructs a World, kicks off one update (which submits
    physics + ecosystem tasks), then immediately drops the world. Pass
    criterion: no abort, destructor returns within 1 second.
    """
    start = time.monotonic()
    w = World(8, 8, 8)
    w.update()  # submits physics + ecosystem tasks
    # Don't sleep — we want to test that ~World blocks on still-in-flight
    # work, not on already-completed work.
    w.release_python_state()
    del w
    gc.collect()
    elapsed = time.monotonic() - start
    assert elapsed < 1.0, (
        f"World construction + update + destruction took {elapsed:.2f}s "
        "(expected < 1s); ~World may not be wait()ing on asyncTasks_, "
        "or the wait is hanging"
    )


def test_repeated_world_construct_destruct_keeps_threads_bounded():
    """Construct + destruct 5 worlds in a row, verifying that thread
    count stays bounded. The TBB worker pool is process-wide and
    persistent, so the second world's first update should re-use the
    same pool without doubling thread count.

    Pre-migration: each world's std::async dispatch leaked threads on
    every update, and a 5-world sequence would multiply the leak.
    """
    # Warmup: spin up TBB workers once so subsequent destruct/construct
    # doesn't have to re-create them.
    w0 = World(8, 8, 8)
    w0.update()
    w0.release_python_state()
    del w0
    gc.collect()
    time.sleep(0.05)
    baseline = _count_threads()

    for _ in range(5):
        w = World(8, 8, 8)
        for _ in range(10):
            w.update()
        w.release_python_state()
        del w
        gc.collect()

    time.sleep(0.1)
    final = _count_threads()
    delta = final - baseline
    assert abs(delta) <= THREAD_COUNT_SLACK, (
        f"thread count grew by {delta} across 5 worlds × 10 updates "
        f"(baseline={baseline}, final={final}); "
        "regression: per-construct or per-update thread leak"
    )
