"""Tests for reference world factories (demo-aligned layouts)."""

from __future__ import annotations

from aetherion.reference.world.factory import pilar_world_factory, pyramid_world_factory


def test_reference_factories_sync_flag_and_dimensions() -> None:
    cfg = {"world_width": 24, "world_height": 24, "world_depth": 8}
    for factory in (pilar_world_factory, pyramid_world_factory):
        world = factory(cfg)
        assert world.process_ecosystem_async is False
        assert world.width == 24
        assert world.height == 24
        assert world.depth == 8


# def test_pilar_world_spawns_reference_grass_terrain() -> None:
#     world = pilar_world_factory({"world_width": 24, "world_height": 24, "world_depth": 8})
#     # Terrain has no PerceptionComponent; get_entity_ids_by_type only lists entities with both.
#     terrains = world.get_entities_by_type(EntityEnum.TERRAIN.value, TerrainEnum.GRASS.value)
#     assert len(terrains) > 0


# def test_empty_square_world_factory_large_enough_for_demo_mask() -> None:
#     cfg = {"world_width": 64, "world_height": 64, "world_depth": 6}
#     world = empty_square_world_factory(cfg)
#     assert world.process_ecosystem_async is False
#     terrains = world.get_entities_by_type(EntityEnum.TERRAIN.value, TerrainEnum.GRASS.value)
#     assert len(terrains) > 0


# def test_dungeon_world_factory_spawns_walls() -> None:
#     cfg = {"world_width": 64, "world_height": 64, "world_depth": 6}
#     world = dungeon_world_factory(cfg)
#     assert world.process_ecosystem_async is False
#     terrains = world.get_entities_by_type(EntityEnum.TERRAIN.value, TerrainEnum.GRASS.value)
#     assert len(terrains) > 0
