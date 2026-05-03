"""Vapor creation must produce ON_GRID_STORAGE cells (no EnTT entity).

`createVaporTerrainEntity` writes a vapor voxel into VDB storage with
terrain id ``-1`` (ON_GRID_STORAGE). It must not allocate an EnTT entity
or write a positive entity handle into the terrain id grid. These tests
pin that contract through the public ``World.dispatch_vapor_creation_event``
binding.
"""

from __future__ import annotations

from time import sleep

from helpers import build_minimal_test_manager, water_vapor

from aetherion import Position


def _coord_to_position(coord: tuple[int, int, int]) -> Position:
    position = Position()
    position.x, position.y, position.z = coord
    return position


def test_vapor_creation_event_lands_as_on_grid_storage():
    """Dispatching a `VaporCreationEvent` at a NONE cell must produce a
    vapor voxel with terrain id ``-1`` (ON_GRID_STORAGE), never a
    positive EnTT entity handle. Vapor amount is written to the matter
    container exactly as requested.
    """
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()

    # `empty_square_world_factory` fills z=0 with stone; z>=1 is NONE.
    # Pick a target up at z=2 so we don't bump into anything below.
    target = (1, 1, 2)
    assert voxel_grid.get_terrain(*target) == -2, "Setup invariant: target is NONE"

    manager.current.world.dispatch_vapor_creation_event(
        position=_coord_to_position(target),
        amount=10,
    )
    manager.update()
    sleep(0.001)

    tid = voxel_grid.get_terrain(*target)
    assert tid == -1, (
        f"Vapor cell at {target} has tid={tid}, expected -1 (ON_GRID_STORAGE)"
    )
    assert water_vapor(voxel_grid, *target) == 10


def test_vapor_creation_does_not_grow_entity_count():
    """Successive vapor-creation dispatches must not allocate EnTT
    entities. Two dispatches into different NONE cells should leave
    the registry's alive-entity count unchanged.

    No public binding for "alive entity count" exists today; instead we
    verify the absence of positive terrain ids at every produced vapor
    cell — a positive id would indicate `registry.create()` ran and the
    handle was written into the grid.
    """
    manager = build_minimal_test_manager(3, 3, 3)
    voxel_grid = manager.current.world.get_voxel_grid()

    targets = [(0, 0, 2), (2, 2, 2)]
    for target in targets:
        assert voxel_grid.get_terrain(*target) == -2

    for target in targets:
        manager.current.world.dispatch_vapor_creation_event(
            position=_coord_to_position(target),
            amount=5,
        )
    manager.update()
    sleep(0.001)

    for target in targets:
        tid = voxel_grid.get_terrain(*target)
        assert tid == -1, (
            f"Vapor cell at {target} got tid={tid}; any value other than -1 "
            f"means an EnTT entity was created"
        )
        assert water_vapor(voxel_grid, *target) == 5
