"""Reference terrain entity types for simulation contract tests and tutorials.

Values (e.g. ReferenceTerrainEnum) align with common LifeSim / demo conventions for
``EntityTypeComponent.sub_type0``; shipping games may use different catalogs.
"""

import aetherion
from aetherion import BaseEntity, DirectionEnum, EntityEnum, GridType, MatterState, TerrainEnum


class ReferenceGrass(BaseEntity):
    """Reference solid grass terrain wiring the engine expects for basic sim tests.

    This is a contract/example type—not required for production game content.
    """

    grid_type = GridType.TERRAIN
    entity_type = None
    mass = None
    position = None
    velocity = None
    health = None
    perception = None

    def __init__(
        self,
        x: int,
        y: int,
        z: int,
        terrain_variation_type: int = 0,
        gradient_direction: DirectionEnum = DirectionEnum.DOWNWARD,
        gradient_descent_gx: float = 0.0,
        gradient_descent_gy: float = 0.0,
        gradient_descent_gz: float = 0.0,
    ) -> None:
        BaseEntity.__init__(self, x, y, z, direction=gradient_direction)

        self.entity_type = aetherion.EntityTypeComponent()
        self.entity_type.main_type = EntityEnum.TERRAIN.value
        self.entity_type.sub_type0 = TerrainEnum.GRASS.value
        self.entity_type.sub_type1 = terrain_variation_type

        self.structural_integrity = aetherion.StructuralIntegrityComponent()
        self.structural_integrity.can_stack_entities = True
        self.structural_integrity.max_load_capacity = -1
        self.structural_integrity.matter_state = MatterState.SOLID

        gradient_vector = aetherion.GradientVector()
        gradient_vector.gx = gradient_descent_gx
        gradient_vector.gy = gradient_descent_gy
        gradient_vector.gz = gradient_descent_gz

        self.matter_container = aetherion.MatterContainer()
        self.matter_container.terrain_matter = 7
        self.matter_container.water_matter = 0
        self.matter_container.water_vapor = 0
        self.matter_container.bio_mass_matter = 0
