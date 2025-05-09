import logging
import random
from enum import IntEnum

import lifesimcore
from lifesimcore import World

# Set up basic configuration for the logger
logging.basicConfig(level=logging.INFO)

# Create a logger instance
logger = logging.getLogger(__name__)


def roll_dice(probability):
    # Generate a random number between 0 and 1
    random_number = random.random()
    # Compare the random number to the desired probability
    if random_number <= probability:
        return True
    else:
        return False


class EntityEnum(IntEnum):
    TERRAIN = 0
    BEAST = 1
    PLANT = 2


class TerrainEnum(IntEnum):
    EMPTY = -1
    GRASS = 0


BLOCKS_IN_WORLDLAYER = {"width": 100, "height": 100, "depth": 10}


def create_grass(x, y, z):
    return lifesimcore.EntityInterface(
        lifesimcore.EntityTypeComponent(EntityEnum.TERRAIN.value, TerrainEnum.GRASS.value),
        lifesimcore.Position(x, y, z),
        lifesimcore.Velocity(0.0, 0.0, 0.0),
    )


class WorldHandler:
    ticks = 0

    def __init__(self, *args, **kwargs) -> None:
        self.num_rows = BLOCKS_IN_WORLDLAYER.get("width")
        self.num_cols = BLOCKS_IN_WORLDLAYER.get("height")
        self.num_layers = BLOCKS_IN_WORLDLAYER.get("depth")

        self._world = World()

        # Initialize terrain tile slot
        self.init_terrains_tensor()

        # This is starting on 1 because the player has entity_id = 0
        # self.entity_id_counter = 1

        # Initialize entity tile slot
        # self.init_entities_tensor()

        # Initialize entity tile slot
        # self.init_events_tensor()

        self.first_cuda_iteration = True

        logger.info("Finished world initialization.")
        self.ready = True

    def init_terrains_tensor(self):
        for i in range(self.num_rows):
            for j in range(self.num_cols):
                for k in range(self.num_layers):
                    self.create_terrain_node(i, j, k)

    def create_terrain_node(self, x, y, z):
        if z == (BLOCKS_IN_WORLDLAYER.get("depth") / 2):
            # Create terrain
            grass = create_grass(x, y, z)
            self._world.set_terrain(x, y, z, grass)
        elif z == ((BLOCKS_IN_WORLDLAYER.get("depth") / 2) + 1):
            # Create terrain
            should_create_grass = roll_dice(0.01)
            if should_create_grass:
                grass = create_grass(x, y, z)
                self._world.set_terrain(x, y, z, grass)


world_handler = WorldHandler()

import ipdb

ipdb.set_trace()

# Create a world instance
world = lifesimcore.World()

# Create an entity
entity = world.create_entity()

# Initialize voxel grid
world.initialize_voxel_grid()

# Set some voxel data
data = lifesimcore.GridData(1, int(entity), 3, 0.5)
world.set_voxel(1, 2, 3, data)

# Get and print voxel data
retrieved_data = world.get_voxel(1, 2, 3)
print(f"Terrain ID: {retrieved_data.terrainID}")
print(f"Entity ID: {retrieved_data.entityID}")
print(f"Event ID: {retrieved_data.eventID}")
print(f"Lighting Level: {retrieved_data.lightingLevel}")
print(f"Biome ID: {retrieved_data.biomeID}")

# Update the world
world.update()

# Remove the entity
world.remove_entity(entity)
