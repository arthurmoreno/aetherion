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

  entt::registry
      registry; // Entity component system - must come before voxelGrid
  entt::dispatcher dispatcher; // Event dispatcher
  VoxelGrid *voxelGrid;        // Change to pointer type
  PyRegistry pyRegistry;

  std::unordered_map<std::string, std::vector<nb::object>> pythonEventCallbacks;

  World(int width, int height, int depth);

  // World(int width, int height, int depth) : width(width), height(height),
  // depth(depth) {}
  ~World();

  // Entity management using EnTT

  // Voxel grid management using VoxelGrid class
  void initializeVoxelGrid();
  void setVoxel(int x, int y, int z, const GridData &data);
  GridData getVoxel(int x, int y, int z) const;

  /**
   * @brief Create an entity using the EntityInterface data
   */
  entt::entity createEntity(const EntityInterface &entityInterface);
  entt::entity createEntityFromPython(nb::object pyEntity);
  void removeEntity(entt::entity entity);
  // Destroy only the EnTT entity handle. Caller must hold appropriate lifecycle
  // locks.
  void destroyEntityHandle(entt::entity entity);

  nb::list getEntityIdsByType(int entityMainType, int entitySubType0);
  nb::dict getEntitiesByType(int entityMainType, int entitySubType0);
  EntityInterface getEntityById(int entityId);

  void setTerrain(int x, int y, int z, const EntityInterface &entityInterface);
  int getTerrain(int x, int y, int z);
  int getEntity(int x, int y, int z);

  GridData getEntityOnVoxel(int x, int y, int z) const;

  // Method to dispatch a movement event
  void dispatchMoveSolidEntityEventById(
      int entityId, std::vector<DirectionEnum> directionsToApply);
  void dispatchMoveSolidEntityEventByPosition(int x, int y, int z,
                                              GridType gridType, float deltaX,
                                              float deltaY, float deltaZ);
  void dispatchTakeItemEventById(int entityId, int hoveredEntityId,
                                 int selectedEntityId);
  void dispatchUseItemEventById(int entityId, int itemSlot, int hoveredEntityId,
                                int selectedEntityId);
  void dispatchSetEntityToDebug(int entityId);

  // Test/debug helper: enqueue a WaterFallEntityEvent directly. Used by Python
  // tests that need to drive the event-based water path without going through
  // the velocity grid. `entity` is advisory; pass -2 (NONE) to skip the entity
  // path entirely.
  void dispatchWaterFallEvent(Position sourcePos, Position destPos,
                              int fallingAmount, int entity);

  // Test/debug helper: enqueue a WaterGravityFlowEvent directly. Source/target
  // type and matter snapshots are read from the repository at dispatch time so
  // the caller does not need to assemble them manually. Pass `targetTerrainId
  // = -2` (NONE) to drive the empty-destination branch.
  void dispatchWaterGravityFlowEvent(Position sourcePos, Position targetPos,
                                     int amount, int targetTerrainId);

  // Test/debug helper: enqueue a CondenseWaterEntityEvent directly. Pass
  // `terrainBelowId = -2` (NONE) to drive the empty-destination branch
  // (createWaterTerrainBelowVapor); pass a non-NONE id to test merging
  // condensation into existing terrain below.
  void dispatchCondenseWaterEvent(Position vaporPos, int condensationAmount,
                                  int terrainBelowId);

  // Test/debug helper: enqueue a MoveGasEntityEvent directly. `entity` is
  // advisory and defaults to ON_GRID_STORAGE (-1) since vapor cells no
  // longer carry an EnTT entity. `forceX` / `forceY` drive horizontal
  // movement (sideways diffusion); `rhoEnv` / `rhoGas` drive vertical
  // buoyancy. Always sets `forceApplyNewVelocity` so the handler does
  // not block on a stale direction guard.
  void dispatchMoveGasEntityEvent(Position position, int entity, float forceX,
                                  float forceY, float rhoEnv, float rhoGas);

  // Test/debug helper: enqueue a VaporCreationEvent directly. The handler
  // creates a vapor cell at `position` (must currently be NONE or a
  // vapor-transitory water cell with WaterMatter == 0) with the given
  // amount. Used by tests that need to drive vapor creation without
  // going through evaporation/condensation timing.
  void dispatchVaporCreationEvent(Position position, int amount);

  // Enqueue a WaterCreationEvent. Materialises liquid water at `position`
  // — either creating a fresh ON_GRID_STORAGE water cell when `position`
  // is NONE, or doing an additive matter merge when it already holds
  // liquid water. Refuses on vapor-only or non-water terrain (with
  // retry-then-abort for the vapor case). Used by `SpringWaterSystem`,
  // weather scripts, and tests that need a coord-only water source that
  // does not drain another cell.
  void dispatchWaterCreationEvent(Position position, int amount);

  // Test/debug helper: delete the terrain voxel at (x, y, z) via
  // `VoxelGrid::deleteTerrain`. The physics layer's velocity-driven
  // pass picks up any settled water above on the next tick. Forwards
  // World's owned dispatcher and acquires the terrain grid lock
  // internally.
  void deleteTerrainAt(int x, int y, int z);

  // World update function
  void update();

  void processOptionalQueries(const std::vector<QueryCommand> &commands,
                              PerceptionResponse &response);
  nb::bytes createPerceptionResponse(int entityId, nb::list optionalQueries);
  std::vector<char>
  createPerceptionResponseC(int entityId,
                            const std::vector<QueryCommand> &commands);
  nb::dict createPerceptionResponses(nb::dict entitiesWithQueries);
  // PerceptionResponse createPerceptionResponse(int entityId);

  // New methods for Python system registration
  void addPythonSystem(nb::object system);
  nb::object getPythonSystem(size_t index) const;

  void addPythonScript(std::string &key, nb::object script);
  void runPythonScript(std::string &key);

  void registerPythonEventHandler(const std::string &eventType,
                                  nb::object callback);

  // Ecosystem on/off toggle (when false, ecosystem step doesn't run at all
  // per tick; when true, runs sync vs async per `runEcosystemSynchronously`).
  bool getProcessEcosystem() const { return processEcosystem_; }
  void setProcessEcosystem(bool value) { processEcosystem_ = value; }

  // Metabolism on/off toggle (mirrors the ecosystem flag's shape).
  bool getProcessMetabolism() const { return processMetabolism_; }
  void setProcessMetabolism(bool value) { processMetabolism_ = value; }

  // Water simulation phase toggles (delegate to PhysicsManager singleton)
  bool getSimulateVaporCondensation() const;
  void setSimulateVaporCondensation(bool value);
  bool getSimulateVaporMovement() const;
  void setSimulateVaporMovement(bool value);
  bool getSimulateWaterMovement() const;
  void setSimulateWaterMovement(bool value);
  bool getSimulateWaterEvaporation() const;
  void setSimulateWaterEvaporation(bool value);
  bool getWaterAutoBalancing() const;
  void setWaterAutoBalancing(bool value);
  bool getRunEcosystemSynchronously() const;
  void setRunEcosystemSynchronously(bool value);

  // Water simulation error handling
  std::vector<ThreadError> getWaterSimErrors() const;
  bool hasWaterSimErrors() const;

  // Method to return a capsule containing the pointer to this instance
  nb::capsule get_ptr() { return nb::capsule(this, "World"); }

  /** Python-facing views; lifetime is tied to this World (see bindings
   * keep_alive). */
  PyRegistry *getPyRegistry() { return &pyRegistry; }
  VoxelGrid *getVoxelGrid() { return voxelGrid; }

  // New GameDBHandler interface methods
  void putTimeSeries(const std::string &seriesName, long long timestamp,
                     double value);
  std::vector<std::pair<uint64_t, double>>
  queryTimeSeries(const std::string &seriesName, long long start,
                  long long end);
  void executeSQL(const std::string &sql);

private:
  // Per-system step runners — extracted from update() to keep that loop
  // readable. Each chooses inline vs std::async based on its own flags.
  void runEcosystemStep();

  std::mutex registryMutex;
  mutable std::shared_mutex
      entityLifecycleMutex; // Protects entity creation/destruction vs
                            // perception
  std::unique_ptr<GameDBHandler> dbHandler;

  // Physics
  PhysicsEngine *physicsEngine;
  std::future<void> physicsFuture;

  // Life
  LifeEngine *lifeEngine;

  // Ecosystem
  EcosystemEngine *ecosystemEngine;
  std::future<void> ecosystemFuture;
  bool ecosystemStarted_ = false;
  bool processEcosystem_ = false;

  // MetabolismSystem
  MetabolismSystem *metabolismSystem;
  std::future<void> metabolismFuture;
  // Default true preserves the historical behaviour (when the prior
  // `const bool processMetabolismAsync = false;` was hardcoded, the sync
  // branch always ran). Setting this false skips metabolism entirely.
  bool processMetabolism_ = true;

  // HealthSystem
  HealthSystem *healthSystem;

  // CombatSystem
  CombatSystem *combatSystem;

  // EffectsSystem
  EffectsSystem *effectsSystem;

  std::vector<nb::object> pythonSystems;
  std::map<std::string, nb::object> pythonScripts;

  int getPerceptionBounds(int pos, int perception) const;
  void onTakeItemEventPython(const TakeItemEvent &event);
  void onUseItemEventPython(const UseItemEvent &event);

  // Perception helpers
  void buildInventoryItems(entt::entity entity, PerceptionResponse &response);
  EntityInterface buildEntityInterface(entt::entity entity);
  std::vector<int>
  buildTerrainView(int x_min, int y_min, int z_min, int x_max, int y_max,
                   int z_max, VoxelGridView &voxelGridView,
                   std::unordered_map<int, EntityInterface> &terrainEntities,
                   const Position &observerPos);
  void buildNonTerrainEntities(
      const std::vector<int> &entitiesIds,
      std::unordered_map<int, EntityInterface> &terrainEntities,
      PerceptionResponse &response,
      entt::view<
          entt::get_t<Position, EntityTypeComponent, PerceptionComponent>>
          allView);
  // Helper: remove entity from terrain storage. Caller MUST hold exclusive
  // `entityLifecycleMutex` before calling this.

  // Helper: acquire lifecycle lock and destroy entity handle safely.
  void destroyEntityHandleWithLifecycleLock(entt::entity entity);

  // Process queued entity deletions when safe to do so (no async tasks running)
  void processEntityDeletion();
};

#endif // WORLD_H
