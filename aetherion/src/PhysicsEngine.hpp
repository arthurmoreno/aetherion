#ifndef PHYSICSENGINE_HPP
#define PHYSICSENGINE_HPP

#include <tbb/concurrent_queue.h>

#include <chrono>
#include <entt/entt.hpp>
#include <mutex>
#include <string>
#include <unordered_map>

#include "EcosystemEngine.hpp"
#include "GameClock.hpp"
#include "GameDBHandler.hpp"
#include "ItemsEvents.hpp"
#include "MoveEntityEvent.hpp"
#include "SunIntensity.hpp"
#include "components/ConsoleLogsComponent.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/PlantsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "physics/PhysicsEvents.hpp"
#include "physics/PhysicsManager.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Forward declarations for water events
struct EvaporateWaterEntityEvent;
struct CondenseWaterEntityEvent;
struct WaterFallEntityEvent;
struct WaterSpreadEvent;
struct WaterGravityFlowEvent;
struct TerrainPhaseConversionEvent;
struct VaporCreationEvent;
struct VaporMergeUpEvent;
struct VaporMergeSidewaysEvent;
struct AddVaporToTileAboveEvent;
struct CreateVaporEntityEvent;

struct SetPhysicsEntityToDebug {
    entt::entity entity;

    SetPhysicsEntityToDebug(entt::entity entity) : entity(entity) {}
};

class PhysicsEngine {
   public:
    std::mutex physicsMutex;

    entt::entity entityBeingDebugged;

    PhysicsEngine() = default;
    PhysicsEngine(entt::registry& reg, entt::dispatcher& disp, VoxelGrid* voxelGrid)
        : registry(reg), dispatcher(disp), voxelGrid(voxelGrid) {}

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
    void onInvalidTerrainFound(const InvalidTerrainFoundEvent& event);

    // Handle water phase change events (moved from EcosystemEngine)
    void onEvaporateWaterEntityEvent(const EvaporateWaterEntityEvent& event);
    void onCondenseWaterEntityEvent(const CondenseWaterEntityEvent& event);
    void onWaterFallEntityEvent(const WaterFallEntityEvent& event);
    void onWaterSpreadEvent(const WaterSpreadEvent& event);
    void onWaterGravityFlowEvent(const WaterGravityFlowEvent& event);
    void onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent& event);
    void onVaporCreationEvent(const VaporCreationEvent& event);
    void onVaporMergeUpEvent(const VaporMergeUpEvent& event);
    void onVaporMergeSidewaysEvent(const VaporMergeSidewaysEvent& event);
    void onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent& event);
    void onCreateVaporEntityEvent(const CreateVaporEntityEvent& event);
    void onDeleteOrConvertTerrainEvent(const DeleteOrConvertTerrainEvent& event);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);
    void registerVoxelGrid(VoxelGrid* voxelGrid) { this->voxelGrid = voxelGrid; }

    // Metrics flush to game DB
    void flushPhysicsMetrics(GameDBHandler* dbHandler);

    // Increment a named metric
    void incPhysicsMetric(const std::string& metricName);

    bool isProcessingComplete() const;

   private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
    VoxelGrid* voxelGrid = nullptr;

    // Monitoring counters for physics events
    std::unordered_map<std::string, uint64_t> physicsMetrics_;
    std::mutex metricsMutex_;

    // Mutex for thread safety
    bool processingComplete = true;  // Flag to indicate processing state

    // Private helper methods
    bool checkIfCanJump(const MoveSolidEntityEvent& event);
};

#endif  // PHYSICSENGINE_HPP