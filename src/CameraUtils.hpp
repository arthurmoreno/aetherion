#ifndef CAMERA_UTILS_HPP
#define CAMERA_UTILS_HPP

#include "EntityInterface.hpp"
#include "LowLevelRenderer/RenderQueue.hpp"
#include "WorldView.hpp"

// bool isMouseWithin(int mx, int my, int x, int y, int width, int height);

// constexpr int TILE_SIZE_ON_SCREEN = 32;

// void setToDrawSelectEntitySquare(
//     int x, int y,
//     std::shared_ptr<RenderQueue> renderQueuePtr,
//     int layerIndex,
//     const std::string& groupToDraw,
//     bool selected = false
// );

class EntityMouseSelection {
   public:
    bool currentEntitySelected;
    EntityMouseSelection(int screenX, int screenY, EntityInterface& entity, int lockOnTarget,
                         int layerIndex, const std::map<std::string, int>& mouseState,
                         int TILE_SIZE_ON_SCREEN)
        : screenX(screenX),
          screenY(screenY),
          entity(entity),
          lockOnTarget(lockOnTarget),
          layerIndex(layerIndex),
          TILE_SIZE_ON_SCREEN(TILE_SIZE_ON_SCREEN) {
        currentEntitySelected = false;
        selectedSquareDrawn = false;

        offsetX = screenX + TILE_SIZE_ON_SCREEN;
        offsetY = screenY + TILE_SIZE_ON_SCREEN;

        mouseX = mouseState.at("x");
        mouseY = mouseState.at("y");
    }

    void drawLockOnTarget(std::shared_ptr<RenderQueue> renderQueuePtr,
                          const std::string& groupToDraw);

    bool isMouseCoordinatesInvalid() const { return mouseX == -1 || mouseY == -1; }

    void setEntityCoordinates() {
        Position entityPos = entity.getComponent<Position>();
        entityX = entityPos.x;
        entityY = entityPos.y;
        entityZ = entityPos.z;
    }

    void setSelectionAndDrawHovered(std::shared_ptr<RenderQueue> renderQueuePtr, int layerIndex,
                                    const std::string& groupToDraw);

    bool checkVoxelBottomSelection(WorldView& worldView,
                                   std::shared_ptr<RenderQueue> renderQueuePtr, int layerIndex,
                                   const std::string& groupToDraw);
    bool checkVoxelTopSelection(WorldView& worldView, std::shared_ptr<RenderQueue> renderQueuePtr,
                                int layerIndex, const std::string& groupToDraw);

   private:
    int screenX, screenY;
    int offsetX, offsetY;
    int layerIndex;
    int TILE_SIZE_ON_SCREEN;

    EntityInterface& entity;
    int lockOnTarget;

    int mouseX, mouseY;
    int entityX, entityY, entityZ;

    bool selectedSquareDrawn;
};

bool getAndDrawSelectedEntity(WorldView& world_view, EntityInterface& entity_interface,
                              nb::dict& mouse_state, int screenX, int screenY,
                              std::shared_ptr<RenderQueue> render_queue_ptr, int layer_index,
                              int selected_entity_id, std::string& groupToDraw,
                              int TILE_SIZE_ON_SCREEN);

void drawTileEffects(EntityInterface& terrain, std::shared_ptr<WorldView>,
                     std::shared_ptr<RenderQueue> render_queue_ptr, int layerIndex,
                     const std::string& guiGroup, int screenX, int screenY,
                     int TILE_SIZE_ON_SCREEN);

bool shouldDrawTerrain(const EntityInterface& terrain, const bool EMPTY_TILE_DEBUGGING);
bool isTerrainAnEmptyWater(const EntityInterface& terrain);
bool isOccludingEntityPerspective(const EntityInterface& entity, const WorldView& worldView,
                                  const EntityInterface& occludingEntity);
bool isOccludingSomeEntity(const WorldView& worldView, const EntityInterface& occludingEntity);

#endif  // CAMERA_UTILS_HPP