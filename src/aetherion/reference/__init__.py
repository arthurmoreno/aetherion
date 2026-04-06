"""Reference implementations for engine simulation contracts, tests, and tutorials.

These types are stable, minimal wiring around core components. They are not prescriptive
for shipping game content—games may define their own terrain classes and enums.

World factories mirror the minimal rogue dungeon demo (pillar, pyramid, masked square,
dungeon layout) using :class:`ReferenceGrass` and set ``process_ecosystem_async = False``.
"""

from aetherion.reference.systems.spring_water import SpringWaterSystem
from aetherion.reference.world.factory import (
    dungeon_world_factory,
    empty_square_world_factory,
    make_mountain_side_pre_update,
    mountain_ridge_source_xyz,
    mountain_side_world_factory,
    pilar_world_factory,
    pyramid_world_factory,
)
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
    "mountain_side_world_factory",
    "pilar_world_factory",
    "pyramid_world_factory",
    "trapezium_column_top_z",
    "SpringWaterSystem",
]
