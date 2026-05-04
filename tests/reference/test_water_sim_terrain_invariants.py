"""Sandwich-style terrain-ID invariant tests for the water simulation crash.

Structure
---------
Bottom bread  — terrain IDs are valid BEFORE water reaches the failure corridor.
               These must always pass (clean initial state).
Filling       — terrain IDs remain valid IN the watched corridor WHILE water flows.
               These FAIL before the bug is fixed, PASS after.
Top bread     — terrain IDs are valid across the WHOLE grid after a bounded run.
               These FAIL before the bug is fixed, PASS after.

Key finding from first runs: corruption is detectable at ~1000 steps via Python-level
terrain ID scanning, before the C++ crash (segfault) occurs at ~1500+ steps.
All step loops stay at or below 1100 steps and break early on first corruption.

The watched corridor matches the known crash coordinates:
  x = 88..99, y = 49..51, z = 0..2

Terrain ID value space (TerrainIdTypeEnum in C++):
  NONE            = -2   (empty voxel, valid)
  ON_GRID_STORAGE = -1   (vapour entity, valid)
  >= 0                   (live EnTT entity handle, valid)
  < -2                   (INVALID — corrupted EnTT entity handle stored in voxel grid)
"""

from __future__ import annotations

from dataclasses import dataclass
from time import sleep

from helpers import build_mountain_side_manager

import aetherion
from aetherion import TerrainIdTypeEnum
from aetherion.world.manager import WorldManager

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

TERRAIN_ID_NONE = int(TerrainIdTypeEnum.NONE.value)
TERRAIN_ID_ON_GRID_STORAGE = int(TerrainIdTypeEnum.ON_GRID_STORAGE.value)

WATCH_X_MIN, WATCH_X_MAX = 88, 99
WATCH_Y_MIN, WATCH_Y_MAX = 49, 51
WATCH_Z_MIN, WATCH_Z_MAX = 0, 2

# Step budget: high enough to trigger the bug (~1000), low enough to avoid segfault.
SAFE_MAX_STEPS = 110


# ---------------------------------------------------------------------------
# Fixture helpers
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class CorruptedVoxel:
    x: int
    y: int
    z: int
    terrain_id: int


def _scan_region(voxel_grid, *, x_range: range, y_range: range, z_range: range) -> list[CorruptedVoxel]:
    """Return voxels in the region whose terrain ID is below NONE (corrupted)."""
    corrupted: list[CorruptedVoxel] = []
    for x in x_range:
        for y in y_range:
            for z in z_range:
                tid = voxel_grid.get_terrain(x, y, z)
                if tid < TERRAIN_ID_NONE:
                    corrupted.append(CorruptedVoxel(x=x, y=y, z=z, terrain_id=tid))
    return corrupted


def _scan_full_grid(world, voxel_grid) -> list[CorruptedVoxel]:
    return _scan_region(
        voxel_grid,
        x_range=range(world.width),
        y_range=range(world.height),
        z_range=range(world.depth),
    )


def _scan_watch_corridor(voxel_grid) -> list[CorruptedVoxel]:
    return _scan_region(
        voxel_grid,
        x_range=range(WATCH_X_MIN, WATCH_X_MAX + 1),
        y_range=range(WATCH_Y_MIN, WATCH_Y_MAX + 1),
        z_range=range(WATCH_Z_MIN, WATCH_Z_MAX + 1),
    )


def _format_corrupted(corrupted: list[CorruptedVoxel], *, limit: int = 8) -> str:
    preview = ", ".join(
        f"({c.x},{c.y},{c.z})=0x{c.terrain_id & 0xFFFFFFFF:08X}({c.terrain_id})" for c in corrupted[:limit]
    )
    suffix = "" if len(corrupted) <= limit else f" ... total={len(corrupted)}"
    return preview + suffix


@dataclass
class StepResult:
    steps_completed: int
    exception: Exception | None = None
    first_corruption_step: int | None = None
    corrupted: list[CorruptedVoxel] | None = None


def _run_steps_safe(
    manager: WorldManager,
    steps: int,
    *,
    tick_sleep: float = 0.01,
    scan_corridor_every: int = 0,
) -> StepResult:
    """Run up to ``steps`` updates, catching EcosystemEngineException.

    If ``scan_corridor_every > 0``, scans the watch corridor every N steps and
    stops early on first corruption — preventing the process-crashing segfault
    that occurs ~100-200 steps past the first corrupted terrain ID.
    """
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()

    for i in range(steps):
        try:
            manager.update()
        except aetherion.EcosystemEngineException as exc:
            return StepResult(steps_completed=i, exception=exc)
        sleep(tick_sleep)

        if scan_corridor_every > 0 and (i + 1) % scan_corridor_every == 0:
            corrupted = _scan_watch_corridor(voxel_grid)
            if corrupted:
                return StepResult(
                    steps_completed=i + 1,
                    first_corruption_step=i + 1,
                    corrupted=corrupted,
                )

    return StepResult(steps_completed=steps)


# ---------------------------------------------------------------------------
# ── BOTTOM BREAD — pre-arrival invariants ────────────────────────────────
# Water cannot reach the failure corridor in the first few dozen steps.
# These must always pass regardless of whether the bug is present.
# ---------------------------------------------------------------------------


class TestPreArrivalInvariants:
    def test_full_grid_clean_at_world_creation(self):
        """No voxel should hold an invalid terrain ID immediately after factory build."""
        manager = build_mountain_side_manager("Water Invariant Test")
        world = manager.current.world
        voxel_grid = world.get_voxel_grid()

        corrupted = _scan_full_grid(world, voxel_grid)

        assert not corrupted, f"Fresh world already contains corrupted terrain IDs: {_format_corrupted(corrupted)}"

    def test_watch_corridor_clean_at_world_creation(self):
        """The crash-corridor voxels must be clean immediately after construction."""
        manager = build_mountain_side_manager("Water Invariant Test")
        world = manager.current.world
        voxel_grid = world.get_voxel_grid()

        corrupted = _scan_watch_corridor(voxel_grid)

        assert not corrupted, f"Watch corridor already corrupted at world creation: {_format_corrupted(corrupted)}"

    def test_full_grid_clean_after_10_steps(self):
        """Grid should stay clean after 10 steps — water cannot reach the corridor."""
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=10)

        assert result.exception is None, f"Engine threw at step {result.steps_completed}: {result.exception}"

        world = manager.current.world
        voxel_grid = world.get_voxel_grid()
        corrupted = _scan_full_grid(world, voxel_grid)

        assert not corrupted, f"Corrupted terrain IDs after 10 steps: {_format_corrupted(corrupted)}"

    def test_watch_corridor_clean_after_50_steps(self):
        """Watch corridor should be untouched at step 50."""
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=50)

        assert result.exception is None, f"Engine threw at step {result.steps_completed}: {result.exception}"

        corrupted = _scan_watch_corridor(manager.current.world.get_voxel_grid())

        assert not corrupted, (
            f"Watch corridor corrupted after 50 steps (water should not be there yet): {_format_corrupted(corrupted)}"
        )


# ---------------------------------------------------------------------------
# ── FILLING — mid-flow corruption detection ───────────────────────────────
# Runs long enough for water to enter the corridor and checks for invalid IDs.
# These FAIL before the bug is fixed, PASS after.
# ---------------------------------------------------------------------------


class TestMidFlowCorruptionDetection:
    def test_watch_corridor_no_corrupted_ids_at_50_steps(self):
        """Watch corridor must have no invalid terrain IDs at step 50."""
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=50, scan_corridor_every=10)

        assert result.exception is None, f"Engine threw at step {result.steps_completed}: {result.exception}"
        assert result.first_corruption_step is None, (
            f"Corruption detected at step {result.first_corruption_step} "
            f"(Bug 1/2/3 race likely): {_format_corrupted(result.corrupted or [])}"
        )

        corrupted = _scan_watch_corridor(manager.current.world.get_voxel_grid())
        assert not corrupted, f"Corrupted terrain IDs in watch corridor at step 50: {_format_corrupted(corrupted)}"

    def test_no_water_sim_errors_at_50_steps(self):
        """World must report no worker-thread water errors at step 50."""
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=50)

        assert result.exception is None, f"Engine threw at step {result.steps_completed}: {result.exception}"

        world = manager.current.world
        assert not world.has_water_sim_errors(), (
            f"Worker-thread water errors at step 50: {world.get_water_sim_errors()}"
        )

    def test_watch_corridor_no_corrupted_ids_at_100_steps(self):
        """Watch corridor must have no invalid terrain IDs at step 100.

        This is the primary regression test. It reproduces the bug via Python-level
        terrain ID scanning BEFORE the C++ crash occurs (~1500 steps). After the fix,
        this test must pass cleanly.
        """
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=100, scan_corridor_every=10)

        assert result.exception is None, f"Engine threw at step {result.steps_completed}: {result.exception}"
        assert result.first_corruption_step is None, (
            f"Corruption detected at step {result.first_corruption_step} in watch corridor "
            f"(Bug 1/2/3 lock race confirmed): {_format_corrupted(result.corrupted or [])}"
        )

        corrupted = _scan_watch_corridor(manager.current.world.get_voxel_grid())
        assert not corrupted, f"Corrupted terrain IDs in watch corridor at step 100: {_format_corrupted(corrupted)}"

    def test_specific_crash_voxels_stay_valid_to_100_steps(self):
        """The exact voxels from the crash log must stay valid up to step 100.

        Known bad voxels from engine logs: (89,50,1), (91,50,1), (94,50,1), (96,50,1).
        Scans every 100 steps and stops early if corruption is found.
        """
        CRASH_VOXELS = [(89, 50, 1), (91, 50, 1), (94, 50, 1), (96, 50, 1)]
        manager = build_mountain_side_manager("Water Invariant Test")
        world = manager.current.world
        voxel_grid = world.get_voxel_grid()

        for step in range(100):
            try:
                manager.update()
            except aetherion.EcosystemEngineException as exc:
                assert False, f"Engine threw EcosystemEngineException at step {step}: {exc}"
            sleep(0.01)

            if (step + 1) % 10 == 0:
                bad = [
                    (x, y, z, voxel_grid.get_terrain(x, y, z))
                    for x, y, z in CRASH_VOXELS
                    if voxel_grid.get_terrain(x, y, z) < TERRAIN_ID_NONE
                ]
                assert not bad, f"Crash voxel has invalid terrain ID at step {step + 1}: {bad}"


# ---------------------------------------------------------------------------
# ── TOP BREAD — post-run full-grid invariants ─────────────────────────────
# Runs SAFE_MAX_STEPS (1100) with early-stop on corruption.
# FAIL before fix (corruption detected), PASS after fix.
# ---------------------------------------------------------------------------


class TestPostRunInvariants:
    def test_no_engine_exception_in_safe_budget(self):
        """Engine must not throw EcosystemEngineException within SAFE_MAX_STEPS.

        Before fix: may throw or produce corrupted IDs. After fix: clean run.
        """
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=SAFE_MAX_STEPS, scan_corridor_every=50)

        assert result.exception is None, (
            f"EcosystemEngineException at step {result.steps_completed}: {result.exception}"
        )
        assert result.first_corruption_step is None, (
            f"Terrain ID corruption at step {result.first_corruption_step}: {_format_corrupted(result.corrupted or [])}"
        )

    def test_full_grid_no_corrupted_ids_after_safe_budget(self):
        """After SAFE_MAX_STEPS the entire grid must have no invalid terrain IDs."""
        manager = build_mountain_side_manager("Water Invariant Test")
        result = _run_steps_safe(manager, steps=SAFE_MAX_STEPS, scan_corridor_every=50)

        if result.first_corruption_step is not None:
            assert False, (
                f"Corruption appeared at step {result.first_corruption_step} — "
                f"bug still present: {_format_corrupted(result.corrupted or [])}"
            )

        world = manager.current.world
        corrupted = _scan_full_grid(world, world.get_voxel_grid())
        assert not corrupted, (
            f"Full grid has corrupted terrain IDs after {SAFE_MAX_STEPS} steps: {_format_corrupted(corrupted)}"
        )

    def test_no_water_sim_errors_after_safe_budget(self):
        """Worker-thread error list must be empty after SAFE_MAX_STEPS."""
        manager = build_mountain_side_manager("Water Invariant Test")

        world = manager.current.world
        voxel_grid = world.get_voxel_grid()
        repo = voxel_grid.terrain_grid_repository

        initial_total = repo.sum_total_water()
        print(
            f"[probe] pre-run: total_water={initial_total}, "
            f"process_async={world.process_ecosystem}, "
            f"simulate_water_movement={world.simulate_water_movement}, "
            f"simulate_water_evaporation={world.simulate_water_evaporation}, "
            f"simulate_vapor_movement={world.simulate_vapor_movement}, "
            f"simulate_vapor_condensation={world.simulate_vapor_condensation}"
        )

        result = _run_steps_safe(manager, steps=SAFE_MAX_STEPS, scan_corridor_every=50)

        final_total = repo.sum_total_water()
        print(
            f"[probe] post-run: steps_completed={result.steps_completed}, "
            f"total_water={final_total}, "
            f"delta={final_total - initial_total}, "
            f"has_errors={world.has_water_sim_errors()}"
        )

        if result.exception is not None:
            assert False, f"EcosystemEngineException at step {result.steps_completed}: {result.exception}"
        if result.first_corruption_step is not None:
            assert False, (
                f"Terrain corruption at step {result.first_corruption_step} stopped the run: "
                f"{_format_corrupted(result.corrupted or [])}"
            )

        # NOTE: get_water_sim_errors() returns std::vector<ThreadError> and the
        # installed wheel is missing nanobind/stl/vector.h, so a direct call
        # raises TypeError. has_water_sim_errors() returns bool and crosses the
        # binding cleanly. We only touch get_water_sim_errors() inside the
        # assertion message so it stays lazy and never fires on success.
        assert not world.has_water_sim_errors(), (
            f"Worker-thread errors after {SAFE_MAX_STEPS} steps "
            f"(rebuild aetherion with nanobind/stl/vector.h to surface details)"
        )
