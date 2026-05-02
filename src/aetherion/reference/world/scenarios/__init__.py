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
    place_vapor,
    place_water,
    water_matter,
    water_vapor,
)
from aetherion.reference.world.scenarios.water_condensation import (
    CONDENSATION_AMOUNT as CONDENSATION_AMOUNT,
)
from aetherion.reference.world.scenarios.water_condensation import (
    INITIAL_VAPOR as CONDENSATION_INITIAL_VAPOR,
)
from aetherion.reference.world.scenarios.water_condensation import (
    TARGET_POS as CONDENSATION_TARGET_POS,
)
from aetherion.reference.world.scenarios.water_condensation import (
    VAPOR_POS as CONDENSATION_VAPOR_POS,
)
from aetherion.reference.world.scenarios.water_condensation import (
    WORLD_DEPTH as CONDENSATION_WORLD_DEPTH,
)
from aetherion.reference.world.scenarios.water_condensation import (
    WORLD_HEIGHT as CONDENSATION_WORLD_HEIGHT,
)
from aetherion.reference.world.scenarios.water_condensation import (
    WORLD_WIDTH as CONDENSATION_WORLD_WIDTH,
)
from aetherion.reference.world.scenarios.water_condensation import (
    water_condense_below_vapor_world_factory,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    INITIAL_WATER_MATTER as MIDAIR_INITIAL_WATER_MATTER,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    WATER_POS as MIDAIR_WATER_POS,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    WORLD_DEPTH as MIDAIR_WORLD_DEPTH,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    WORLD_HEIGHT as MIDAIR_WORLD_HEIGHT,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    WORLD_WIDTH as MIDAIR_WORLD_WIDTH,
)
from aetherion.reference.world.scenarios.water_fall_from_midair import (
    water_column_above_floor_world_factory,
    water_fall_from_midair_world_factory,
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
    "place_vapor",
    "place_water",
    "water_matter",
    "water_vapor",
    # gravity-flow scenario
    "GRAVITY_FLOW_AMOUNT",
    "GRAVITY_FLOW_INITIAL_WATER_MATTER",
    "GRAVITY_FLOW_SOURCE_POS",
    "GRAVITY_FLOW_TARGET_POS",
    "GRAVITY_FLOW_WORLD_DEPTH",
    "GRAVITY_FLOW_WORLD_HEIGHT",
    "GRAVITY_FLOW_WORLD_WIDTH",
    "water_gravity_flow_world_factory",
    # condensation scenario
    "CONDENSATION_AMOUNT",
    "CONDENSATION_INITIAL_VAPOR",
    "CONDENSATION_TARGET_POS",
    "CONDENSATION_VAPOR_POS",
    "CONDENSATION_WORLD_DEPTH",
    "CONDENSATION_WORLD_HEIGHT",
    "CONDENSATION_WORLD_WIDTH",
    "water_condense_below_vapor_world_factory",
    # mid-air falling-water scenario
    "MIDAIR_INITIAL_WATER_MATTER",
    "MIDAIR_WATER_POS",
    "MIDAIR_WORLD_DEPTH",
    "MIDAIR_WORLD_HEIGHT",
    "MIDAIR_WORLD_WIDTH",
    "water_column_above_floor_world_factory",
    "water_fall_from_midair_world_factory",
]
