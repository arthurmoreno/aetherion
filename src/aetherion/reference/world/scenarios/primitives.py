"""Voxel-placement primitives shared by reference scenario worlds and tests.

These helpers materialize a single voxel of a given kind directly into
``TerrainGridRepository`` and the terrain id grid, bypassing ``World.create_entity``
so that no EnTT entity is allocated for terrain. They are the canonical building
blocks for hand-crafted scenarios under :mod:`aetherion.reference.world.scenarios`.
"""

from __future__ import annotations

import aetherion
from aetherion import (
    DirectionEnum,
    EntityEnum,
    EntityTypeComponent,
    MatterContainer,
    MatterState,
    PhysicsStats,
    Position,
    TerrainEnum,
    TerrainIdTypeEnum,
)


def make_position(x: int, y: int, z: int, direction: DirectionEnum = DirectionEnum.DOWN) -> Position:
    """Build a ``Position`` struct (the binding has no positional constructor)."""
    position = Position()
    position.x = x
    position.y = y
    position.z = z
    position.direction = direction
    return position


def make_water_physics_stats() -> PhysicsStats:
    """Default ``PhysicsStats`` for a water voxel."""
    physics_stats = PhysicsStats()
    physics_stats.mass = 20.0
    physics_stats.max_speed = 10.0
    physics_stats.min_speed = 0.0
    physics_stats.force_x = 0.0
    physics_stats.force_y = 0.0
    physics_stats.force_z = 0.0
    return physics_stats


def place_stone(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> None:
    """Materialize an immovable solid (GRASS-typed) voxel at ``(x, y, z)``."""
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.ON_GRID_STORAGE.value)
    repo.set_position(x, y, z, make_position(x, y, z))

    entity_type = EntityTypeComponent()
    entity_type.main_type = EntityEnum.TERRAIN.value
    entity_type.sub_type0 = TerrainEnum.GRASS.value
    entity_type.sub_type1 = 0
    repo.set_terrain_entity_type(x, y, z, entity_type, True)

    matter = MatterContainer()
    matter.terrain_matter = 7
    matter.water_matter = 0
    matter.water_vapor = 0
    matter.bio_mass_matter = 0
    repo.set_terrain_matter_container(x, y, z, matter)

    repo.set_matter_state(x, y, z, MatterState.SOLID)


def place_water(
    voxel_grid: aetherion.VoxelGrid,
    x: int,
    y: int,
    z: int,
    *,
    water_matter: int = 1000,
) -> None:
    """Materialize a liquid-water voxel at ``(x, y, z)`` with no ECS entity."""
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.ON_GRID_STORAGE.value)
    repo.set_position(x, y, z, make_position(x, y, z))

    entity_type = EntityTypeComponent()
    entity_type.main_type = EntityEnum.TERRAIN.value
    entity_type.sub_type0 = TerrainEnum.WATER.value
    entity_type.sub_type1 = 0
    repo.set_terrain_entity_type(x, y, z, entity_type, True)

    matter = MatterContainer()
    matter.terrain_matter = 0
    matter.water_matter = water_matter
    matter.water_vapor = 0
    matter.bio_mass_matter = 0
    repo.set_terrain_matter_container(x, y, z, matter)

    repo.set_matter_state(x, y, z, MatterState.LIQUID)
    repo.set_physics_stats(x, y, z, make_water_physics_stats(), True)


def place_vapor(
    voxel_grid: aetherion.VoxelGrid,
    x: int,
    y: int,
    z: int,
    *,
    water_vapor: int = 1000,
) -> None:
    """Materialize a water-vapor (gaseous) voxel at ``(x, y, z)`` with no ECS entity.

    Same shape as :func:`place_water` but ``WaterVapor`` is non-zero and
    ``WaterMatter`` is zero, mirroring the "only vapor or only water"
    invariant the condensation handler enforces.
    """
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.ON_GRID_STORAGE.value)
    repo.set_position(x, y, z, make_position(x, y, z))

    entity_type = EntityTypeComponent()
    entity_type.main_type = EntityEnum.TERRAIN.value
    entity_type.sub_type0 = TerrainEnum.WATER.value
    entity_type.sub_type1 = 0
    repo.set_terrain_entity_type(x, y, z, entity_type, True)

    matter = MatterContainer()
    matter.terrain_matter = 0
    matter.water_matter = 0
    matter.water_vapor = water_vapor
    matter.bio_mass_matter = 0
    repo.set_terrain_matter_container(x, y, z, matter)

    repo.set_matter_state(x, y, z, MatterState.GAS)
    repo.set_physics_stats(x, y, z, make_water_physics_stats(), True)


def place_empty(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> None:
    """Force ``(x, y, z)`` to be a fully empty cell (terrain id ``NONE``).

    Useful when a higher-level factory pre-populated the cell and the scenario
    needs to re-assert the empty invariant (or the tests want a deliberately
    transitional state).
    """
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.NONE.value)

    matter = MatterContainer()
    matter.terrain_matter = 0
    matter.water_matter = 0
    matter.water_vapor = 0
    matter.bio_mass_matter = 0
    repo.set_terrain_matter_container(x, y, z, matter)


def water_matter(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> int:
    """Read ``WaterMatter`` at ``(x, y, z)`` from the repository."""
    matter = voxel_grid.get_terrain_matter_container_component(x, y, z)
    return int(matter.water_matter)


def water_vapor(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> int:
    """Read ``WaterVapor`` at ``(x, y, z)`` from the repository."""
    matter = voxel_grid.get_terrain_matter_container_component(x, y, z)
    return int(matter.water_vapor)


def fall_event_position(x: int, y: int, z: int) -> Position:
    """Build a ``Position`` for use in ``World.dispatch_water_fall_event``."""
    return make_position(x, y, z)
