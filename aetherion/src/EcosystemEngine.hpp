#ifndef ECOSYSTEM_ENGINE_HPP
#define ECOSYSTEM_ENGINE_HPP

#include <openvdb/openvdb.h>
#include <spdlog/spdlog.h>
#include <tbb/concurrent_queue.h>

#include <atomic>
#include <chrono>
#include <entt/entt.hpp>
#include <memory>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <thread>

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

struct EvaporateWaterEntityEvent {
    entt::entity entity;
    Position position;
    float sunIntensity;

    EvaporateWaterEntityEvent(entt::entity entity, Position position, float sunIntensity)
        : entity(entity), position(position), sunIntensity(sunIntensity) {}
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

// Forward declarations and supporting types for GridBoxProcessor
struct GridBox {
    int minX, minY, minZ;
    int maxX, maxY, maxZ;

    GridBox(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
        : minX(minX), minY(minY), minZ(minZ), maxX(maxX), maxY(maxY), maxZ(maxZ) {}
};

enum class WaterFlowType { WATER_FLOW, EVAPORATION, CONDENSATION };

struct WaterFlow {
    WaterFlowType type;
    int x, y, z;
    int amount;
    int targetX, targetY, targetZ;  // For flow direction

    WaterFlow(WaterFlowType type, int x, int y, int z, int amount)
        : type(type), x(x), y(y), z(z), amount(amount), targetX(x), targetY(y), targetZ(z) {}

    WaterFlow(WaterFlowType type, int x, int y, int z, int amount, int targetX, int targetY,
              int targetZ)
        : type(type),
          x(x),
          y(y),
          z(z),
          amount(amount),
          targetX(targetX),
          targetY(targetY),
          targetZ(targetZ) {}
};

// Forward declaration for WaterSimulationManager
class WaterSimulationManager;

// Task structure for grid box processing with priority
struct GridBoxTask {
    size_t boxIndex;
    float sunIntensity;
    std::chrono::steady_clock::time_point creationTime;
    int priority;  // Lower values = higher priority

    GridBoxTask(size_t idx, float sunIntensity)
        : boxIndex(idx),
          sunIntensity(sunIntensity),
          creationTime(std::chrono::steady_clock::now()),
          priority(0) {}

    // Comparison operator for priority queue (lower priority value = higher actual priority)
    bool operator<(const GridBoxTask& other) const {
        return priority > other.priority;  // Reverse for min-heap behavior
    }
};

// Round Robin scheduler with priority aging
class RoundRobinScheduler {
   private:
    std::priority_queue<GridBoxTask> taskQueue_;
    mutable std::mutex queueMutex_;  // mutable allows locking in const methods
    std::atomic<int> nextPriority_{0};
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<int> randomDist_;

    // Aging parameters
    static constexpr int MAX_PRIORITY = 1000;
    static constexpr int AGE_BONUS = 10;  // Priority reduction per aging cycle

   public:
    RoundRobinScheduler() : gen_(rd_()), randomDist_(0, 5) {}  // Small random variance

    void addTask(size_t boxIndex, float sunIntensity) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        GridBoxTask task(boxIndex, sunIntensity);
        task.priority =
            nextPriority_.fetch_add(1) + randomDist_(gen_);  // FIFO with small randomization
        taskQueue_.push(task);
    }

    bool getNextTask(size_t& boxIndex, float& sunIntensity) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (taskQueue_.empty()) return false;

        auto task = taskQueue_.top();
        taskQueue_.pop();

        boxIndex = task.boxIndex;
        sunIntensity = task.sunIntensity;
        return true;
    }

    void ageAllTasks() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::priority_queue<GridBoxTask> newQueue;

        while (!taskQueue_.empty()) {
            auto task = taskQueue_.top();
            taskQueue_.pop();
            task.priority = std::max(
                0, task.priority - AGE_BONUS);  // Reduce priority (increase actual priority)
            newQueue.push(task);
        }

        taskQueue_ = std::move(newQueue);
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return taskQueue_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return taskQueue_.empty();
    }
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
    entt::dispatcher* dispatcher_;
    tbb::concurrent_queue<EvaporateWaterEntityEvent>* pendingEvaporateWater;
    tbb::concurrent_queue<CondenseWaterEntityEvent>* pendingCreateWater;
    tbb::concurrent_queue<WaterFallEntityEvent>* pendingWaterFall;

   public:
    // Initialize accessors and registry when processor is created
    void initializeAccessors(
        entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
        tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater,
        tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCreateWater,
        tbb::concurrent_queue<WaterFallEntityEvent>& pendingWaterFall);

    std::vector<WaterFlow> processBox(const GridBox& box, float sunIntensity);

   private:
    void processVoxelWater(int x, int y, int z, std::vector<WaterFlow>& flows);
    void processVoxelEvaporation(int x, int y, int z, std::vector<WaterFlow>& flows);
};

// WaterSimulationManager class for coordinating parallel water simulation
class WaterSimulationManager {
   private:
    std::shared_mutex gridWriteMutex_;  // Reader-writer lock
    std::vector<std::unique_ptr<GridBoxProcessor>> processors_;
    std::vector<GridBox> gridBoxes_;  // Pre-computed grid boxes
    std::vector<std::thread> workerThreads_;
    RoundRobinScheduler scheduler_;
    int numThreads_;

    // Thread pool control
    std::atomic<bool> stopWorkers_{false};
    std::condition_variable taskAvailable_;
    std::mutex taskMutex_;

    // Results collection
    tbb::concurrent_queue<std::vector<WaterFlow>> resultQueue_;
    std::atomic<int> activeWorkers_{0};
    std::atomic<int> completedTasks_{0};

    // Default minimum box dimensions for optimal cache performance (32x32x32)
    static constexpr int DEFAULT_MIN_BOX_SIZE = 32;

   public:
    WaterSimulationManager(int numThreads = std::thread::hardware_concurrency());
    ~WaterSimulationManager();

    // Initialize processors with terrain storage access
    void initializeProcessors(
        entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
        tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater,
        tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCreateWater,
        tbb::concurrent_queue<WaterFallEntityEvent>& pendingWaterFall);

    // Main parallel water simulation processing
    void processWaterSimulation(entt::registry& registry, VoxelGrid& voxelGrid, float sunIntensity);

    // Populate scheduler with a subset of grid boxes (for continuous processing)
    void populateSchedulerWithSubset(float percentage = 0.3f, float sunIntensity = 1.0f);

   private:
    // Thread pool management
    void startWorkerThreads(entt::registry& registry, VoxelGrid& voxelGrid);
    void stopWorkerThreads();
    void workerThreadFunction(int threadId, entt::registry& registry, VoxelGrid& voxelGrid);

    // Partition grid into boxes for parallel processing with minimum box dimensions
    std::vector<GridBox> partitionGridIntoBoxes(const VoxelGrid& voxelGrid,
                                                const GridBox& minBoxDimensions);

    // Utility method to get recommended box dimensions for cache optimization
    static GridBox getRecommendedBoxDimensions() {
        return GridBox(0, 0, 0, 31, 31, 31);
    }  // 32x32x32

    // Process a single box concurrently
    std::vector<WaterFlow> processBoxConcurrently(int processorIndex, const GridBox& box,
                                                  float sunIntensity);

    // Apply water flow modifications with thread synchronization
    void applyModificationsWithLock(entt::registry& registry, VoxelGrid& voxelGrid,
                                    const std::vector<WaterFlow>& modifications);
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

void processTileWater(int x, int y, int z, entt::registry& registry, VoxelGrid& voxelGrid,
                      entt::dispatcher& dispatcher, float sunIntensity,
                      tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater,
                      tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCondenseWater,
                      tbb::concurrent_queue<WaterFallEntityEvent>& pendingWaterFall,
                      std::random_device& rd, std::mt19937& gen,
                      std::uniform_int_distribution<>& disWaterSpreading);

class EcosystemEngine {
   public:
    tbb::concurrent_queue<EvaporateWaterEntityEvent> pendingEvaporateWater;
    tbb::concurrent_queue<CondenseWaterEntityEvent> pendingCondenseWater;
    tbb::concurrent_queue<WaterFallEntityEvent> pendingWaterFall;

    // Water simulation manager for parallel processing
    std::unique_ptr<WaterSimulationManager> waterSimManager_;

    entt::entity entityBeingDebugged;

    int countCreatedEvaporatedWater = 0;

    EcosystemEngine() : waterSimManager_(std::make_unique<WaterSimulationManager>()) {
        // waterSimManager_->initializeProcessors(reg, *voxelGrid);
    }
    // EcosystemEngine() : registry(reg) {}

    // Method to process physics-related events
    void processEcosystem(entt::registry& registry, VoxelGrid& voxelGrid,
                          entt::dispatcher& dispatcher, GameClock& clock);
    void processEcosystemAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                               entt::dispatcher& dispatcher, GameClock& clock);
    void loopTiles(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                   float sunIntensity);

    // Parallel water simulation using WaterSimulationManager
    void processParallelWaterSimulation(entt::registry& registry, VoxelGrid& voxelGrid);

    void processEvaporateWaterEvents(
        entt::registry& registry, VoxelGrid& voxelGrid,
        tbb::concurrent_queue<EvaporateWaterEntityEvent>& pendingEvaporateWater);
    void processCondenseWaterEvents(
        entt::registry& registry, VoxelGrid& voxelGrid,
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