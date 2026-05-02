"""Minimal mid-air falling water scenario.

5x5x10 empty world with a stone floor at z=0. A single water voxel sits
mid-air at ``WATER_POS = (2, 2, 8)`` with no initial velocity and no ECS
entity (placed via `place_water` which writes ON_GRID_STORAGE directly).
Under correct gravity behavior, the water should fall through the empty
column, land on the stone floor at z=0, and stop there.

Used by `tests/reference/test_water_falls_from_midair.py` to validate that
the velocity-grid-driven gravity discovery picks up ON_GRID_STORAGE water
voxels (the velocity seed at creation + propagation across `moveTerrain`).

Loadable via `WorldManager` for visual inspection in the live game.
"""

from __future__ import annotations

import aetherion
from aetherion.reference.world.scenarios.primitives import place_stone, place_water
from aetherion.reference.world.world_factories import EmptySquareWorldFactory

WORLD_WIDTH = 5
WORLD_HEIGHT = 5
WORLD_DEPTH = 10

WATER_POS: tuple[int, int, int] = (2, 2, 8)
INITIAL_WATER_MATTER: int = 1000


def water_fall_from_midair_world_factory(world_config: dict[str, int]) -> aetherion.World:
    """Build the mid-air falling water scenario world.

    Layout:
        z = 0       stone floor (5x5)
        z = 1..7    empty
        z = 8       water at (2, 2, 8)
        z = 9       empty

    The ``world_config`` dimensions are ignored — this scenario is fixed at
    5x5x10 so coordinates stay stable across runs.
    """
    del world_config
    factory = EmptySquareWorldFactory(width=WORLD_WIDTH, height=WORLD_HEIGHT, depth=WORLD_DEPTH)

    world = factory.generate_world()
    world.process_ecosystem_async = False

    voxel_grid = world.get_voxel_grid()

    for x in range(WORLD_WIDTH):
        for y in range(WORLD_HEIGHT):
            place_stone(voxel_grid, x, y, 0)

    place_water(voxel_grid, *WATER_POS, water_matter=INITIAL_WATER_MATTER)

    return world


def water_column_above_floor_world_factory(world_config: dict[str, int]) -> aetherion.World:
    """Variant for the "delete-floor → cascade" wake-up test.

    Layout:
        z = 0       stone floor (5x5)
        z = 1..4    water column at (2, 2, *), water_matter=1000 each
        z = 5..9    empty

    Tests then delete the stone at ``(2, 2, 0)`` and assert the water
    column collapses one step downward via the ``deleteTerrain`` wake-up
    cascade.
    """
    del world_config
    factory = EmptySquareWorldFactory(width=WORLD_WIDTH, height=WORLD_HEIGHT, depth=WORLD_DEPTH)

    world = factory.generate_world()
    world.process_ecosystem_async = False

    voxel_grid = world.get_voxel_grid()

    for x in range(WORLD_WIDTH):
        for y in range(WORLD_HEIGHT):
            place_stone(voxel_grid, x, y, 0)

    for z in (1, 2, 3, 4):
        place_water(voxel_grid, 2, 2, z, water_matter=INITIAL_WATER_MATTER)

    return world
