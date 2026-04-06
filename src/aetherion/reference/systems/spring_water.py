from aetherion import MatterContainer, PyRegistry, VoxelGrid


class SpringWaterSystem:
    """Periodically injects +1 water_matter into a fixed terrain voxel.

    Designed for use with ``mountain_side_world_factory`` as a python system
    registered via ``world.add_python_system()``.  The source voxel is typically
    placed at Z-Max - 1 (one below the mountain peak) so water can flow downhill.
    """

    def __init__(
        self,
        pace: int,
        source_x: int,
        source_y: int,
        source_z: int,
    ) -> None:
        self._pace = pace
        self._source_x = source_x
        self._source_y = source_y
        self._source_z = source_z
        self._tick_count = 0

    def update(self, registry: PyRegistry, voxel_grid: VoxelGrid) -> None:
        self._tick_count += 1
        if self._tick_count % self._pace != 0:
            return

        terrain_id = voxel_grid.get_terrain(self._source_x, self._source_y, self._source_z)
        if terrain_id == -2:
            print(
                "SpringWaterSystem: No terrain at "
                f"({self._source_x}, {self._source_y}, {self._source_z}). "
                "Cannot inject water."
            )
            return

        mc: MatterContainer | None = voxel_grid.get_terrain_matter_container_component(
            self._source_x, self._source_y, self._source_z
        )
        if mc is None:
            print(
                "SpringWaterSystem: No MatterContainer found at "
                f"({self._source_x}, {self._source_y}, {self._source_z}). "
                "Cannot inject water."
            )
            return

        mc.water_matter += 1
        print(
            "SpringWaterSystem: Injected water at "
            f"({self._source_x}, {self._source_y}, {self._source_z}). "
            f"New water_matter: {mc.water_matter}"
        )
        voxel_grid.set_terrain_matter_container_component(self._source_x, self._source_y, self._source_z, mc)
