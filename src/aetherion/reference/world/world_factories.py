"""Reference ``WorldFactory`` implementations mirroring the minimal dungeon demo."""

from __future__ import annotations

from typing import override

import numpy as np

from aetherion import DirectionEnum, TerrainVariantEnum, World
from aetherion.logger import logger
from aetherion.reference.world._heightmap import compute_gradient_descent, fill_missing_vectors, generate_heightmap
from aetherion.reference.world.terrains import ReferenceGrass
from aetherion.world.models import WorldFactory


class PilarWorldFactory(WorldFactory):
    pause_debug = False

    def draw_square(self, tile_matrix: np.ndarray, h: int = 6, w: int = 6) -> np.ndarray:
        """Draw a centered square of 'full_block' voxels in the tile matrix."""
        tile_matrix[:] = "empty"

        cx = self.width // 2
        cy = self.height // 2

        x_start = max(cx - w // 2, 0)
        x_end = min(cx + w // 2, self.width)
        y_start = max(cy - h // 2, 0)
        y_end = min(cy + h // 2, self.height)

        tile_matrix[:, :, 0] = "full_block"
        tile_matrix[x_start:x_end, y_start:y_end, :4] = "full_block"

        return tile_matrix

    @override
    def generate_world(self) -> World:
        _world = self.create_world()

        heightmap_2d = generate_heightmap(self.width, self.height, self.depth)

        tile_matrix = np.empty((self.width, self.height, self.depth), dtype=object)
        self.final_processed_map = self.draw_square(tile_matrix)

        descent_gx, descent_gy, _gradient_magnitude = compute_gradient_descent(heightmap_2d)
        self.gradient_descent_gx, self.gradient_descent_gy = fill_missing_vectors(descent_gx, descent_gy)
        self.entities_state_views = {}
        self.entity_id_counter = 1
        self.aeolus_entity = None

        self.perception_thread = None

        self.init_world_grid(_world)

        logger.info("Finished world initialization.")

        return _world

    def init_world_grid(self, world: World) -> None:
        for i in range(self.width):
            for j in range(self.height):
                for k in range(self.depth):
                    self.create_terrain_node(world, i, j, k)

    def create_terrain_node(self, world: World, x: int, y: int, z: int) -> None:
        if self.final_processed_map[x, y, z] == "full_block":
            grass = ReferenceGrass(
                x,
                y,
                z,
                TerrainVariantEnum.FULL.value,
                DirectionEnum.DOWN,
                gradient_descent_gx=self.gradient_descent_gx[x, y],
                gradient_descent_gy=self.gradient_descent_gy[x, y],
            )
            world.create_entity(grass)

    def create_entity_node(self, _world: World, _x: int, _y: int, _z: int) -> None:
        return None


class PyramidWorldFactory(WorldFactory):
    """Stepped pyramid: higher z-layers are progressively smaller and centered."""

    pause_debug = False

    def draw_pyramid(self, tile_matrix: np.ndarray) -> np.ndarray:
        tile_matrix[:] = "empty"

        cx = self.width // 2
        cy = self.height // 2

        for z in range(self.depth):
            half_size = self.depth - 1 - z

            x_start = max(cx - half_size, 0)
            x_end = min(cx + half_size + 1, self.width)
            y_start = max(cy - half_size, 0)
            y_end = min(cy + half_size + 1, self.height)

            if x_start >= x_end or y_start >= y_end:
                continue

            tile_matrix[x_start:x_end, y_start:y_end, z] = "full_block"

        return tile_matrix

    @override
    def generate_world(self) -> World:
        _world = self.create_world()

        heightmap_2d = generate_heightmap(self.width, self.height, self.depth)

        tile_matrix = np.empty((self.width, self.height, self.depth), dtype=object)
        self.final_processed_map = self.draw_pyramid(tile_matrix)

        descent_gx, descent_gy, _gradient_magnitude = compute_gradient_descent(heightmap_2d)
        self.gradient_descent_gx, self.gradient_descent_gy = fill_missing_vectors(descent_gx, descent_gy)
        self.entities_state_views = {}
        self.entity_id_counter = 1
        self.aeolus_entity = None
        self.perception_thread = None

        self.init_world_grid(_world)

        logger.info("Finished pyramid world initialization.")

        return _world

    def init_world_grid(self, world: World) -> None:
        for i in range(self.width):
            for j in range(self.height):
                for k in range(self.depth):
                    self.create_terrain_node(world, i, j, k)

    def create_terrain_node(self, world: World, x: int, y: int, z: int) -> None:
        if self.final_processed_map[x, y, z] == "full_block":
            grass = ReferenceGrass(
                x,
                y,
                z,
                TerrainVariantEnum.FULL.value,
                DirectionEnum.DOWN,
                gradient_descent_gx=self.gradient_descent_gx[x, y],
                gradient_descent_gy=self.gradient_descent_gy[x, y],
            )
            world.create_entity(grass)


class EmptySquareWorldFactory(WorldFactory):
    """Define each z-layer with fills and masks (same workflow as the demo)."""

    pause_debug = False

    def __init__(self, width: int = 3, height: int = 3, depth: int = 3):
        super().__init__(width, height, depth)
        self.tile_matrix: np.ndarray = np.empty((width, height, depth), dtype=object)
        self.tile_matrix[:] = "empty"

    def fill_layer(self, z: int, value: str = "full_block") -> None:
        self.tile_matrix[:, :, z] = value

    def apply_mask(
        self,
        z: int,
        mask: list[list[int]],
        starting_x: int = 0,
        starting_y: int = 0,
        value: str = "full_block",
    ) -> None:
        for mx, row in enumerate(mask):
            wx = starting_x + mx
            if wx < 0 or wx >= self.width:
                continue
            for my, cell in enumerate(row):
                wy = starting_y + my
                if wy < 0 or wy >= self.height:
                    continue
                if cell == 1:
                    self.tile_matrix[wx, wy, z] = value

    @override
    def generate_world(self) -> World:
        _world = self.create_world()

        heightmap_2d = generate_heightmap(self.width, self.height, self.depth)
        self.final_processed_map = self.tile_matrix

        descent_gx, descent_gy, _gradient_magnitude = compute_gradient_descent(heightmap_2d)
        self.gradient_descent_gx, self.gradient_descent_gy = fill_missing_vectors(descent_gx, descent_gy)
        self.entities_state_views = {}
        self.entity_id_counter = 1
        self.aeolus_entity = None
        self.perception_thread = None

        self.init_world_grid(_world)

        logger.info("Finished EmptySquareWorldFactory world initialization.")

        return _world

    def init_world_grid(self, world: World) -> None:
        for i in range(self.width):
            for j in range(self.height):
                for k in range(self.depth):
                    self.create_terrain_node(world, i, j, k)

    def create_terrain_node(self, world: World, x: int, y: int, z: int) -> None:
        if self.final_processed_map[x, y, z] == "full_block":
            grass = ReferenceGrass(
                x,
                y,
                z,
                TerrainVariantEnum.FULL.value,
                DirectionEnum.DOWN,
                gradient_descent_gx=self.gradient_descent_gx[x, y],
                gradient_descent_gy=self.gradient_descent_gy[x, y],
            )
            world.create_entity(grass)
