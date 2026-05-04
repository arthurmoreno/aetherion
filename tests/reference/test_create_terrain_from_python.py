"""Task G — `World.create_entity` three-path dispatcher contract.

  * Plain terrain — no entity; terrain grid stores ON_GRID_STORAGE (-1).
  * Hybrid terrain (Inventory / TileEffectsList) — entity + VDB dual-write;
    terrain grid stores the entity int.
  * Non-terrain entity — legacy path; terrain grid untouched (NONE = -2),
    entity grid stores the entity int.

No public "alive entity count" binding exists; following
``test_vapor_creation_event.py``, we verify ECS allocation indirectly via
the terrain-grid sentinel at the new voxel.
"""

from __future__ import annotations

from helpers import build_minimal_test_manager

import aetherion
from aetherion import (
    DirectionEnum,
    EntityEnum,
    EntityTypeComponent,
    GridType,
    Inventory,
    MatterContainer,
    MatterState,
    PhysicsStats,
    Position,
    StructuralIntegrityComponent,
    TerrainEnum,
    TerrainIdTypeEnum,
)
from aetherion.entities import BaseEntity

# Fixtures — inline, test-specific.


def _terrain_state(
    x: int,
    y: int,
    z: int,
    *,
    sub_type0: int = TerrainEnum.GRASS.value,
    terrain_matter: int = 7,
    can_stack: bool = True,
) -> tuple[
    Position,
    EntityTypeComponent,
    StructuralIntegrityComponent,
    MatterContainer,
    PhysicsStats,
]:
    pos = Position()
    pos.x = x
    pos.y = y
    pos.z = z
    pos.direction = DirectionEnum.DOWN

    entity_type = EntityTypeComponent()
    entity_type.main_type = EntityEnum.TERRAIN.value
    entity_type.sub_type0 = sub_type0
    entity_type.sub_type1 = 0

    sic = StructuralIntegrityComponent()
    sic.can_stack_entities = can_stack
    sic.max_load_capacity = -1
    sic.matter_state = MatterState.SOLID

    mc = MatterContainer()
    mc.terrain_matter = terrain_matter
    mc.water_matter = 0
    mc.water_vapor = 0
    mc.bio_mass_matter = 0

    ps = PhysicsStats()
    ps.mass = 1.0
    ps.max_speed = 0.0
    ps.min_speed = 0.0

    return pos, entity_type, sic, mc, ps


class _PlainGrass(BaseEntity):
    """TERRAIN, no inventory / tile_effects_list — classifies as plain."""

    grid_type = GridType.TERRAIN

    def __init__(self, x: int, y: int, z: int) -> None:
        BaseEntity.__init__(self, x, y, z)
        pos, entity_type, sic, mc, ps = _terrain_state(x, y, z)
        self.position = pos
        self.entity_type = entity_type
        self.structural_integrity = sic
        self.matter_container = mc
        self.physics_stats = ps


class _ChestWithInventory(BaseEntity):
    """TERRAIN + non-None Inventory — classifies as hybrid."""

    grid_type = GridType.TERRAIN

    def __init__(self, x: int, y: int, z: int) -> None:
        BaseEntity.__init__(self, x, y, z)
        pos, entity_type, sic, mc, ps = _terrain_state(x, y, z)
        self.position = pos
        self.entity_type = entity_type
        self.structural_integrity = sic
        self.matter_container = mc
        self.physics_stats = ps
        self.inventory = Inventory()


class _Beast(BaseEntity):
    """ENTITY-typed — classifies as non-terrain (legacy path)."""

    grid_type = GridType.ENTITY

    def __init__(self, x: int, y: int, z: int) -> None:
        BaseEntity.__init__(self, x, y, z)
        pos = Position()
        pos.x = x
        pos.y = y
        pos.z = z
        pos.direction = DirectionEnum.DOWN
        self.position = pos

        entity_type = EntityTypeComponent()
        entity_type.main_type = EntityEnum.BEAST.value
        entity_type.sub_type0 = 0
        entity_type.sub_type1 = 0
        self.entity_type = entity_type


def _build_world() -> aetherion.World:
    manager = build_minimal_test_manager(8, 8, 4)
    return manager.current.world


# ─────────────────────────────────────────────────────────────────────────────
# Tests — terrain-grid sentinel pattern (no `alive_entity_count` binding
# needed; matches `test_vapor_creation_event.py` style).
# ─────────────────────────────────────────────────────────────────────────────


def test_plain_terrain_lands_as_on_grid_storage() -> None:
    """Path 1: plain terrain → ON_GRID_STORAGE; no entity allocated.

    z=2 is above the factory's z=0 floor (matches `test_vapor_creation_event.py`).
    """
    world = _build_world()
    voxel_grid = world.get_voxel_grid()

    assert voxel_grid.get_terrain(2, 2, 2) == TerrainIdTypeEnum.NONE.value

    world.create_entity(_PlainGrass(2, 2, 2))

    tid = voxel_grid.get_terrain(2, 2, 2)
    assert tid == TerrainIdTypeEnum.ON_GRID_STORAGE.value, (
        f"any value other than -1 means an EnTT entity was created; got tid={tid}"
    )


def test_plain_terrain_components_in_vdb() -> None:
    """Path 1: plain-terrain components readable via VDB-backed getters."""
    world = _build_world()
    world.create_entity(_PlainGrass(3, 3, 2))

    voxel_grid = world.get_voxel_grid()
    et = voxel_grid.get_terrain_entity_type_component(3, 3, 2)
    assert et.main_type == EntityEnum.TERRAIN.value
    assert et.sub_type0 == TerrainEnum.GRASS.value


def test_hybrid_terrain_stores_entity_int_in_terrain_grid() -> None:
    """Path 2: hybrid terrain stores entity int in the terrain grid AND
    dual-writes terrain-state to VDB."""
    world = _build_world()
    voxel_grid = world.get_voxel_grid()

    assert voxel_grid.get_terrain(4, 4, 2) == TerrainIdTypeEnum.NONE.value

    entity = world.create_entity(_ChestWithInventory(4, 4, 2))
    assert entity is not None

    tid = voxel_grid.get_terrain(4, 4, 2)
    # Positive int (not -1, not -2) ⇒ entity created and its handle written.
    assert tid != TerrainIdTypeEnum.ON_GRID_STORAGE.value
    assert tid != TerrainIdTypeEnum.NONE.value
    assert tid == entity.get_id(), f"terrain grid should hold entity id={entity.get_id()}; got {tid}"

    # Inventory readable from the registry.
    inventory_obj = world.get_py_registry().get_component(entity.get_id(), "Inventory")
    assert inventory_obj is not None

    # Terrain-state also in VDB (the dual-write).
    et_vdb = voxel_grid.get_terrain_entity_type_component(4, 4, 2)
    assert et_vdb.main_type == EntityEnum.TERRAIN.value


def test_non_terrain_entity_path_unchanged() -> None:
    """Path 3: ENTITY-typed entities → terrain grid untouched, entity grid
    holds the entity int."""
    world = _build_world()
    voxel_grid = world.get_voxel_grid()

    assert voxel_grid.get_terrain(5, 5, 2) == TerrainIdTypeEnum.NONE.value

    entity = world.create_entity(_Beast(5, 5, 2))
    assert entity is not None

    # Terrain grid untouched; entity grid holds the int.
    assert voxel_grid.get_terrain(5, 5, 2) == TerrainIdTypeEnum.NONE.value
    assert world.get_entity(5, 5, 2) == entity.get_id()
