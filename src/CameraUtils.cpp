#include "CameraUtils.hpp"

bool isMouseWithin(int mx, int my, int x, int y, int width, int height) {
    return x <= mx && mx <= x + width && y <= my && my <= y + height;
}

std::map<std::string, int> convertMouseState(const nb::dict& mouse_state) {
    std::map<std::string, int> mouseState;
    for (const auto& item : mouse_state) {
        std::string key = nb::cast<std::string>(item.first);
        int value = nb::cast<int>(item.second);
        mouseState[key] = value;
    }
    return mouseState;
}

void setToDrawSelectEntitySquare(int x, int y, std::shared_ptr<RenderQueue> renderQueuePtr,
                                 int layerIndex, const std::string& groupToDraw,
                                 bool selected = false, int TILE_SIZE_ON_SCREEN = 32) {
    SDL_Color whiteSDLColor = {255, 255, 255, 255};
    SDL_Color yellowSDLColor = {255, 255, 0, 255};
    SDL_Color selectedSDLColor = selected ? whiteSDLColor : whiteSDLColor;

    int selectedAreaWidth = TILE_SIZE_ON_SCREEN;
    int selectedAreaHeight = TILE_SIZE_ON_SCREEN;

    renderQueuePtr->add_task_draw_rect(layerIndex, groupToDraw, x, y, selectedAreaWidth,
                                       selectedAreaHeight, 3, selectedSDLColor);
}

void EntityMouseSelection::drawLockOnTarget(std::shared_ptr<RenderQueue> renderQueuePtr,
                                            const std::string& groupToDraw) {
    std::cout << "[drawLockOnTarget] Value of lockOnTarget = " << lockOnTarget << std::endl;
    std::cout << "[drawLockOnTarget] Value of entity.getEntityId() = " << entity.getEntityId()
              << std::endl;
    if (lockOnTarget != -1 && entity.getEntityId() == lockOnTarget) {
        std::cout << "[drawLockOnTarget] Inside the if offlockOnTarget != -1 && "
                     "entity.getEntityId() == lockOnTarget."
                  << std::endl;
        setToDrawSelectEntitySquare(offsetX, offsetY, renderQueuePtr, layerIndex, groupToDraw, true,
                                    TILE_SIZE_ON_SCREEN);

        std::ostringstream errorMessage;
        errorMessage << "Have caught an entity. EntityID: " << entity.getEntityId() << std::endl;
        throw std::runtime_error(errorMessage.str());
        selectedSquareDrawn = true;
    }
}

void EntityMouseSelection::setSelectionAndDrawHovered(std::shared_ptr<RenderQueue> renderQueuePtr,
                                                      int layerIndex,
                                                      const std::string& groupToDraw) {
    currentEntitySelected = true;
    if (!selectedSquareDrawn) {
        setToDrawSelectEntitySquare(offsetX, offsetY, renderQueuePtr, layerIndex, groupToDraw,
                                    false, TILE_SIZE_ON_SCREEN);
    }
}

bool EntityMouseSelection::checkVoxelBottomSelection(WorldView& worldView,
                                                     std::shared_ptr<RenderQueue> renderQueuePtr,
                                                     int layerIndex,
                                                     const std::string& groupToDraw) {
    std::cout << "Inside the checkVoxelBottomSelection()." << std::endl;
    if (isMouseWithin(mouseX, mouseY, offsetX, offsetY, TILE_SIZE_ON_SCREEN, TILE_SIZE_ON_SCREEN)) {
        std::cout << "Inside the if  checkVoxelBottomSelection() and isMouseWithin." << std::endl;
        auto cornerSETerrain = worldView.checkIfTerrainExist(entityX + 1, entityY + 1, entityZ);
        auto aboveSETerrain = worldView.checkIfTerrainExist(entityX + 2, entityY + 2, entityZ + 1);

        if (!cornerSETerrain && !aboveSETerrain) {
            std::ostringstream errorMessage;
            errorMessage << "Have entered in !cornerSETerrain && !aboveSETerrain: "
                         << entity.getEntityId() << std::endl;
            throw std::runtime_error(errorMessage.str());
            setSelectionAndDrawHovered(renderQueuePtr, layerIndex, groupToDraw);
            return true;
        }

        bool areaRayCollision = true;

        aboveSETerrain = worldView.checkIfTerrainExist(entityX + 1, entityY + 1, entityZ + 1);
        if (!areaRayCollision && !aboveSETerrain) {
            std::ostringstream errorMessage;
            errorMessage << "Have entered in !areaRayCollision && !aboveSETerrain: "
                         << entity.getEntityId() << std::endl;
            throw std::runtime_error(errorMessage.str());
            setSelectionAndDrawHovered(renderQueuePtr, layerIndex, groupToDraw);
            return true;
        }
    }

    return false;
}

bool EntityMouseSelection::checkVoxelTopSelection(WorldView& worldView,
                                                  std::shared_ptr<RenderQueue> renderQueuePtr,
                                                  int layerIndex, const std::string& groupToDraw) {
    EntityTypeComponent entityType = entity.getComponent<EntityTypeComponent>();
    if (entityType.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
        std::cout << "Inside the checkVoxelTopSelection()." << std::endl;
        std::cout << "Inside the checkVoxelTopSelection(); variables values: " << "mouseX: "
                  << mouseX << "; mouseY: " << mouseY << "; screenX: " << screenX
                  << "; screenY: " << screenY << "; TILE_SIZE_ON_SCREEN: " << TILE_SIZE_ON_SCREEN
                  << std::endl;
        if (isMouseWithin(mouseX, mouseY, screenX, screenY, TILE_SIZE_ON_SCREEN,
                          TILE_SIZE_ON_SCREEN)) {
            std::cout << "Inside the if  checkVoxelTopSelection() and isMouseWithin." << std::endl;
            auto aboveSETerrainExist =
                worldView.checkIfTerrainExist(entityX + 1, entityY + 1, entityZ + 1);
            auto aboveEntityExist = worldView.checkIfEntityExist(entityX, entityY, entityZ + 1);

            if (!aboveSETerrainExist && !aboveEntityExist) {
                setSelectionAndDrawHovered(renderQueuePtr, layerIndex, groupToDraw);
                return true;
            }
        }
    }

    return false;
}

// def _get_and_draw_selected_entity(
//     world_view, entity, mouse_state, screen_x, screen_y, render_queue, layer_index, camera,
//     selected_entity
// ):
//     entity_mouse_selection = EntityMouseSelection(screen_x, screen_y, entity, selected_entity,
//     layer_index, mouse_state) entity_mouse_selection.draw_lock_on_target(render_queue, camera)

//     if entity_mouse_selection.is_mouse_coordinates_invalid():
//         return entity_mouse_selection.current_entity_selected
//     entity_mouse_selection.set_entity_coordinates()

//     if entity_mouse_selection.check_voxel_botton_selection(world_view, render_queue, layer_index,
//     camera):
//         return entity_mouse_selection.current_entity_selected

//     if entity_mouse_selection.check_voxel_top_selection(world_view, render_queue, layer_index,
//     camera):
//         return entity_mouse_selection.current_entity_selected

// TODO: Organize better the order of parameters here
bool getAndDrawSelectedEntity(WorldView& world_view, EntityInterface& entity_interface,
                              nb::dict& mouse_state, int screenX, int screenY,
                              std::shared_ptr<RenderQueue> render_queue_ptr, int layer_index,
                              int selected_entity_id, std::string& groupToDraw,
                              int TILE_SIZE_ON_SCREEN) {
    std::cout << "Beginning of getAndDrawSelectedEntity." << std::endl;
    EntityMouseSelection entity_mouse_selection =
        EntityMouseSelection{screenX,
                             screenY,
                             entity_interface,
                             selected_entity_id,
                             layer_index,
                             convertMouseState(mouse_state),
                             TILE_SIZE_ON_SCREEN};
    entity_mouse_selection.drawLockOnTarget(render_queue_ptr, groupToDraw);

    if (entity_mouse_selection.isMouseCoordinatesInvalid()) {
        std::cout << "Inside the if  off entity_mouse_selection.isMouseCoordinatesInvalid()."
                  << std::endl;
        return entity_mouse_selection.currentEntitySelected;
    }
    entity_mouse_selection.setEntityCoordinates();

    if (entity_mouse_selection.checkVoxelBottomSelection(world_view, render_queue_ptr, layer_index,
                                                         groupToDraw)) {
        std::cout << "Inside the if  checkVoxelBottomSelection()." << std::endl;
        return entity_mouse_selection.currentEntitySelected;
    }

    if (entity_mouse_selection.checkVoxelTopSelection(world_view, render_queue_ptr, layer_index,
                                                      groupToDraw)) {
        std::cout << "Inside the if  off checkVoxelTopSelection()." << std::endl;
        return entity_mouse_selection.currentEntitySelected;
    }

    return false;
}

constexpr SDL_Color BLOOD_DAMAGE_COLOR = {195, 0, 6, 255};

void drawTileEffects(EntityInterface& terrain, std::shared_ptr<WorldView> worldView,
                     std::shared_ptr<RenderQueue> render_queue_ptr, int layerIndex,
                     const std::string& guiGroup, int screenX, int screenY,
                     int TILE_SIZE_ON_SCREEN) {
    if (terrain.hasComponent(ComponentFlag::TILE_EFFECTS_LIST)) {
        const TileEffectsList effectsList = terrain.getComponent<TileEffectsList>();
        if (!effectsList.tileEffectsIDs.empty()) {
            for (const auto& effectId : effectsList.tileEffectsIDs) {
                EntityInterface* effect = worldView->getEntityById(effectId);
                if (effect) {
                    const TileEffectComponent& tileEffectComp =
                        effect->getComponent<TileEffectComponent>();
                    if (tileEffectComp.tileEffectType ==
                        static_cast<int>(TileEffectTypeEnum::BLOOD_DAMAGE)) {
                        std::string damageValueStr =
                            std::to_string(static_cast<int>(tileEffectComp.damageValue));
                        render_queue_ptr->add_task_text(
                            layerIndex, guiGroup, damageValueStr, "my_font", BLOOD_DAMAGE_COLOR,
                            screenX + static_cast<int>(TILE_SIZE_ON_SCREEN * 1.25),
                            screenY + static_cast<int>(TILE_SIZE_ON_SCREEN * 0.6) +
                                tileEffectComp.effectRemainingTime);
                    }
                }
            }
        }
    }
}

// def should_draw_terrain(terrain):
//     return (
//         terrain.get_entity_type().sub_type0 == lifesimcore.TerrainEnum_EMPTY and
//         EMPTY_TILE_DEBUGGING
//     ) or terrain.get_entity_type().sub_type0 != lifesimcore.TerrainEnum_EMPTY
bool shouldDrawTerrain(const EntityInterface& terrain, const bool EMPTY_TILE_DEBUGGING) {
    if (terrain.hasComponent(ComponentFlag::ENTITY_TYPE)) {
        const EntityTypeComponent entityType = terrain.getComponent<EntityTypeComponent>();
        return (entityType.subType0 == static_cast<int>(TerrainEnum::EMPTY) &&
                EMPTY_TILE_DEBUGGING) ||
               entityType.subType0 != static_cast<int>(TerrainEnum::EMPTY);
    } else {
        return false;
    }
}

// def emtpy_water_terrain(terrain):
//     return (
//         terrain.get_entity_type().main_type == lifesimcore.EntityEnum_TERRAIN
//         and terrain.get_entity_type().sub_type0 == lifesimcore.TerrainEnum_WATER
//         and terrain.get_matter_container().water_matter == 0
//         and terrain.get_matter_container().water_vapor == 0
//     )
bool isTerrainAnEmptyWater(const EntityInterface& terrain) {
    if (terrain.hasComponent(ComponentFlag::ENTITY_TYPE) &&
        terrain.hasComponent(ComponentFlag::MATTER_CONTAINER)) {
        const EntityTypeComponent entityType = terrain.getComponent<EntityTypeComponent>();
        const MatterContainer matterContainer = terrain.getComponent<MatterContainer>();

        return (entityType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                entityType.subType0 == static_cast<int>(TerrainEnum::WATER) &&
                matterContainer.WaterMatter == 0 && matterContainer.WaterVapor == 0);
    } else {
        return false;
    }
}

bool isOccludingEntityPerspective(const EntityInterface& entity, const WorldView& worldView,
                                  const EntityInterface& occludingEntity) {
    // Get the position of the entity
    const Position entityPos = entity.getComponent<Position>();
    const int entityX = entityPos.x;
    const int entityY = entityPos.y;
    const int entityZ = entityPos.z;

    // Get the position of the occluding entity
    const Position occludingEntityPos = occludingEntity.getComponent<Position>();
    const int occludingEntityX = occludingEntityPos.x;
    const int occludingEntityY = occludingEntityPos.y;
    const int occludingEntityZ = occludingEntityPos.z;

    // If both entities are at the same position, it's not occluding
    if (entityX == occludingEntityX && entityY == occludingEntityY && entityZ == occludingEntityZ) {
        return false;
    }

    // Check if the occluding entity is of type TERRAIN and at the same Z level
    const EntityTypeComponent occludingEntityType =
        occludingEntity.getComponent<EntityTypeComponent>();
    if (occludingEntityType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        entityZ == occludingEntityZ) {
        // Check direct overlap
        if (entityX == occludingEntityX && entityY == occludingEntityY) {
            return true;
        }

        // Check offsets for occlusion
        if ((entityX + 1 == occludingEntityX && entityY + 1 == occludingEntityY) ||
            (entityX + 1 == occludingEntityX && entityY + 2 == occludingEntityY) ||
            (entityX == occludingEntityX && entityY + 1 == occludingEntityY) ||
            (entityX + 1 == occludingEntityX && entityY == occludingEntityY)) {
            return true;
        }
    }
    // Check if the occluding entity is at a higher Z level
    else if (entityZ < occludingEntityZ) {
        // Check direct overlap
        if (entityX == occludingEntityX && entityY == occludingEntityY) {
            return true;
        }

        // Check diagonal overlap
        if (entityX == occludingEntityX + 1 && entityY == occludingEntityY + 1) {
            return true;
        }
    }

    return false;
}

bool isOccludingSomeEntity(const WorldView& worldView, const EntityInterface& occludingEntity) {
    // Get the position of the occluding entity
    const Position occludingEntityPos = occludingEntity.getComponent<Position>();
    const int occludingEntityX = occludingEntityPos.x;
    const int occludingEntityY = occludingEntityPos.y;
    const int occludingEntityZ = occludingEntityPos.z;

    // Get the occluding entity type
    const EntityTypeComponent occludingEntityType =
        occludingEntity.getComponent<EntityTypeComponent>();

    // Check if the occluding entity is of type TERRAIN
    if (occludingEntityType.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
        // Check the 3 critical voxel positions for terrain occlusion at same Z level:
        // 1. Direct overlap position
        if (worldView.checkIfEntityExist(occludingEntityX, occludingEntityY, occludingEntityZ)) {
            return true;
        }

        // 2. Southeast diagonal offset positions
        if (worldView.checkIfEntityExist(occludingEntityX - 1, occludingEntityY - 1,
                                         occludingEntityZ) ||
            worldView.checkIfEntityExist(occludingEntityX - 1, occludingEntityY - 2,
                                         occludingEntityZ) ||
            worldView.checkIfEntityExist(occludingEntityX, occludingEntityY - 1,
                                         occludingEntityZ) ||
            worldView.checkIfEntityExist(occludingEntityX - 1, occludingEntityY,
                                         occludingEntityZ)) {
            return true;
        }
    }

    // Check for entities at lower Z levels that could be occluded
    // 1. Direct overlap at lower Z
    if (worldView.checkIfEntityExist(occludingEntityX, occludingEntityY, occludingEntityZ - 1)) {
        return true;
    }

    // 2. Diagonal overlap at lower Z
    if (worldView.checkIfEntityExist(occludingEntityX - 1, occludingEntityY - 1,
                                     occludingEntityZ - 1)) {
        return true;
    }

    return false;
}