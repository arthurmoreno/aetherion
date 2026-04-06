"""Reference world helpers (fixtures, minimal terrain types, demo-aligned factories)."""

from aetherion.reference.systems.spring_water import SpringWaterSystem

from .factory import (
    dungeon_world_factory,
    empty_square_world_factory,
    make_mountain_side_pre_update,
    mountain_ridge_source_xyz,
    mountain_side_world_factory,
    pilar_world_factory,
    pyramid_world_factory,
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
]
