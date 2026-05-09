#ifndef PHYSICSENGINE_HPP
#define PHYSICSENGINE_HPP

#include <tbb/concurrent_queue.h>

#include <chrono>
#include <entt/entt.hpp>
#include <mutex>
#include <string>

#include "EcosystemEngine.hpp"
#include "EventSink.hpp"
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
#include "diag/Diag.hpp"
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
struct WaterCreationEvent;
struct VaporMergeUpEvent;
struct VaporMergeSidewaysEvent;
struct AddVaporToTileAboveEvent;
struct PlantWaterUptakeEvent;

struct SetPhysicsEntityToDebug {
  entt::entity entity;

  SetPhysicsEntityToDebug(entt::entity entity) : entity(entity) {}
};

class PhysicsEngine {
public:
  std::mutex physicsMutex;

  entt::entity entityBeingDebugged;

  PhysicsEngine() = default;
  PhysicsEngine(entt::registry &reg, EventSink &sinkRef, VoxelGrid *voxelGrid)
      : registry(reg), sink(sinkRef), voxelGrid(voxelGrid) {}

  // Method to process physics-related events
  void processPhysics(entt::registry &registry, VoxelGrid &voxelGrid,
                      EventSink &sink, GameClock &clock);
  void processPhysicsAsync(entt::registry &registry, VoxelGrid &voxelGrid,
                           EventSink &sink, GameClock &clock);

  // Iteration helpers used by `processPhysics` / `processPhysicsAsync`.
  // The orchestrator methods call them in sequence; each handles one
  // iteration source so the two iteration models (ECS view vs. VDB grid
  // scan) stay clearly separated.

  // Gravity-force enqueue for ECS-backed entities. Outer iterates
  // `registry.view<Position>()`; inner is the per-entity decision tree.
  void applyGravityForcesToECSEntities(entt::registry &registry,
                                       VoxelGrid &voxelGrid, EventSink &sink);
  void applyGravityForceToEntity(entt::entity entity, entt::registry &registry,
                                 VoxelGrid &voxelGrid, EventSink &sink);

  // Velocity processing for ECS-backed entities. Outer iterates
  // `registry.view<Velocity>()`; inner runs per-entity validation +
  // cold-vapor revival + `handleMovement`.
  void processVelocityForECSEntities(entt::registry &registry,
                                     VoxelGrid &voxelGrid, EventSink &sink);
  void processVelocityForEntity(entt::entity entity, entt::registry &registry,
                                VoxelGrid &voxelGrid, EventSink &sink);

  // Velocity processing for ON_GRID_STORAGE voxels driven by the VDB
  // velocity grid. Outer two-phase: collects active VDB voxels (skipping
  // any whose terrainId points at an ECS entity, since the ECS pass
  // already handled them), then processes each. Inner runs the per-voxel
  // velocity update + movement trigger.
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
  void onWaterCreationEvent(const WaterCreationEvent &event);
  void onVaporMergeUpEvent(const VaporMergeUpEvent &event);
  void onVaporMergeSidewaysEvent(const VaporMergeSidewaysEvent &event);
  void onAddVaporToTileAboveEvent(const AddVaporToTileAboveEvent &event);
  void onDeleteOrConvertTerrainEvent(const DeleteOrConvertTerrainEvent &event);
  void onPlantWaterUptakeEvent(const PlantWaterUptakeEvent &event);

  // Register the event handler
  void registerEventHandlers(entt::dispatcher &dispatcher);
  void registerVoxelGrid(VoxelGrid *voxelGrid) { this->voxelGrid = voxelGrid; }

  // Register diag::Counter handles for every physics metric. Call once
  // after `aetherion::diag::Registry::instance().initialize(...)`. Safe
  // to call before initialise — counters with GameDBSink will silently
  // drop samples until a handler is wired in.
  void registerDiagCounters();

  bool isProcessingComplete() const;

  // Per-event counters owned by this engine. Names match the legacy
  // `physics_*` GameDB series so historical data and existing GUI plots
  // continue to work.
  struct PhysicsCounters {
    aetherion::diag::Counter move_gas_entity;
    aetherion::diag::Counter move_solid_entity;
    aetherion::diag::Counter evaporate_water_entity;
    aetherion::diag::Counter condense_water_entity;
    aetherion::diag::Counter water_fall_entity;
    aetherion::diag::Counter water_spread;
    aetherion::diag::Counter water_gravity_flow;
    aetherion::diag::Counter terrain_phase_conversion;
    aetherion::diag::Counter vapor_creation;
    aetherion::diag::Counter water_creation;
    aetherion::diag::Counter vapor_merge_up;
    aetherion::diag::Counter vapor_merge_sideways;
    aetherion::diag::Counter add_vapor_to_tile_above;
    aetherion::diag::Counter delete_or_convert_terrain;
    aetherion::diag::Counter invalid_terrain_found;
    aetherion::diag::Counter plant_water_uptake;
  };
  PhysicsCounters counters_;

private:
  entt::registry &registry;
  EventSink &sink; // routes enqueue by thread (main → direct, other → staging)
  VoxelGrid *voxelGrid = nullptr;

  // Mutex for thread safety
  bool processingComplete = true; // Flag to indicate processing state

  // Private helper methods
  bool checkIfCanJump(const MoveSolidEntityEvent &event);
};

#endif // PHYSICSENGINE_HPP