#ifndef ECOSYSTEM_ENGINE_HPP
#define ECOSYSTEM_ENGINE_HPP

#include <spdlog/spdlog.h>

#include <entt/entt.hpp>

#include "GameClock.hpp"
#include "LifeEvents.hpp"
#include "Logger.hpp"
#include "MoveEntityEvent.hpp"
#include "PhysicsManager.hpp"
#include "SunIntensity.hpp"
#include "VoxelGrid.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/PlantsComponents.hpp"
#include "components/TerrainComponents.hpp"

struct SetEcoEntityToDebug {
    entt::entity entity;

    SetEcoEntityToDebug(entt::entity entity) : entity(entity) {}
};

struct ToBeCreatedWaterTile {
    entt::entity entity;
    Position position;
    EntityTypeComponent type;
    MatterContainer matterContainer;
    StructuralIntegrityComponent structuralIntegrity;
};

struct EvaporateWaterEntityEvent {
    entt::entity entity;
    float sunIntensity;

    EvaporateWaterEntityEvent(entt::entity entity, float sunIntensity)
        : entity(entity), sunIntensity(sunIntensity) {}
};

struct CondenseWaterEntityEvent {
    entt::entity entity;
    int condensationAmount;

    CondenseWaterEntityEvent(entt::entity entity, int condensationAmount)
        : entity(entity), condensationAmount(condensationAmount) {}
};

struct WaterFallEntityEvent {
    entt::entity entity;
    Position position;
    int fallingAmount;

    WaterFallEntityEvent(entt::entity entity, Position position, int fallingAmount)
        : entity(entity), position(position), fallingAmount(fallingAmount) {}
};

void processTileWater(entt::entity entity, entt::registry& registry, VoxelGrid& voxelGrid,
                      entt::dispatcher& dispatcher, float sunIntensity,
                      std::vector<EvaporateWaterEntityEvent>& pendingEvaporateWater,
                      std::vector<CondenseWaterEntityEvent>& pendingCondenseWater,
                      std::vector<WaterFallEntityEvent>& pendingWaterFall, std::random_device& rd,
                      std::mt19937& gen, std::uniform_int_distribution<>& disWaterSpreading);

class EcosystemEngine {
   public:
    std::vector<EvaporateWaterEntityEvent> pendingEvaporateWater;
    std::vector<CondenseWaterEntityEvent> pendingCondenseWater;
    std::vector<WaterFallEntityEvent> pendingWaterFall;

    entt::entity entityBeingDebugged;

    int countCreatedEvaporatedWater = 0;

    EcosystemEngine() = default;
    // EcosystemEngine() : registry(reg) {}

    // Method to process physics-related events
    void processEcosystem(entt::registry& registry, VoxelGrid& voxelGrid,
                          entt::dispatcher& dispatcher, GameClock& clock);
    void processEcosystemAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                               entt::dispatcher& dispatcher, GameClock& clock);
    void loopTiles(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                   float sunIntensity);

    void processEvaporateWaterEvents(entt::registry& registry, VoxelGrid& voxelGrid,
                                     std::vector<EvaporateWaterEntityEvent>& pendingEvaporateWater);
    void processCondenseWaterEvents(entt::registry& registry, VoxelGrid& voxelGrid,
                                    std::vector<CondenseWaterEntityEvent>& pendingCondenseWater);
    void processWaterFallEvents(entt::registry& registry, VoxelGrid& voxelGrid,
                                std::vector<WaterFallEntityEvent>& pendingWaterFall);

    // Register the event handler
    void onSetEcoEntityToDebug(const SetEcoEntityToDebug& event);
    void registerEventHandlers(entt::dispatcher& dispatcher);

    bool isProcessingComplete() const;

   private:
    // entt::registry& registry;
    // VoxelGrid* voxelGrid;

    // Mutex for thread safety
    std::mutex ecosystemMutex;
    bool processingComplete = true;  // Flag to indicate processing state
};

#endif  // ECOSYSTEM_ENGINE_HPP