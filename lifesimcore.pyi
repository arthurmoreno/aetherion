import packages.lifesimcore.lifesimcore

from . import lifesimcore as lifesimcore
from .lifesimcore import (
    ComponentFlag as ComponentFlag,
)
from .lifesimcore import (
    ConsoleLogsComponent as ConsoleLogsComponent,
)
from .lifesimcore import (
    DefaultGenomeConfig as DefaultGenomeConfig,
)
from .lifesimcore import (
    DigestingFoodItem as DigestingFoodItem,
)
from .lifesimcore import (
    DigestionComponent as DigestionComponent,
)
from .lifesimcore import (
    DirectionEnum as DirectionEnum,
)
from .lifesimcore import (
    DropRates as DropRates,
)
from .lifesimcore import (
    Durability as Durability,
)
from .lifesimcore import (
    Entity as Entity,
)
from .lifesimcore import (
    EntityEnum as EntityEnum,
)
from .lifesimcore import (
    EntityInterface as EntityInterface,
)
from .lifesimcore import (
    EntityTypeComponent as EntityTypeComponent,
)
from .lifesimcore import (
    FoodItem as FoodItem,
)
from .lifesimcore import (
    FruitGrowth as FruitGrowth,
)
from .lifesimcore import (
    GameClock as GameClock,
)
from .lifesimcore import (
    GenomeParams as GenomeParams,
)
from .lifesimcore import (
    GradientVector as GradientVector,
)
from .lifesimcore import (
    GridData as GridData,
)
from .lifesimcore import (
    GridType as GridType,
)
from .lifesimcore import (
    HealthComponent as HealthComponent,
)
from .lifesimcore import (
    Inventory as Inventory,
)
from .lifesimcore import (
    ItemConfiguration as ItemConfiguration,
)
from .lifesimcore import (
    ItemEnum as ItemEnum,
)
from .lifesimcore import (
    ItemFoodEnum as ItemFoodEnum,
)
from .lifesimcore import (
    ItemToolEnum as ItemToolEnum,
)
from .lifesimcore import (
    ItemTypeComponent as ItemTypeComponent,
)
from .lifesimcore import (
    ListStringResponse as ListStringResponse,
)
from .lifesimcore import (
    Logger as Logger,
)
from .lifesimcore import (
    MapEntityInterface as MapEntityInterface,
)
from .lifesimcore import (
    MapOfMapsOfDoubleResponse as MapOfMapsOfDoubleResponse,
)
from .lifesimcore import (
    MapOfMapsResponse as MapOfMapsResponse,
)
from .lifesimcore import (
    MapStrDouble as MapStrDouble,
)
from .lifesimcore import (
    MapStrInt as MapStrInt,
)
from .lifesimcore import (
    MapStrMapStrDouble as MapStrMapStrDouble,
)
from .lifesimcore import (
    MapStrMapStrStr as MapStrMapStrStr,
)
from .lifesimcore import (
    MapStrStr as MapStrStr,
)
from .lifesimcore import (
    MatterContainer as MatterContainer,
)
from .lifesimcore import (
    MatterState as MatterState,
)
from .lifesimcore import (
    MeeleAttackComponent as MeeleAttackComponent,
)
from .lifesimcore import (
    MetabolismComponent as MetabolismComponent,
)
from .lifesimcore import (
    MovingComponent as MovingComponent,
)
from .lifesimcore import (
    ParentsComponent as ParentsComponent,
)
from .lifesimcore import (
    PerceptionComponent as PerceptionComponent,
)
from .lifesimcore import (
    PerceptionResponse as PerceptionResponse,
)
from .lifesimcore import (
    PerceptionResponseFlatB as PerceptionResponseFlatB,
)
from .lifesimcore import (
    PhysicsStats as PhysicsStats,
)
from .lifesimcore import (
    Position as Position,
)
from .lifesimcore import (
    PyRegistry as PyRegistry,
)
from .lifesimcore import (
    QueryResponse as QueryResponse,
)
from .lifesimcore import (
    RenderQueue as RenderQueue,
)
from .lifesimcore import (
    StructuralIntegrityComponent as StructuralIntegrityComponent,
)
from .lifesimcore import (
    SunIntensity as SunIntensity,
)
from .lifesimcore import (
    TerrainEnum as TerrainEnum,
)
from .lifesimcore import (
    TileEffectComponent as TileEffectComponent,
)
from .lifesimcore import (
    TileEffectsList as TileEffectsList,
)
from .lifesimcore import (
    TileEffectTypeEnum as TileEffectTypeEnum,
)
from .lifesimcore import (
    VecDirectionEnum as VecDirectionEnum,
)
from .lifesimcore import (
    VecInt as VecInt,
)
from .lifesimcore import (
    VecStr as VecStr,
)
from .lifesimcore import (
    VecVoxelGridCoordinates as VecVoxelGridCoordinates,
)
from .lifesimcore import (
    Velocity as Velocity,
)
from .lifesimcore import (
    VoxelGrid as VoxelGrid,
)
from .lifesimcore import (
    VoxelGridCoordinates as VoxelGridCoordinates,
)
from .lifesimcore import (
    VoxelGridView as VoxelGridView,
)
from .lifesimcore import (
    VoxelGridViewFlatB as VoxelGridViewFlatB,
)
from .lifesimcore import (
    WeaponAttributes as WeaponAttributes,
)
from .lifesimcore import (
    World as World,
)
from .lifesimcore import (
    WorldView as WorldView,
)
from .lifesimcore import (
    WorldViewFlatB as WorldViewFlatB,
)
from .lifesimcore import (
    deregister_item_configuration as deregister_item_configuration,
)
from .lifesimcore import (
    destroy_texture as destroy_texture,
)
from .lifesimcore import (
    draw_tile_effects as draw_tile_effects,
)
from .lifesimcore import (
    get_and_draw_selected_entity as get_and_draw_selected_entity,
)
from .lifesimcore import (
    get_item_configuration as get_item_configuration,
)
from .lifesimcore import (
    get_pruned_copy as get_pruned_copy,
)
from .lifesimcore import (
    get_terrain_camera_stats as get_terrain_camera_stats,
)
from .lifesimcore import (
    get_water_camera_stats as get_water_camera_stats,
)
from .lifesimcore import (
    imgui_init as imgui_init,
)
from .lifesimcore import (
    render_in_game_gui_frame as render_in_game_gui_frame,
)
from .lifesimcore import (
    imgui_process_event as imgui_process_event,
)
from .lifesimcore import (
    imgui_render as imgui_render,
)
from .lifesimcore import (
    is_occluding_entity_perspective as is_occluding_entity_perspective,
)
from .lifesimcore import (
    is_terrain_an_empty_water as is_terrain_an_empty_water,
)
from .lifesimcore import (
    load_texture as load_texture,
)
from .lifesimcore import (
    load_texture_on_manager as load_texture_on_manager,
)
from .lifesimcore import (
    register_item_configuration as register_item_configuration,
)
from .lifesimcore import (
    render_texture as render_texture,
)
from .lifesimcore import (
    render_texture_from_manager as render_texture_from_manager,
)
from .lifesimcore import (
    should_draw_terrain as should_draw_terrain,
)
from .lifesimcore import (
    wants_capture_keyboard as wants_capture_keyboard,
)
from .lifesimcore import (
    wants_capture_mouse as wants_capture_mouse,
)

ARMOR: site - packages.lifesimcore.lifesimcore.ItemEnum = site - packages.lifesimcore.lifesimcore.ItemEnum.ARMOR

BEAST: site - packages.lifesimcore.lifesimcore.EntityEnum = site - packages.lifesimcore.lifesimcore.EntityEnum.BEAST

BLOOD_DAMAGE: site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum = (
    site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum.BLOOD_DAMAGE
)

COMPONENT_COUNT: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.COMPONENT_COUNT
)

DOWN: site - packages.lifesimcore.lifesimcore.DirectionEnum = site - packages.lifesimcore.lifesimcore.DirectionEnum.DOWN

DOWNWARD: site - packages.lifesimcore.lifesimcore.DirectionEnum = (
    site - packages.lifesimcore.lifesimcore.DirectionEnum.DOWNWARD
)

DirectionEnum_DOWN: int = 3

DirectionEnum_DOWNWARD: int = 6

DirectionEnum_LEFT: int = 4

DirectionEnum_RIGHT: int = 2

DirectionEnum_UP: int = 1

DirectionEnum_UPWARD: int = 5

EMPTY: site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum = (
    site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum.EMPTY
)

ENTITY: site - packages.lifesimcore.lifesimcore.GridType = site - packages.lifesimcore.lifesimcore.GridType.ENTITY

ENTITY_TYPE: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.ENTITY_TYPE
)

EntityEnum_BEAST: int = 2

EntityEnum_PLANT: int = 1

EntityEnum_TERRAIN: int = 0

EntityEnum_TILE_EFFECT: int = 3

FOOD: site - packages.lifesimcore.lifesimcore.ItemEnum = site - packages.lifesimcore.lifesimcore.ItemEnum.FOOD

GAS: site - packages.lifesimcore.lifesimcore.MatterState = site - packages.lifesimcore.lifesimcore.MatterState.GAS

GRASS: site - packages.lifesimcore.lifesimcore.TerrainEnum = site - packages.lifesimcore.lifesimcore.TerrainEnum.GRASS

GREEN_DAMAGE: site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum = (
    site - packages.lifesimcore.lifesimcore.TileEffectTypeEnum.GREEN_DAMAGE
)

HEALTH: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.HEALTH
)

LEFT: site - packages.lifesimcore.lifesimcore.DirectionEnum = site - packages.lifesimcore.lifesimcore.DirectionEnum.LEFT

LIQUID: site - packages.lifesimcore.lifesimcore.MatterState = site - packages.lifesimcore.lifesimcore.MatterState.LIQUID

MASS: site - packages.lifesimcore.lifesimcore.ComponentFlag = site - packages.lifesimcore.lifesimcore.ComponentFlag.MASS

MOVING_COMPONENT: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.MOVING_COMPONENT
)

PERCEPTION: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.PERCEPTION
)

PLANT: site - packages.lifesimcore.lifesimcore.EntityEnum = site - packages.lifesimcore.lifesimcore.EntityEnum.PLANT

PLASMA: site - packages.lifesimcore.lifesimcore.MatterState = site - packages.lifesimcore.lifesimcore.MatterState.PLASMA

POSITION: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.POSITION
)

PlantEnum_RASPBERRY: int = 1

RASPBERRY_FRUIT: site - packages.lifesimcore.lifesimcore.ItemFoodEnum = (
    site - packages.lifesimcore.lifesimcore.ItemFoodEnum.RASPBERRY_FRUIT
)

RASPBERRY_LEAF: site - packages.lifesimcore.lifesimcore.ItemFoodEnum = (
    site - packages.lifesimcore.lifesimcore.ItemFoodEnum.RASPBERRY_LEAF
)

RIGHT: site - packages.lifesimcore.lifesimcore.DirectionEnum = (
    site - packages.lifesimcore.lifesimcore.DirectionEnum.RIGHT
)

SOLID: site - packages.lifesimcore.lifesimcore.MatterState = site - packages.lifesimcore.lifesimcore.MatterState.SOLID

STONE_AXE: site - packages.lifesimcore.lifesimcore.ItemToolEnum = (
    site - packages.lifesimcore.lifesimcore.ItemToolEnum.STONE_AXE
)

TERRAIN: site - packages.lifesimcore.lifesimcore.EntityEnum = site - packages.lifesimcore.lifesimcore.EntityEnum.TERRAIN

TILE_EFFECT: site - packages.lifesimcore.lifesimcore.EntityEnum = (
    site - packages.lifesimcore.lifesimcore.EntityEnum.TILE_EFFECT
)

TOOL: site - packages.lifesimcore.lifesimcore.ItemEnum = site - packages.lifesimcore.lifesimcore.ItemEnum.TOOL

TerrainEnum_EMPTY: int = -1

TerrainEnum_GRASS: int = 0

TerrainEnum_WATER: int = 1

UP: site - packages.lifesimcore.lifesimcore.DirectionEnum = site - packages.lifesimcore.lifesimcore.DirectionEnum.UP

UPWARD: site - packages.lifesimcore.lifesimcore.DirectionEnum = (
    site - packages.lifesimcore.lifesimcore.DirectionEnum.UPWARD
)

VELOCITY: site - packages.lifesimcore.lifesimcore.ComponentFlag = (
    site - packages.lifesimcore.lifesimcore.ComponentFlag.VELOCITY
)

WATER: site - packages.lifesimcore.lifesimcore.TerrainEnum = site - packages.lifesimcore.lifesimcore.TerrainEnum.WATER

WEAPON: site - packages.lifesimcore.lifesimcore.ItemEnum = site - packages.lifesimcore.lifesimcore.ItemEnum.WEAPON
