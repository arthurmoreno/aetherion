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
  PhysicsEngine(entt::registry &reg, entt::dispatcher &disp,
                VoxelGrid *voxelGrid)
      : registry(reg), dispatcher(disp), voxelGrid(voxelGrid) {}

  // Method to process physics-related events
  void processPhysics(entt::registry &registry, VoxelGrid &voxelGrid,
                      entt::dispatcher &dispatcher, GameClock &clock);
  void processPhysicsAsync(entt::registry &registry, VoxelGrid &voxelGrid,
                           entt::dispatcher &dispatcher, GameClock &clock);

  // ── Iteration helpers extracted from processPhysics / processPhysicsAsync ──
  // These are pure code-organization seams: bodies are byte-identical to the
  // original inlined loops (only `continue` → `return` translation on early
  // exits, since each "inner" method is now a per-iteration callable). They
  // are kept private; the orchestrator methods call them in sequence.

  // C3.0 — gravity-force enqueue for ECS-backed entities (used by
  // `processPhysicsAsync`). Outer iterates `registry.view<Position>()`; inner
  // is the per-entity decision tree.
  void applyGravityForcesToECSEntities(entt::registry &registry,
                                       VoxelGrid &voxelGrid,
                                       entt::dispatcher &dispatcher);
  void applyGravityForceToEntity(entt::entity entity,
                                 entt::registry &registry,
                                 VoxelGrid &voxelGrid,
                                 entt::dispatcher &dispatcher);

  // Velocity processing (Loop 1) for ECS-backed entities (used by
  // `processPhysics`). Outer iterates `registry.view<Velocity>()`; inner
  // runs per-entity validation + cold-vapor revival + `handleMovement`.
  void processVelocityForECSEntities(entt::registry &registry,
                                     VoxelGrid &voxelGrid,
                                     entt::dispatcher &dispatcher);
  void processVelocityForEntity(entt::entity entity,
                                entt::registry &registry,
                                VoxelGrid &voxelGrid,
                                entt::dispatcher &dispatcher);

  // Velocity processing (Loop 2) for ON_GRID_STORAGE voxels driven by the
  // VDB velocity grid (used by `processPhysics`). Outer two-phase: collects
  // active VDB voxels (skipping any whose terrainId points at an ECS entity
  // — Loop 1 already handled them), then processes each. Inner runs the
  // per-voxel velocity update + movement trigger.
  void processVelocityForVDBVoxels(entt::registry &registry,
                                   VoxelGrid &voxelGrid);
  void processVelocityForVoxel(int x, int y, int z, float vx, float vy,
                               float vz, entt::registry &registry,
                               VoxelGrid &voxelGrid);

  // Example: Applying force to an entity
  // void applyForce(entt::registry& registry, VoxelGrid& voxelGrid,
  // entt::entity entity, float fx, float fy, float fz);

  // Handle entity movement event
  void onMoveSolidEntityEvent(const MoveSolidEntityEvent &event);
  void onMoveSolidLiquidTerrainEvent(const MoveSolidLiquidTerrainEvent &event);
  void onMoveGasEntityEvent(const MoveGasEntityEvent &event);
  void onTakeItemEvent(const TakeItemEvent &event);
  void onUseItemEvent(const UseItemEvent &event);
  void onSetPhysicsEntityToDebug(const SetPhysicsEntityToDebug &event);
  void onInvalidTerrainFound(const InvalidTerrainFoundEvent &event);

  // Handle water phase change events (moved from EcosystemEngine)
  void onEvaporateWaterEntityEvent(const EvaporateWaterEntityEvent &event);
  void onCondenseWaterEntityEvent(const CondenseWaterEntityEvent &event);
  void onWaterFallEntityEvent(const WaterFallEntityEvent &event);
  void onWaterSpreadEvent(const WaterSpreadEvent &event);
  void onWaterGravityFlowEvent(const WaterGravityFlowEvent &event);
  void onTerrainPhaseConversionEvent(const TerrainPhaseConversionEvent &event);
  void onVaporCreationEvent(const VaporCreationEvent &event);
  void onVaporMergeUpEvent(const VaporMergeUpEvent &event);
  void onVaporMergeSidewaysEvent(const VaporMergeSidewaysEvent &event);
  void onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent &event);
  void onCreateVaporEntityEvent(const CreateVaporEntityEvent &event);
  void onDeleteOrConvertTerrainEvent(const DeleteOrConvertTerrainEvent &event);

  // Register the event handler
  void registerEventHandlers(entt::dispatcher &dispatcher);
  void registerVoxelGrid(VoxelGrid *voxelGrid) { this->voxelGrid = voxelGrid; }

  // Metrics flush to game DB
  void flushPhysicsMetrics(GameDBHandler *dbHandler);

  // Increment a named metric
  void incPhysicsMetric(const std::string &metricName);

  bool isProcessingComplete() const;

private:
  entt::registry &registry;
  entt::dispatcher &dispatcher;
  VoxelGrid *voxelGrid = nullptr;

  // Monitoring counters for physics events
  std::unordered_map<std::string, uint64_t> physicsMetrics_;
  std::mutex metricsMutex_;

  // Mutex for thread safety
  bool processingComplete = true; // Flag to indicate processing state

  // Private helper methods
  bool checkIfCanJump(const MoveSolidEntityEvent &event);
};

#endif // PHYSICSENGINE_HPP