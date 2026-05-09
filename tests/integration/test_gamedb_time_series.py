"""GameDB time-series — bounded retention + persistence regression gates.

Plan: .claude/docs/epics-plans/2026-05-09-gamedb-time-series-bounded-retention.md

Tasks covered:
  Task 2 — putTimeSeries round-trips through SQLite (proves the
           `needsSync = true` fix actually persists data).
  Task 3 — 100K putTimeSeries calls keep the in-memory cache bounded by
           kMaxInMemoryPointsPerSeries (the safety net).
  Task 4 — AFTER INSERT trigger caps disk rows per series at
           kMaxOnDiskRowsPerSeries (the durable retention bound).

These are the regression gates for the runtime memory leak. Pre-fix
baseline (recorded in lifesim/.claude/docs/analysis/2026-05-09-memory-
leak-hunt-status.md) showed +325 MB across 30 cycles with pop=0; with
these tests green that path is closed.
"""

from __future__ import annotations

import gc
import uuid

import pytest

from aetherion import World


def _unique_series(prefix: str) -> str:
    """Generate a per-test unique series name. The C++ World opens a
    persistent SQLite file at `./data/game.sqlite` and reloads its
    contents on construction, so reusing a series name across runs (or
    across tests in one run) lets stale rows from earlier sessions
    shadow what the current test wrote. A uuid suffix sidesteps that
    coupling without needing to wipe the shared DB."""
    return f"{prefix}.{uuid.uuid4().hex[:8]}"


# Constants must match the C++ side. Hard-coded here on purpose — if
# the C++ side changes its caps, these tests should fail loudly so the
# coupling is reviewed (not silently re-derived).
MAX_IN_MEMORY = 1000
MAX_ON_DISK = 10_000

# Disk trim is amortised: it runs at most once per `kInsertsPerTrim`
# puts to a series, so the disk-row count can transiently exceed
# MAX_ON_DISK by up to (kInsertsPerTrim - 1) rows between trims.
INSERTS_PER_TRIM = 500
MAX_ON_DISK_WITH_OVERSHOOT = MAX_ON_DISK + INSERTS_PER_TRIM


@pytest.fixture
def world():
    """Construct a World, yield it, then dispose to break the
    Python<->C++ ref cycle (see WorldInterface dispose pattern)."""
    w = World(3, 3, 3)
    try:
        yield w
    finally:
        w.release_python_state()
        gc.collect()


# ─── Task 2 — round-trip via SQLite ──────────────────────────────────


def test_put_time_series_round_trips(world):
    """Insert one value, query it back. Pre-fix this would silently
    return nothing because `needsSync = false` short-circuited the
    flush — the value never reached SQLite."""
    series = _unique_series("test.roundtrip")
    world.put_time_series(series, 1234, 5.5)
    rows = world.query_time_series(series, 0, 10_000)
    assert len(rows) == 1
    ts, val = rows[0]
    assert ts == 1234
    assert val == 5.5


def test_put_time_series_round_trips_via_disk_only(world):
    """Insert past the in-memory cache cap with strictly older
    timestamps than what the cache retains, then query the cold range.
    Cache is empty for that range → query falls through to SQLite. Proves
    the disk path actually has the data, not just the cache."""
    # Insert 1500 samples at ts 1..1500. Cache retains the newest 1000
    # (ts 501..1500); querying [1, 100] is a guaranteed cache miss.
    series = _unique_series("test.disk_only")
    for ts in range(1, 1501):
        world.put_time_series(series, ts, float(ts))
    rows = world.query_time_series(series, 1, 100)
    assert len(rows) == 100, (
        f"expected 100 rows from disk for ts in [1,100], got {len(rows)} "
        "(cache should not contain this range; data must come from SQLite)"
    )
    assert rows[0] == (1, 1.0)
    assert rows[-1] == (100, 100.0)


# ─── Task 3 — in-memory cap holds across many inserts ────────────────


def test_in_memory_cache_bounded_after_many_inserts(world):
    """After 5K inserts the in-memory cache must hold ≤ 1000 entries.
    Pre-fix this would grow unbounded — the original leak symptom.
    5K is enough to prove the cap (5x over) without making the test
    long-running."""
    series = _unique_series("test.bounded")
    for ts in range(1, 5_001):
        world.put_time_series(series, ts, float(ts))
    size = world.peek_time_series_size(series)
    assert size <= MAX_IN_MEMORY, (
        f"in-memory cache grew to {size} (> cap {MAX_IN_MEMORY}); TimeSeriesComponent::addDataPoint eviction is broken"
    )


# ─── Task 4 — disk trim caps rows per series ─────────────────────────


def test_disk_trim_caps_rows_per_series(world):
    """After 12K inserts the disk row count must stay near the cap.
    Trim is amortised (every kInsertsPerTrim puts), so overshoot up to
    kInsertsPerTrim - 1 rows is allowed between trims."""
    series = _unique_series("test.disk_cap")
    for ts in range(1, 12_001):
        world.put_time_series(series, ts, float(ts))
    on_disk = world.count_time_series_rows_on_disk(series)
    assert 0 < on_disk <= MAX_ON_DISK_WITH_OVERSHOOT, (
        f"disk row count = {on_disk}; expected (0, {MAX_ON_DISK_WITH_OVERSHOOT}]. "
        "trimSeriesOnDisk is missing or broken."
    )


def test_disk_trim_retains_newest_rows(world):
    """Trim must evict oldest, not random. After enough inserts to
    trigger trim, a query for a cold range (well below the retained
    window) must return nothing — cache holds the newest 1000 and disk
    holds the newest ~10K."""
    series = _unique_series("test.disk_evict_oldest")
    for ts in range(1, 12_001):
        world.put_time_series(series, ts, float(ts))

    # Cache has ts 11001..12000. Disk should hold ~ts 2001..12000.
    # Querying [1, 1000] is below both → expect zero rows.
    rows = world.query_time_series(series, 1, 1_000)
    assert len(rows) == 0, (
        f"expected oldest rows to be evicted, but got {len(rows)} "
        "rows in [1, 1000]; trim may be evicting newest instead"
    )


# ─── Smoke test: aggregate ──────────────────────────────────────────


def test_no_growth_under_steady_inserts(world):
    """Sanity: the in-memory cache size for a series saturates at the
    cap and does not keep growing as more inserts arrive (the actual
    pre-fix leak signature)."""
    series = _unique_series("test.steady")
    for ts in range(1, 2_000):
        world.put_time_series(series, ts, float(ts))
    size_a = world.peek_time_series_size(series)

    for ts in range(2_000, 4_000):
        world.put_time_series(series, ts, float(ts))
    size_b = world.peek_time_series_size(series)

    assert size_a == MAX_IN_MEMORY
    assert size_b == MAX_IN_MEMORY
    assert size_a == size_b, (
        f"cache size grew from {size_a} to {size_b} between batches; eviction is not stable at the cap"
    )
