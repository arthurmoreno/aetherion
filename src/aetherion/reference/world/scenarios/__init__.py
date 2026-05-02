"""Hand-crafted minimal worlds shared by tests and the live game engine.

Each scenario is a small, fixed world layout that exercises one specific
physics path (water fall, gravity flow, spread, vapor condense, ...). The
factory functions returned here can be:

- registered with :class:`WorldManager` and loaded into the live game for
  visual / experiential validation, or
- consumed directly by tests that build their own ``WorldManager`` wrapper
  and dispatch events at known coordinates.

The :mod:`primitives` submodule provides the voxel-placement helpers
(``place_stone``, ``place_water``, ``place_empty``, ``water_matter``, ...)
that scenarios and tests both build on.
"""

from aetherion.reference.world.scenarios.primitives import (
    fall_event_position,
    make_position,
    make_water_physics_stats,
    place_empty,
    place_stone,
    place_water,
    water_matter,
)
from aetherion.reference.world.scenarios.water_gravity_flow import GRAVITY_FLOW_AMOUNT, water_gravity_flow_world_factory
from aetherion.reference.world.scenarios.water_gravity_flow import (
    INITIAL_WATER_MATTER as GRAVITY_FLOW_INITIAL_WATER_MATTER,
)
from aetherion.reference.world.scenarios.water_gravity_flow import SOURCE_POS as GRAVITY_FLOW_SOURCE_POS
from aetherion.reference.world.scenarios.water_gravity_flow import TARGET_POS as GRAVITY_FLOW_TARGET_POS
from aetherion.reference.world.scenarios.water_gravity_flow import WORLD_DEPTH as GRAVITY_FLOW_WORLD_DEPTH
from aetherion.reference.world.scenarios.water_gravity_flow import WORLD_HEIGHT as GRAVITY_FLOW_WORLD_HEIGHT
from aetherion.reference.world.scenarios.water_gravity_flow import WORLD_WIDTH as GRAVITY_FLOW_WORLD_WIDTH

__all__ = [
    # primitives
    "fall_event_position",
    "make_position",
    "make_water_physics_stats",
    "place_empty",
    "place_stone",
    "place_water",
    "water_matter",
    # gravity-flow scenario
    "GRAVITY_FLOW_AMOUNT",
    "GRAVITY_FLOW_INITIAL_WATER_MATTER",
    "GRAVITY_FLOW_SOURCE_POS",
    "GRAVITY_FLOW_TARGET_POS",
    "GRAVITY_FLOW_WORLD_DEPTH",
    "GRAVITY_FLOW_WORLD_HEIGHT",
    "GRAVITY_FLOW_WORLD_WIDTH",
    "water_gravity_flow_world_factory",
]
