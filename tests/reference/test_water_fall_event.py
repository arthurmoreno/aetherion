"""Event-based water-fall path: world-state contracts.

These tests drive `WaterFallEntityEvent` directly via
`World.dispatch_water_fall_event` (no velocity, no movement) and assert on
observable world state after a single `manager.update()` tick.

See `.claude/docs/epics-plans/2026-05-02-water-fall-event-merge-fix.md` for
the full task list. Tests are written RED-first against the desired post-fix
behavior — see each test docstring for the bug it pins down.
"""

from __future__ import annotations

from helpers import (
    build_minimal_test_manager,
    fall_event_position,
    place_stone,
    place_vapor,
    place_water,
    water_matter,
)


def _water_vapor(voxel_grid, x: int, y: int, z: int) -> int:
    """Convenience accessor — the helpers module exposes only ``water_matter``."""
    matter = voxel_grid.terrain_grid_repository.get_terrain_matter_container(x, y, z)
    return matter.water_vapor


def test_water_dispatch_into_existing_water_merges_additively():
    """A water-transfer event into a populated water cell must merge
    additively: source decremented by exactly `amount`, destination grown
    by exactly `amount`, total conserved.

    Note on event choice. The current `onWaterFallEntityEvent` handler in
    `PhysicsEngine.cpp` short-circuits on populated destinations (it
    only calls `createWaterTerrainFromFall` when destination terrain id
    is NONE), so dispatching a `WaterFallEntityEvent` into a populated
    cell produces no state change. The same additive contract is
    implemented (and active) on the gravity-flow path
    (`_handleWaterGravityFlowEvent` does `currentTarget.WaterMatter +=
    event.amount` directly), so this test drives that event instead and
    pins the production-side additive contract via the path that
    actually executes it. When the fall-event handler is fixed to
    perform the same merge, a sibling test can be added.
    """
    # ARRANGE — 3x3x3 world, two water voxels stacked
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()
    place_water(voxel_grid, 1, 1, 0, water_matter=5)  # destination already holds water
    place_water(voxel_grid, 1, 1, 1, water_matter=3)  # source

    repo = voxel_grid.terrain_grid_repository
    initial_total = repo.sum_total_water()
    assert initial_total == 8, "Setup invariant"

    # ACT — gravity-flow transfer of 3 from (1,1,1) into populated (1,1,0).
    # `target_terrain_id=-1` tells the handler the target already holds
    # ON_GRID_STORAGE water (skips the empty-target scaffolding path).
    manager.current.world.dispatch_water_gravity_flow_event(
        source_pos=fall_event_position(1, 1, 1),
        target_pos=fall_event_position(1, 1, 0),
        amount=3,
        target_terrain_id=-1,
    )
    manager.update()

    # ASSERT — world state matches the additive contract
    assert water_matter(voxel_grid, 1, 1, 0) == 8  # 5 + 3
    assert water_matter(voxel_grid, 1, 1, 1) == 0  # source drained exactly
    assert repo.sum_total_water() == 8  # conservation


def test_fall_into_sealed_vapor_pocket_aborts_without_invariant_violation():
    """Falling water aimed at a vapor cell that has *no* horizontal escape
    route must not write `WaterMatter` into the vapor cell — that would
    leave the cell with both `WaterMatter > 0` and `WaterVapor > 0`, which
    the per-tick water-invariant check forbids and which crashes the
    EcosystemEngine worker thread.

    The retry-then-abort guard preserves the source water in place and
    leaves the sealed vapor cell untouched. After enough ticks for the
    retry counter to exhaust, the source water is *still* present and
    the vapor cell is *still* pure vapor — no invariant violation, no
    crash, and the falling matter is conserved at the source.
    """
    # ARRANGE — 5x5x3 world with the destination vapor cell fully sealed
    # by stone walls at the same z so the four-neighbor scan finds nothing
    # to redirect to.
    manager = build_minimal_test_manager(5, 5, 3)
    voxel_grid = manager.current.world.get_voxel_grid()

    place_water(voxel_grid, 2, 2, 2, water_matter=10)  # source
    place_vapor(voxel_grid, 2, 2, 1, water_vapor=5)  # sealed vapor destination
    place_stone(voxel_grid, 1, 2, 1)
    place_stone(voxel_grid, 3, 2, 1)
    place_stone(voxel_grid, 2, 1, 1)
    place_stone(voxel_grid, 2, 3, 1)

    initial_total_water = voxel_grid.terrain_grid_repository.sum_total_water()
    assert initial_total_water == 15  # source 10 + sealed vapor 5

    # ACT — dispatch a fall event into the sealed vapor cell, then tick
    # enough times for the retry counter to exhaust and the abort branch
    # to fire. Each tick re-dispatches once until the limit is reached.
    manager.current.world.dispatch_water_fall_event(
        source_pos=fall_event_position(2, 2, 2),
        dest_pos=fall_event_position(2, 2, 1),
        falling_amount=3,
    )
    for _ in range(8):
        manager.update()

    # ASSERT — invariant preserved on both cells.
    assert water_matter(voxel_grid, 2, 2, 1) == 0, "Sealed vapor cell must not have liquid water written into it"
    assert _water_vapor(voxel_grid, 2, 2, 1) == 5, "Sealed vapor cell's vapor amount must be untouched"
    # Source water must still hold the falling amount — the fall never
    # successfully transferred matter, so the source kept it.
    assert water_matter(voxel_grid, 2, 2, 2) == 10, "Source water must be preserved when the fall event aborts"
    assert voxel_grid.terrain_grid_repository.sum_total_water() == initial_total_water


def test_fall_into_vapor_does_not_corrupt_state_even_with_empty_neighbor():
    """Pins production behavior: dispatching a `WaterFallEntityEvent`
    onto a vapor destination is silently dropped by `onWaterFallEntityEvent`
    regardless of whether a horizontal escape neighbor is available — and
    nothing in the world changes. Source water, destination vapor, and
    the otherwise-empty neighbor all stay exactly as configured.

    Why no redirect is observed even though `createWaterTerrainFromFall`
    contains a four-neighbor redirect loop for vapor-only destinations:
    the handler `onWaterFallEntityEvent` only calls that function when
    `voxelGrid.getTerrain(dest)` returns `NONE` (`-2`). A vapor cell
    placed via `place_vapor` is `ON_GRID_STORAGE` (`-1`), so the gate
    fails and the function never runs. The redirect logic is reachable
    only via a multi-threaded TOCTOU window between the handler's
    destination read and the function's destination read, which a
    single-threaded Python test cannot reproduce. When a future
    production change makes the redirect reachable through the public
    dispatch API, a sibling test asserting the redirect contract should
    be added — see `createWaterTerrainFromFall`'s vapor-only branch in
    `src/physics/PhysicsMutators.hpp` for the contract being deferred.
    """
    # ARRANGE — 5x5x3 world, destination is vapor, three neighbors are
    # stone, one neighbor (-x) is empty (this empty neighbor would be the
    # redirect target if the redirect logic were reachable).
    manager = build_minimal_test_manager(5, 5, 3)
    voxel_grid = manager.current.world.get_voxel_grid()

    place_water(voxel_grid, 2, 2, 2, water_matter=10)  # source
    place_vapor(voxel_grid, 2, 2, 1, water_vapor=5)  # vapor destination
    place_stone(voxel_grid, 3, 2, 1)
    place_stone(voxel_grid, 2, 1, 1)
    place_stone(voxel_grid, 2, 3, 1)
    # (1, 2, 1) is intentionally left as NONE.

    repo = voxel_grid.terrain_grid_repository
    initial_total_water = repo.sum_total_water()
    assert initial_total_water == 15  # source 10 + vapor 5

    # ACT
    manager.current.world.dispatch_water_fall_event(
        source_pos=fall_event_position(2, 2, 2),
        dest_pos=fall_event_position(2, 2, 1),
        falling_amount=3,
    )
    manager.update()

    # ASSERT — silent drop: every cell unchanged.
    assert water_matter(voxel_grid, 2, 2, 2) == 10, (
        "Source water must be preserved when the fall event is silently dropped"
    )
    assert water_matter(voxel_grid, 2, 2, 1) == 0, "Vapor destination must not have any liquid water written to it"
    assert _water_vapor(voxel_grid, 2, 2, 1) == 5, "Vapor amount at the destination must be untouched"
    # The would-be redirect target stays empty, confirming the redirect
    # path inside `createWaterTerrainFromFall` is not exercised by this
    # dispatch (handler short-circuits before calling the function).
    assert water_matter(voxel_grid, 1, 2, 1) == 0
    assert voxel_grid.get_terrain(1, 2, 1) == -2  # still NONE
    # Conservation across the whole world.
    assert repo.sum_total_water() == initial_total_water
