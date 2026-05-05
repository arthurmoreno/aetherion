from aetherion import (
    Position,
    PyRegistry,
    VoxelGrid,
    World,
)


class SpringWaterSystem:
    """Periodically materialises +1 water_matter at a fixed coordinate.

    The system is fully event-driven: each pace-tick dispatches a
    ``WaterCreationEvent`` through ``World.dispatch_water_creation_event``.
    The event handler in the physics engine validates the destination
    cell, picks one of three branches (NONE -> create new water terrain,
    liquid water -> additive merge, vapor -> retry-then-abort), and
    performs the actual repository writes inside a centralised mutator.

    From the spring's perspective this means: it does not read terrain
    state, does not know whether the source cell exists yet, and does
    not need any invariant guards of its own. The mutator owns all
    state-checking and write semantics — keeping the spring on the
    "writes are event-driven, writes live in a centralised mutator,
    mutators validate their own preconditions, mutators never accept
    inconsistency" architectural rules.

    Designed for use with ``mountain_side_world_factory`` as a python
    system registered via ``world.add_python_system()``. The source
    voxel typically sits near max altitude so water can flow downhill;
    the spring will create the cell from air on the first tick if it
    does not exist yet.
    """

    def __init__(
        self,
        world: World,
        pace: int,
        source_x: int,
        source_y: int,
        source_z: int,
    ) -> None:
        self._world = world
        self._pace = pace
        self._source_x = source_x
        self._source_y = source_y
        self._source_z = source_z
        self._tick_count = 0

    def update(self, registry: PyRegistry, voxel_grid: VoxelGrid) -> None:
        del registry, voxel_grid  # state is owned by the event handler

        self._tick_count += 1
        if self._tick_count % self._pace != 0:
            return

        position = Position()
        position.x = self._source_x
        position.y = self._source_y
        position.z = self._source_z
        self._world.dispatch_water_creation_event(position=position, amount=1)
