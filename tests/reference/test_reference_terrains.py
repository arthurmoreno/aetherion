"""Tests for reference terrain entities (simulation contract)."""

from aetherion import DirectionEnum, EntityEnum, MatterState, TerrainEnum, TerrainVariantEnum
from aetherion.reference.world.terrains import ReferenceGrass


def test_reference_grass_creation() -> None:
    grass = ReferenceGrass(1, 2, 3)

    assert grass.position.x == 1
    assert grass.position.y == 2
    assert grass.position.z == 3
    assert grass.entity_type is not None
    assert grass.entity_type.main_type == EntityEnum.TERRAIN.value
    assert grass.entity_type.sub_type0 == TerrainEnum.GRASS.value


def test_reference_grass_with_variation() -> None:
    grass = ReferenceGrass(
        x=5,
        y=5,
        z=5,
        terrain_variation_type=TerrainVariantEnum.RAMP_EAST.value,
        gradient_direction=DirectionEnum.RIGHT,
    )

    assert grass.entity_type.sub_type1 == TerrainVariantEnum.RAMP_EAST.value
    assert grass.position.direction == DirectionEnum.RIGHT


def test_reference_grass_matter_properties() -> None:
    grass = ReferenceGrass(0, 0, 0)

    assert grass.matter_container.terrain_matter == 7
    assert grass.matter_container.water_matter == 0
    assert grass.matter_container.water_vapor == 0
    assert grass.matter_container.bio_mass_matter == 0


def test_reference_grass_structural_integrity() -> None:
    grass = ReferenceGrass(0, 0, 0)

    assert grass.structural_integrity.can_stack_entities is True
    assert grass.structural_integrity.max_load_capacity == -1
    assert grass.structural_integrity.matter_state == MatterState.SOLID
