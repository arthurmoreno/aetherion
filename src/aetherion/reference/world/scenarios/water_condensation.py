"""Minimal water-condensation-below-vapor scenario.

3x3x3 empty world. A vapor cell sits at ``VAPOR_POS = (1, 1, 1)`` with
``INITIAL_VAPOR`` units of ``WaterVapor`` and zero ``WaterMatter``. The cell
directly below at ``TARGET_POS = (1, 1, 0)`` is genuinely empty (terrain id
``NONE``). Dispatching a ``CondenseWaterEntityEvent`` for the vapor cell with
``terrainBelowId = NONE`` triggers ``createWaterTerrainBelowVapor``: the
``CONDENSATION_AMOUNT`` units transfer from the vapor cell to a newly
created liquid water voxel at ``TARGET_POS``, which must end as
``ON_GRID_STORAGE`` (terrain id ``-1``) — no EnTT entity allocated.

``INITIAL_VAPOR > CONDENSATION_AMOUNT`` so the vapor cell does **not** fully
deplete during the event. The vapor-depletion cleanup branch in
``createWaterTerrainBelowVapor`` (which calls ``voxelGrid.setTerrain(..., NONE)``
and would currently throw because of the latent ``setTerrain`` rejection
check at ``VoxelGrid.cpp:104``) is therefore not exercised by this scenario.
That cleanup-branch bug is tracked separately and out of scope for the C3
``registry.create()`` removal this scenario covers.

Shared between:

- Tests under ``tests/reference/test_water_condensation.py`` — drive the
  condensation handler directly via
  ``World.dispatch_condense_water_event`` and assert the destination's
  terrain id and the conservation invariant.
- The live game engine — load this scenario via :class:`WorldManager` to
  visually confirm vapor → liquid condensation lands water at
  ``ON_GRID_STORAGE`` without growing the registry.
"""

from __future__ import annotations

import aetherion
from aetherion.reference.world.scenarios.primitives import place_vapor
from aetherion.reference.world.world_factories import EmptySquareWorldFactory

WORLD_WIDTH = 3
WORLD_HEIGHT = 3
WORLD_DEPTH = 3

VAPOR_POS: tuple[int, int, int] = (1, 1, 1)
TARGET_POS: tuple[int, int, int] = (1, 1, 0)
INITIAL_VAPOR: int = 5
CONDENSATION_AMOUNT: int = 3


def water_condense_below_vapor_world_factory(
    world_config: dict[str, int],
) -> aetherion.World:
    """Build the condensation-below-vapor scenario world.

    The ``world_config`` dimensions are ignored — this scenario is fixed at
    3x3x3 so coordinates stay stable across runs. Other config keys
    (``gravity``, ``friction``, ...) are accepted for parity with other
    reference factories but do not affect the layout.
    """
    del world_config
    factory = EmptySquareWorldFactory(width=WORLD_WIDTH, height=WORLD_HEIGHT, depth=WORLD_DEPTH)

    world = factory.generate_world()
    world.process_ecosystem = False

    voxel_grid = world.get_voxel_grid()
    place_vapor(voxel_grid, *VAPOR_POS, water_vapor=INITIAL_VAPOR)

    return world
