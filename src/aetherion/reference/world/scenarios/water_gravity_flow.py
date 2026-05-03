"""Minimal water-gravity-flow scenario: water source above an empty cell.

3x3x3 empty world. A liquid water source sits at ``(1, 1, 1)`` and the cell
directly below at ``(1, 1, 0)`` is genuinely empty (terrain id ``NONE``).
Dispatching a ``WaterGravityFlowEvent`` from the source to the destination
should drain the source by ``GRAVITY_FLOW_AMOUNT`` and create a water voxel at
the destination as ``ON_GRID_STORAGE`` (no EnTT entity).

This scenario is shared between:

- Tests under ``tests/reference/`` — drive the gravity-flow handler directly
  via ``World.dispatch_water_gravity_flow_event`` and assert the destination's
  terrain id and the conservation invariant.
- The live game engine — load this scenario via :class:`WorldManager` to
  visually confirm that gravity flow lands water at ``ON_GRID_STORAGE`` without
  growing the registry.

Both consumers should reuse the coordinate / amount constants exposed at
module level so a single source of truth defines what success looks like.
"""

from __future__ import annotations

import aetherion
from aetherion.reference.systems.spring_water import SpringWaterSystem
from aetherion.reference.world.scenarios.primitives import place_water
from aetherion.reference.world.world_factories import EmptySquareWorldFactory

WORLD_WIDTH = 3
WORLD_HEIGHT = 3
WORLD_DEPTH = 3

# A water source sits one cell above an empty destination. The destination
# stays at NONE — the ``EmptySquareWorldFactory`` default — until the
# gravity-flow handler creates the new voxel.
SOURCE_POS: tuple[int, int, int] = (1, 1, 1)
TARGET_POS: tuple[int, int, int] = (1, 1, 0)
INITIAL_WATER_MATTER: int = 3
GRAVITY_FLOW_AMOUNT: int = 3


def water_gravity_flow_world_factory_3x3x3(world_config: dict[str, int]) -> aetherion.World:
    """Build the gravity-flow-into-empty-cell scenario world.

    The ``world_config`` dimensions are ignored — this scenario is
    intentionally fixed at 3x3x3 so coordinates stay stable across runs.
    Other config keys (``gravity``, ``friction``, ...) are accepted for
    parity with the other reference factories but do not affect the layout.
    """
    del world_config  # layout is fixed; config is consumed by WorldManager metadata only
    factory = EmptySquareWorldFactory(width=WORLD_WIDTH, height=WORLD_HEIGHT, depth=WORLD_DEPTH)

    world = factory.generate_world()
    world.process_ecosystem_async = True
    world.water_auto_balancing = False
    world.simulate_water_movement = True
    world.simulate_water_evaporation = True
    world.simulate_vapor_movement = True
    world.simulate_vapor_condensation = True

    voxel_grid = world.get_voxel_grid()
    place_water(voxel_grid, *SOURCE_POS, water_matter=INITIAL_WATER_MATTER)

    return world


def water_gravity_flow_world_factory(world_config: dict[str, int]) -> aetherion.World:
    """Build the gravity-flow-into-empty-cell scenario world.

    The ``world_config`` dimensions are ignored — this scenario is
    intentionally fixed at 3x3x3 so coordinates stay stable across runs.
    Other config keys (``gravity``, ``friction``, ...) are accepted for
    parity with the other reference factories but do not affect the layout.
    """
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)
    factory = EmptySquareWorldFactory(width=world_width, height=world_height, depth=world_depth)

    # z=0: solid floor
    factory.fill_layer(z=0)

    world = factory.generate_world()

    world.process_ecosystem_async = True
    world.water_auto_balancing = False
    world.simulate_water_movement = True
    world.simulate_water_evaporation = True
    world.simulate_vapor_movement = True
    world.simulate_vapor_condensation = True

    rx = world_width // 2
    ry = world_height // 2
    rz = world_depth - 1
    spring_pace: int = world_config.get("spring_pace", 5)
    world.add_python_system(SpringWaterSystem(world=world, pace=spring_pace, source_x=rx, source_y=ry, source_z=rz))

    return world
