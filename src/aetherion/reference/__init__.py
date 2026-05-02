"""Reference implementations for engine simulation contracts, tests, and tutorials.

These types are stable, minimal wiring around core components. They are not prescriptive
for shipping game content—games may define their own terrain classes and enums.

World factories mirror the minimal rogue dungeon demo (pillar, pyramid, masked square,
dungeon layout) using :class:`ReferenceGrass` and set ``process_ecosystem_async = False``.
"""

from aetherion.reference.systems.spring_water import SpringWaterSystem
from aetherion.reference.world import scenarios
from aetherion.reference.world.factory import (
    dungeon_world_factory,
    empty_square_world_factory,
    make_mountain_side_pre_update,
    mountain_ridge_source_xyz,
    pilar_world_factory,
    pyramid_world_factory,
)
from aetherion.reference.world.scenarios import (
    CONDENSATION_AMOUNT,
    CONDENSATION_INITIAL_VAPOR,
    CONDENSATION_TARGET_POS,
    CONDENSATION_VAPOR_POS,
    CONDENSATION_WORLD_DEPTH,
    CONDENSATION_WORLD_HEIGHT,
    CONDENSATION_WORLD_WIDTH,
    GRAVITY_FLOW_AMOUNT,
    GRAVITY_FLOW_INITIAL_WATER_MATTER,
    GRAVITY_FLOW_SOURCE_POS,
    GRAVITY_FLOW_TARGET_POS,
    GRAVITY_FLOW_WORLD_DEPTH,
    GRAVITY_FLOW_WORLD_HEIGHT,
    GRAVITY_FLOW_WORLD_WIDTH,
    fall_event_position,
    place_empty,
    place_stone,
    place_vapor,
    place_water,
    water_condense_below_vapor_world_factory,
    water_gravity_flow_world_factory,
    water_matter,
    water_vapor,
)
from aetherion.reference.world.scenarios.water_mountain_side_world import mountain_side_stream_world_factory
from aetherion.reference.world.terrains import ReferenceGrass
from aetherion.reference.world.world_factories import (
    EmptySquareWorldFactory,
    MountainSideWorldFactory,
    PilarWorldFactory,
    PyramidWorldFactory,
    trapezium_column_top_z,
)

__all__ = [
    "ReferenceGrass",
    "EmptySquareWorldFactory",
    "MountainSideWorldFactory",
    "PilarWorldFactory",
    "PyramidWorldFactory",
    "dungeon_world_factory",
    "empty_square_world_factory",
    "make_mountain_side_pre_update",
    "mountain_ridge_source_xyz",
    "mountain_side_stream_world_factory",
    "pilar_world_factory",
    "pyramid_world_factory",
    "trapezium_column_top_z",
    "SpringWaterSystem",
    "scenarios",
    "fall_event_position",
    "place_empty",
    "place_stone",
    "place_vapor",
    "place_water",
    "water_matter",
    "water_vapor",
    "GRAVITY_FLOW_AMOUNT",
    "GRAVITY_FLOW_INITIAL_WATER_MATTER",
    "GRAVITY_FLOW_SOURCE_POS",
    "GRAVITY_FLOW_TARGET_POS",
    "GRAVITY_FLOW_WORLD_DEPTH",
    "GRAVITY_FLOW_WORLD_HEIGHT",
    "GRAVITY_FLOW_WORLD_WIDTH",
    "water_gravity_flow_world_factory",
    "CONDENSATION_AMOUNT",
    "CONDENSATION_INITIAL_VAPOR",
    "CONDENSATION_TARGET_POS",
    "CONDENSATION_VAPOR_POS",
    "CONDENSATION_WORLD_DEPTH",
    "CONDENSATION_WORLD_HEIGHT",
    "CONDENSATION_WORLD_WIDTH",
    "water_condense_below_vapor_world_factory",
]
