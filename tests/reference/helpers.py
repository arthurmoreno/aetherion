from __future__ import annotations

from typing import Any

from conftest import FakeEventBus

from aetherion import GameEventType, WorldInterfaceMetadata
from aetherion.reference.world.factory import mountain_side_world_factory
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
