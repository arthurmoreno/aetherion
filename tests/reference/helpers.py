"""WorldManager glue for reference-test fixtures.

Voxel-placement primitives (``place_stone``, ``place_water``, ``place_empty``,
``water_matter``, ``fall_event_position``) live in
``aetherion.reference.world.scenarios.primitives`` so the live game engine and
tests share the same building blocks. They are re-exported here so existing
tests that import directly from ``helpers`` keep working.

Hand-crafted scenario world factories live alongside the primitives in
``aetherion.reference.world.scenarios``. Use them via ``WorldManager`` like any
other reference world.
"""

from __future__ import annotations

from typing import Any, Callable

from conftest import FakeEventBus

from aetherion import GameEventType, WorldInterfaceMetadata
from aetherion.reference.world.factory import empty_square_world_factory
from aetherion.reference.world.scenarios.primitives import (
    fall_event_position,
    make_position,
    make_water_physics_stats,
    place_empty,
    place_stone,
    place_vapor,
    place_water,
    water_matter,
    water_vapor,
)
from aetherion.reference.world.scenarios.water_mountain_side_world import mountain_side_stream_world_factory
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.manager import WorldManager

__all__ = [
    "MOUNTAIN_SIDE_WORLD_CONFIG",
    "MINIMAL_WORLD_CONFIG",
    "build_mountain_side_manager",
    "build_minimal_test_manager",
    "build_scenario_manager",
    # re-exported primitives
    "fall_event_position",
    "make_position",
    "make_water_physics_stats",
    "place_empty",
    "place_stone",
    "place_vapor",
    "place_water",
    "water_matter",
    "water_vapor",
]

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
    manager.register_factory("default", mountain_side_stream_world_factory)

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
    # `WorldManager.load_world` resets status to "ready", which makes
    # `manager.update()` a no-op. Tests built on this helper expect the
    # simulation to actually tick — flip it to "running" before returning.
    manager.current_metadata.status = "running"
    return manager


def build_scenario_manager(
    scenario_factory: Callable[[dict[str, Any]], Any],
    world_name: str,
    *,
    config_overrides: dict[str, Any] | None = None,
) -> WorldManager:
    """Wrap any reference-scenario factory in a runnable ``WorldManager``.

    The factory function must accept a ``world_config`` dict and return a
    populated ``World``. World dimensions are owned by the scenario itself —
    this helper only forwards the canonical ``MINIMAL_WORLD_CONFIG`` (with
    optional overrides) for the WorldManager metadata.
    """
    manager = WorldManager(
        event_bus=FakeEventBus(),
        event_handlers={GameEventType.WORLD_CREATED: None},
    )
    manager.register_factory("default", scenario_factory)

    world_key = world_name.lower().replace(" ", "_")
    manager.worlds_metadata[world_key] = WorldInterfaceMetadata(
        key=world_key,
        name=world_name,
        description="",
        status="running",
    )

    world_config = dict(MINIMAL_WORLD_CONFIG)
    if config_overrides:
        world_config.update(config_overrides)

    manager.load_world(
        world_name=world_name,
        world_factory_name="default",
        world_config=world_config,
    )

    manager.current_key = world_key
    manager.current = manager.worlds[world_key]
    manager.current_metadata = manager.worlds_metadata[world_key]
    manager.current_metadata.status = "running"
    return manager
