from typing import Self

from aetherion import DirectionEnum, EntityInterface, Position, EntityTypeComponent


class BaseEntity:
    """
    Base class for all entities in the simulation. Defines common attributes and logic:
      - position (aetherion.Position)
      - velocity (aetherion.Velocity)
      - behavior() method stub
      - from_entity_interface() constructor
      - x, y, z convenience properties
    Subclasses should override class-level `grid_type` and initialize their own `entity_type`,
    and any additional components (e.g., structural_integrity, matter_container, physics_stats).
    """

    # grid_type: GridType
    # entity_type: EntityTypeComponent
    # mass: int
    position: Position
    entity_type: EntityTypeComponent | None
    entity_id: int | None = None

    def __init__(self, x: int = -1, y: int = -1, z: int = -1, direction: DirectionEnum = DirectionEnum.DOWN):
        super().__init__()
        # Initialize position
        self.position = Position()
        self.position.x = x
        self.position.y = y
        self.position.z = z
        self.position.direction = direction
        self.entity_type = EntityTypeComponent()

    def behavior(self):
        """
        Default behavior method. Subclasses should override to implement specific logic.
        """
        print(f"Running behavior for {self}")

    @classmethod
    def from_entity_interface(cls, entity_interface: EntityInterface) -> Self:
        """
        Create an instance from a generic entity interface, copying over core components.
        """
        inst: "BaseEntity" = cls()
        inst.position = entity_interface.get_position()

        return inst

    @property
    def x(self) -> int:
        return self.position.x

    @property
    def y(self) -> int:
        return self.position.y

    @property
    def z(self) -> int:
        return self.position.z
