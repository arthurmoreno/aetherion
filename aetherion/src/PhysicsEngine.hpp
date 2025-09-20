#ifndef PHYSICSENGINE_HPP
#define PHYSICSENGINE_HPP

#include <entt/entt.hpp>

#include "EcosystemEngine.hpp"
#include "GameClock.hpp"
#include "ItemsEvents.hpp"
#include "MoveEntityEvent.hpp"
#include "PhysicsManager.hpp"
#include "SunIntensity.hpp"
#include "VoxelGrid.hpp"
#include "components/ConsoleLogsComponent.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/PlantsComponents.hpp"
#include "components/TerrainComponents.hpp"

struct SetPhysicsEntityToDebug {
    entt::entity entity;

    SetPhysicsEntityToDebug(entt::entity entity) : entity(entity) {}
};

class PhysicsEngine {
   public:
    std::mutex physicsMutex;

    entt::entity entityBeingDebugged;

    PhysicsEngine() = default;
    PhysicsEngine(entt::registry& reg, VoxelGrid* voxelGrid)
        : registry(reg), voxelGrid(voxelGrid) {}

    // Method to process physics-related events
    void processPhysics(entt::registry& registry, VoxelGrid& voxelGrid,
                        entt::dispatcher& dispatcher, GameClock& clock);
    void processPhysicsAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                             entt::dispatcher& dispatcher, GameClock& clock);

    // Example: Applying force to an entity
    // void applyForce(entt::registry& registry, VoxelGrid& voxelGrid,
    // entt::entity entity, float fx, float fy, float fz);

    // Handle entity movement event
    void onMoveSolidEntityEvent(const MoveSolidEntityEvent& event);
    void onMoveGasEntityEvent(const MoveGasEntityEvent& event);
    void onTakeItemEvent(const TakeItemEvent& event);
    void onUseItemEvent(const UseItemEvent& event);
    void onSetPhysicsEntityToDebug(const SetPhysicsEntityToDebug& event);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);
    void registerVoxelGrid(VoxelGrid* voxelGrid) { this->voxelGrid = voxelGrid; }

    bool isProcessingComplete() const;

   private:
    entt::registry& registry;
    VoxelGrid* voxelGrid = nullptr;

    // Mutex for thread safety
    bool processingComplete = true;  // Flag to indicate processing state

    // Private helper methods
    bool checkIfCanJump(const MoveSolidEntityEvent& event);
    std::tuple<float, float, float> translatePhysicsToGridMovement(
        float velocityX, float velocityY, float velocityZ, float accelerationX, float accelerationY,
        float accelerationZ, int16_t maxSpeed);
};

#endif  // PHYSICSENGINE_HPP