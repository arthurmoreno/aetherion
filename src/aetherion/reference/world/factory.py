"""High-level helpers that build populated ``World`` instances (demo-aligned layouts)."""

# ruff: noqa: E501 — dungeon mask rows are copied verbatim from the minimal dungeon demo.

from __future__ import annotations

from collections.abc import Callable

import aetherion
from aetherion import EntityEnum, MatterState, TerrainEnum, World
from aetherion.reference.systems.spring_water import SpringWaterSystem
from aetherion.reference.world.world_factories import (
    EmptySquareWorldFactory,
    MountainSideWorldFactory,
    PilarWorldFactory,
    PyramidWorldFactory,
    trapezium_column_top_z,
)


def pilar_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    world_factory = PilarWorldFactory(width=world_width, height=world_height, depth=world_depth)
    world = world_factory.generate_world()
    world.process_ecosystem_async = False

    return world


def pyramid_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    world_factory = PyramidWorldFactory(width=world_width, height=world_height, depth=world_depth)
    world = world_factory.generate_world()
    world.process_ecosystem_async = False

    return world


def mountain_side_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    world_factory = MountainSideWorldFactory(width=world_width, height=world_height, depth=world_depth)
    world = world_factory.generate_world()
    world.process_ecosystem_async = True
    world.water_auto_balancing = False
    world.simulate_water_movement = True
    world.simulate_water_evaporation = False
    world.simulate_vapor_movement = False
    world.simulate_vapor_condensation = False

    rx, ry, rz = mountain_ridge_source_xyz(world_width, world_height, world_depth)
    spring_pace: int = world_config.get("spring_pace", 5)
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry, source_z=rz))

    return world


def mountain_ridge_source_xyz(width: int, height: int, depth: int) -> tuple[int, int, int]:
    """Ridge cell for the river source: left column, middle ``y``, top surface ``z``."""
    x = 5
    y = (height // 2) if height > 0 else 0
    z = trapezium_column_top_z(x, width, depth)
    return x, y, z


def _seed_liquid_water_at(repo: aetherion.TerrainGridRepository, x: int, y: int, z: int) -> None:
    et = aetherion.EntityTypeComponent()
    et.main_type = EntityEnum.TERRAIN.value
    et.sub_type0 = TerrainEnum.WATER.value
    et.sub_type1 = 0
    repo.set_terrain_entity_type(x, y, z, et)

    mc = aetherion.MatterContainer()
    mc.terrain_matter = 0
    mc.water_matter = 1000
    mc.water_vapor = 0
    mc.bio_mass_matter = 0
    repo.set_terrain_matter_container(x, y, z, mc)

    repo.set_matter_state(x, y, z, MatterState.LIQUID)

    si = aetherion.StructuralIntegrityComponent()
    si.can_stack_entities = False
    si.max_load_capacity = -1
    si.matter_state = MatterState.LIQUID
    repo.set_terrain_structural_integrity(x, y, z, si)


def make_mountain_side_pre_update(world: World) -> Callable[..., None]:
    """Build a ``WorldInterface.pre_update_handler`` for :func:`mountain_side_world_factory`.

    Each tick: set all terrain gradients to face +x. Once: turn the ridge voxel into a
    liquid water source so flow can run downslope toward +x / lower terrain.
    """

    w, h, d = world.width, world.height, world.depth
    seeded = False

    def pre(_reg: object, voxel_grid: aetherion.VoxelGrid) -> None:
        nonlocal seeded
        repo = voxel_grid.get_terrain_grid_repository()
        coords = voxel_grid.get_all_terrain_in_region(0, 0, 0, w - 1, h - 1, d - 1)

        g = aetherion.GradientVector()
        g.gx = 1.0
        g.gy = 0.0
        g.gz = 0.0

        for c in coords:
            repo.set_gradient(c.x, c.y, c.z, g)

        if not seeded:
            rx, ry, rz = mountain_ridge_source_xyz(w, h, d)
            if rz >= 0:
                _seed_liquid_water_at(repo, rx, ry, rz)
            seeded = True

    return pre


def empty_square_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    factory = EmptySquareWorldFactory(width=world_width, height=world_height, depth=world_depth)

    # z=0: solid floor
    factory.fill_layer(z=0)

    # z=1: walls around a 6×6 empty center
    mask = [
        [1, 1, 1, 1, 1, 1, 1, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 0, 0, 0, 0, 0, 0, 1],
        [1, 1, 1, 1, 1, 1, 1, 1],
    ]
    factory.apply_mask(z=1, mask=mask, starting_x=46, starting_y=46)

    world = factory.generate_world()
    world.process_ecosystem_async = False

    return world


def dungeon_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    factory = EmptySquareWorldFactory(width=world_width, height=world_height, depth=world_depth)

    # z=0: solid stone floor
    factory.fill_layer(z=0)

    cx = world_width // 2
    cy = world_height // 2

    _ = 0  # empty
    W = 1  # wall
    # fmt: off
    dungeon = [
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, W, W, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, _, _, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, _, _, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W],
        [W, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, W],
        [W, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, W],
        [W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W],
        [W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W],
        [W, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, W],
        [W, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, W],
        [W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W, W],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, _, _, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, _, _, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
        [_, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, W, W, W, W, W, W, W, W, W, W, W, W, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _, _],
    ]
    # fmt: on

    factory.apply_mask(z=1, mask=dungeon, starting_x=cx - 24, starting_y=cy - 24)

    world = factory.generate_world()
    world.process_ecosystem_async = False

    return world
