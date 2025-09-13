#ifndef ECOSYSTEM_ENGINE_HPP
#define ECOSYSTEM_ENGINE_HPP

#include <spdlog/spdlog.h>

#include <entt/entt.hpp>
#include <openvdb/openvdb.h>
#include <tbb/concurrent_queue.h>
#include <memory>
#include <optional>

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
#include "terrain/TerrainStorage.hpp"

// Forward declarations and supporting types for GridBoxProcessor
struct GridBox {
    int minX, minY, minZ;
    int maxX, maxY, maxZ;
    
    GridBox(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
        : minX(minX), minY(minY), minZ(minZ), maxX(maxX), maxY(maxY), maxZ(maxZ) {}
};

enum class WaterFlowType {
    WATER_FLOW,
    EVAPORATION,
    CONDENSATION
};

struct WaterFlow {
    WaterFlowType type;
    int x, y, z;
    int amount;
    int targetX, targetY, targetZ; // For flow direction
    
    WaterFlow(WaterFlowType type, int x, int y, int z, int amount)
        : type(type), x(x), y(y), z(z), amount(amount), targetX(x), targetY(y), targetZ(z) {}
        
    WaterFlow(WaterFlowType type, int x, int y, int z, int amount, int targetX, int targetY, int targetZ)
        : type(type), x(x), y(y), z(z), amount(amount), targetX(targetX), targetY(targetY), targetZ(targetZ) {}
};

// GridBoxProcessor class for parallel water simulation
class GridBoxProcessor {
private:
    struct ThreadAccessors {
        std::optional<openvdb::Int32Grid::Accessor> waterAccessor;
        std::optional<openvdb::Int32Grid::Accessor> vaporAccessor;
        std::optional<openvdb::Int32Grid::Accessor> mainTypeAccessor;
        std::optional<openvdb::Int32Grid::Accessor> subType0Accessor;
        std::optional<openvdb::Int32Grid::ConstAccessor> flagsAccessor;
    };
    
    std::unique_ptr<ThreadAccessors> accessors_;
    entt::registry* registry_;
    VoxelGrid* voxelGrid_;
    
public:
    // Initialize accessors and registry when processor is created
    void initializeAccessors(entt::registry& registry, VoxelGrid& voxelGrid);
    
    std::vector<WaterFlow> processBox(const GridBox& box);
    
private:
    void processVoxelWater(int x, int y, int z, std::vector<WaterFlow>& flows);
    void processVoxelEvaporation(int x, int y, int z, std::vector<WaterFlow>& flows);
};

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
                      tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater,
                      tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCondenseWater,
                      tbb::concurrent_queue<WaterFallEntityEvent>& pendingWaterFall, std::random_device& rd,
                      std::mt19937& gen, std::uniform_int_distribution<>& disWaterSpreading);

class EcosystemEngine {
   public:
    tbb::concurrent_queue<EvaporateWaterEntityEvent> pendingEvaporateWater;
    tbb::concurrent_queue<CondenseWaterEntityEvent> pendingCondenseWater;
    tbb::concurrent_queue<WaterFallEntityEvent> pendingWaterFall;

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
                                     tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater);
    void processCondenseWaterEvents(entt::registry& registry, VoxelGrid& voxelGrid,
                                    tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCondenseWater);
    void processWaterFallEvents(entt::registry& registry, VoxelGrid& voxelGrid,
                                tbb::concurrent_queue<WaterFallEntityEvent>& pendingWaterFall);

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