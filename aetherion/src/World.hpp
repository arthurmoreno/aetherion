#ifndef WORLD_H
#define WORLD_H
#define ENTT_USE_ATOMIC 1
#define ENTT_ENTITY_TYPE int

#include <nanobind/nanobind.h>
#include <spdlog/spdlog.h>

#include <entt/entt.hpp>
#include <future>
#include <map>
#include <memory>
#include <shared_mutex>

#include "CombatSystem.hpp"
#include "EcosystemEngine.hpp"
#include "EffectsSystem.hpp"
#include "EntityInterface.hpp"
#include "GameClock.hpp"
#include "GameDBHandler.hpp"
#include "HealthSystem.hpp"
#include "LifeEvents.hpp"
#include "Logger.hpp"
#include "MetabolismSystem.hpp"
#include "PerceptionResponse.hpp"
#include "PhysicsEngine.hpp"
#include "PyRegistry.hpp"
#include "QueryCommand.hpp"
#include "WorldView.hpp"
#include "voxelgrid/VoxelGrid.hpp"

namespace nb = nanobind;

/**
 * @brief This is the World class that represents the game world.
 */
class World {
   public:
    GameClock gameClock;

    int width;
    int height;
    int depth;

    entt::registry registry;      // Entity component system - must come before voxelGrid
    entt::dispatcher dispatcher;  // Event dispatcher
    VoxelGrid* voxelGrid;         // Change to pointer type
    PyRegistry pyRegistry;

    std::unordered_map<std::string, std::vector<nb::object>> pythonEventCallbacks;

    World(int width, int height, int depth);

    // World(int width, int height, int depth) : width(width), height(height),
    // depth(depth) {}
    ~World();

    // Entity management using EnTT

    // Voxel grid management using VoxelGrid class
    void initializeVoxelGrid();
    void setVoxel(int x, int y, int z, const GridData& data);
    GridData getVoxel(int x, int y, int z) const;

    /**
     * @brief Create an entity using the EntityInterface data
     */
    entt::entity createEntity(const EntityInterface& entityInterface);
    entt::entity createEntityFromPython(nb::object pyEntity);
    void removeEntity(entt::entity entity);
    // Destroy only the EnTT entity handle. Caller must hold appropriate lifecycle locks.
    void destroyEntityHandle(entt::entity entity);

    nb::list getEntityIdsByType(int entityMainType, int entitySubType0);
    nb::dict getEntitiesByType(int entityMainType, int entitySubType0);
    EntityInterface getEntityById(int entityId);

    void setTerrain(int x, int y, int z, const EntityInterface& entityInterface);
    int getTerrain(int x, int y, int z);
    int getEntity(int x, int y, int z);

    GridData getEntityOnVoxel(int x, int y, int z) const;

    // Method to dispatch a movement event
    void dispatchMoveSolidEntityEventById(int entityId,
                                          std::vector<DirectionEnum> directionsToApply);
    void dispatchMoveSolidEntityEventByPosition(int x, int y, int z, GridType gridType,
                                                float deltaX, float deltaY, float deltaZ);
    void dispatchTakeItemEventById(int entityId, int hoveredEntityId, int selectedEntityId);
    void dispatchUseItemEventById(int entityId, int itemSlot, int hoveredEntityId,
                                  int selectedEntityId);
    void dispatchSetEntityToDebug(int entityId);

    // World update function
    void update();

    void processOptionalQueries(const std::vector<QueryCommand>& commands,
                                PerceptionResponse& response);
    nb::bytes createPerceptionResponse(int entityId, nb::list optionalQueries);
    std::vector<char> createPerceptionResponseC(int entityId,
                                                const std::vector<QueryCommand>& commands);
    nb::dict createPerceptionResponses(nb::dict entitiesWithQueries);
    // PerceptionResponse createPerceptionResponse(int entityId);

    // New methods for Python system registration
    void addPythonSystem(nb::object system);
    nb::object getPythonSystem(size_t index) const;

    void addPythonScript(std::string& key, nb::object script);
    void runPythonScript(std::string& key);

    void registerPythonEventHandler(const std::string& eventType, nb::object callback);

    // Ecosystem async processing toggle
    bool getProcessEcosystemAsync() const { return processEcosystemAsync_; }
    void setProcessEcosystemAsync(bool value) { processEcosystemAsync_ = value; }

    // Water simulation error handling
    std::vector<ThreadError> getWaterSimErrors() const;
    bool hasWaterSimErrors() const;

    // Method to return a capsule containing the pointer to this instance
    nb::capsule get_ptr() { return nb::capsule(this, "World"); }

    // New GameDBHandler interface methods
    void putTimeSeries(const std::string& seriesName, long long timestamp, double value);
    std::vector<std::pair<uint64_t, double>> queryTimeSeries(const std::string& seriesName,
                                                             long long start, long long end);
    void executeSQL(const std::string& sql);

   private:
    std::mutex registryMutex;
    mutable std::shared_mutex
        entityLifecycleMutex;  // Protects entity creation/destruction vs perception
    std::unique_ptr<GameDBHandler> dbHandler;

    // Physics
    PhysicsEngine* physicsEngine;
    std::future<void> physicsFuture;

    // Life
    LifeEngine* lifeEngine;

    // Ecosystem
    EcosystemEngine* ecosystemEngine;
    std::future<void> ecosystemFuture;
    bool ecosystemStarted_ = false;
    bool processEcosystemAsync_ = false;

    // MetabolismSystem
    MetabolismSystem* metabolismSystem;
    std::future<void> metabolismFuture;
    const bool processMetabolismAsync = false;

    // HealthSystem
    HealthSystem* healthSystem;

    // CombatSystem
    CombatSystem* combatSystem;

    // EffectsSystem
    EffectsSystem* effectsSystem;

    std::vector<nb::object> pythonSystems;
    std::map<std::string, nb::object> pythonScripts;

    int getPerceptionBounds(int pos, int perception) const;
    void onTakeItemEventPython(const TakeItemEvent& event);
    void onUseItemEventPython(const UseItemEvent& event);
    // Helper: remove entity from terrain storage. Caller MUST hold exclusive
    // `entityLifecycleMutex` before calling this.

    // Helper: acquire lifecycle lock and destroy entity handle safely.
    void destroyEntityHandleWithLifecycleLock(entt::entity entity);

    // Process queued entity deletions when safe to do so (no async tasks running)
    void processEntityDeletion();
};

#endif  // WORLD_H
