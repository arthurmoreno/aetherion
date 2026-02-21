from enum import IntEnum

from aetherion import TerrainVariantEnum


class WorldInstanceTypes(IntEnum):
    SYNCHRONOUS = 1
    PIPE_PROCESS = 2
    SERVER = 3



TERRAIN_VARIATION_TYPE_MAP = {
    TerrainVariantEnum.FULL.value: "full",
    TerrainVariantEnum.RAMP_EAST.value: "ramp_east",
    TerrainVariantEnum.RAMP_WEST.value: "ramp_west",
    TerrainVariantEnum.CORNER_SOUTH_EAST.value: "corner_south_east",
    TerrainVariantEnum.CORNER_SOUTH_EAST_INV.value: "corner_south_east_inverted",
    TerrainVariantEnum.CORNER_NORTH_EAST.value: "corner_north_east",
    TerrainVariantEnum.CORNER_NORTH_EAST_INV.value: "corner_north_east_inverted",
    TerrainVariantEnum.RAMP_SOUTH.value: "ramp_south",
    TerrainVariantEnum.RAMP_NORTH.value: "ramp_north",
    TerrainVariantEnum.CORNER_SOUTH_WEST.value: "corner_south_west",
    TerrainVariantEnum.CORNER_NORTH_WEST.value: "corner_north_west",
}
