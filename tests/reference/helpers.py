from __future__ import annotations

from typing import Any

from conftest import FakeEventBus

import aetherion
from aetherion import (
    DirectionEnum,
    EntityEnum,
    EntityTypeComponent,
    GameEventType,
    MatterContainer,
    MatterState,
    PhysicsStats,
    Position,
    TerrainEnum,
    TerrainIdTypeEnum,
    WorldInterfaceMetadata,
)
from aetherion.reference.world.factory import (
    empty_square_world_factory,
    mountain_side_world_factory,
)
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.manager import WorldManager

MOUNTAIN_SIDE_WORLD_CONFIG: dict[str, Any] = {
    "type": WorldInstanceTypes.SYNCHRONOUS,
    "world_seed": "mountain-side-water-regression",
    "world_width": 100,
    "world_height": 100,
    "world_depth": 10,
    "gravity": 5.0,
    "friction": 1.0,
    "allow_multi_direction": True,
    "evaporation_coefficient": 8.0,
    "heat_to_water_evaporation": 120.0,
    "water_minimum_units": 30000,
    "metabolism_cost_to_apply_force": 1.9999999949504854e-06,
    "spring_pace": 5,
}

MINIMAL_WORLD_CONFIG: dict[str, Any] = {
    "type": WorldInstanceTypes.SYNCHRONOUS,
    "world_seed": "reference-minimal-world",
    "gravity": 5.0,
    "friction": 1.0,
    "allow_multi_direction": True,
    "evaporation_coefficient": 8.0,
    "heat_to_water_evaporation": 120.0,
    "water_minimum_units": 30000,
    "metabolism_cost_to_apply_force": 1.9999999949504854e-06,
    "spring_pace": 5,
}


def build_mountain_side_manager(world_name: str) -> WorldManager:
    manager = WorldManager(
        event_bus=FakeEventBus(),
        event_handlers={GameEventType.WORLD_CREATED: None},
    )
    manager.register_factory("default", mountain_side_world_factory)

    world_key = world_name.lower().replace(" ", "_")
    manager.worlds_metadata[world_key] = WorldInterfaceMetadata(
        key=world_key,
        name=world_name,
        description="",
        status="running",
    )

    manager.load_world(
        world_name=world_name,
        world_factory_name="default",
        world_config=dict(MOUNTAIN_SIDE_WORLD_CONFIG),
    )

    manager.current_key = world_key
    manager.current = manager.worlds[world_key]
    manager.current_metadata = manager.worlds_metadata[world_key]
    return manager


def build_minimal_test_manager(
    width: int,
    height: int,
    depth: int,
    *,
    gravity: float = 5.0,
    friction: float = 1.0,
) -> WorldManager:
    manager = WorldManager(
        event_bus=FakeEventBus(),
        event_handlers={GameEventType.WORLD_CREATED: None},
    )
    manager.register_factory("default", empty_square_world_factory)

    world_name = f"Minimal Reference {width}x{height}x{depth}"
    world_key = world_name.lower().replace(" ", "_")
    manager.worlds_metadata[world_key] = WorldInterfaceMetadata(
        key=world_key,
        name=world_name,
        description="",
        status="running",
    )

    world_config = dict(MINIMAL_WORLD_CONFIG)
    world_config.update(
        {
            "world_width": width,
            "world_height": height,
            "world_depth": depth,
            "gravity": gravity,
            "friction": friction,
        }
    )

    manager.load_world(
        world_name=world_name,
        world_factory_name="default",
        world_config=world_config,
    )

    manager.current_key = world_key
    manager.current = manager.worlds[world_key]
    manager.current_metadata = manager.worlds_metadata[world_key]
    return manager


def _make_position(x: int, y: int, z: int) -> Position:
    position = Position()
    position.x = x
    position.y = y
    position.z = z
    position.direction = DirectionEnum.DOWN
    return position


def _make_water_physics_stats() -> PhysicsStats:
    physics_stats = PhysicsStats()
    physics_stats.mass = 20.0
    physics_stats.max_speed = 10.0
    physics_stats.min_speed = 0.0
    physics_stats.force_x = 0.0
    physics_stats.force_y = 0.0
    physics_stats.force_z = 0.0
    return physics_stats


def place_stone(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> None:
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.ON_GRID_STORAGE.value)
    repo.set_position(x, y, z, _make_position(x, y, z))

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


def fall_event_position(x: int, y: int, z: int) -> Position:
    """Build a Position struct for use in WaterFallEntityEvent dispatch."""
    return _make_position(x, y, z)


def water_matter(voxel_grid: aetherion.VoxelGrid, x: int, y: int, z: int) -> int:
    """Read WaterMatter at (x, y, z) from the terrain grid repository."""
    matter = voxel_grid.get_terrain_matter_container_component(x, y, z)
    return int(matter.water_matter)


def place_water(
    voxel_grid: aetherion.VoxelGrid,
    x: int,
    y: int,
    z: int,
    *,
    water_matter: int = 1000,
) -> None:
    repo = voxel_grid.terrain_grid_repository
    voxel_grid.set_terrain_id_raw(x, y, z, TerrainIdTypeEnum.ON_GRID_STORAGE.value)
    repo.set_position(x, y, z, _make_position(x, y, z))

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
    repo.set_physics_stats(x, y, z, _make_water_physics_stats(), True)
