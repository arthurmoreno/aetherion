"""Reference world helpers (fixtures, minimal terrain types, demo-aligned factories)."""

from aetherion.reference.systems.spring_water import SpringWaterSystem

from . import scenarios
from .factory import (
    dungeon_world_factory,
    empty_square_world_factory,
    make_mountain_side_pre_update,
    mountain_ridge_source_xyz,
    mountain_side_world_factory,
    pilar_world_factory,
    pyramid_world_factory,
)
from .scenarios import (
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
    place_water,
    water_gravity_flow_world_factory,
    water_matter,
)
from .terrains import ReferenceGrass
from .world_factories import (
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
    "mountain_side_world_factory",
    "pilar_world_factory",
    "pyramid_world_factory",
    "trapezium_column_top_z",
    "SpringWaterSystem",
    "scenarios",
    # scenario primitives + C2 constants/factory
    "fall_event_position",
    "place_empty",
    "place_stone",
    "place_water",
    "water_matter",
    "GRAVITY_FLOW_AMOUNT",
    "GRAVITY_FLOW_INITIAL_WATER_MATTER",
    "GRAVITY_FLOW_SOURCE_POS",
    "GRAVITY_FLOW_TARGET_POS",
    "GRAVITY_FLOW_WORLD_DEPTH",
    "GRAVITY_FLOW_WORLD_HEIGHT",
    "GRAVITY_FLOW_WORLD_WIDTH",
    "water_gravity_flow_world_factory",
]
