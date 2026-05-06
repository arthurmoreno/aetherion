#ifndef ECOSYSTEM_EVENTS_HPP
#define ECOSYSTEM_EVENTS_HPP

#include <entt/entt.hpp>

#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/PlantsComponents.hpp"
#include "components/TerrainComponents.hpp"

struct EvaporateWaterEntityEvent {
  entt::entity entity;
  Position position;
  float sunIntensity;

  EvaporateWaterEntityEvent(entt::entity entity, Position position,
                            float sunIntensity)
      : entity(entity), position(position), sunIntensity(sunIntensity) {}
};

struct CondenseWaterEntityEvent {
  Position vaporPos; // Position of the vapor (x, y, z)
  int condensationAmount;
  int terrainBelowId; // Terrain ID at z-1 for handler decision
  // Number of times this condensation has already been re-dispatched
  // after hitting an unresolved vapor cell at the intended destination.
  // Bounded by `createWaterTerrainBelowVapor` to prevent infinite loops
  // on sealed vapor stacks.
  int retryCount;

  CondenseWaterEntityEvent(Position vaporPos, int condensationAmount,
                           int terrainBelowId, int retryCount = 0)
      : vaporPos(vaporPos), condensationAmount(condensationAmount),
        terrainBelowId(terrainBelowId), retryCount(retryCount) {}
};

struct WaterFallEntityEvent {
  entt::entity entity;
  Position sourcePos; // Position of the falling water (x, y, z)
  Position position;
  int fallingAmount;
  // Number of times this fall has already been re-dispatched after
  // hitting an unresolved vapor destination. Bounded by
  // `createWaterTerrainFromFall` to prevent infinite loops on sealed
  // vapor pockets.
  int retryCount;

  WaterFallEntityEvent(entt::entity entity, Position sourcePos,
                       Position position, int fallingAmount, int retryCount = 0)
      : entity(entity), sourcePos(sourcePos), position(position),
        fallingAmount(fallingAmount), retryCount(retryCount) {}
};

struct WaterSpreadEvent {
  Position source;
  Position target;
  int amount;
  DirectionEnum direction;
  EntityTypeComponent sourceType;
  EntityTypeComponent targetType;
  MatterContainer sourceMatter;
  MatterContainer targetMatter;

  WaterSpreadEvent(Position source, Position target, int amount,
                   DirectionEnum direction, EntityTypeComponent sourceType,
                   EntityTypeComponent targetType, MatterContainer sourceMatter,
                   MatterContainer targetMatter)
      : source(source), target(target), amount(amount), direction(direction),
        sourceType(sourceType), targetType(targetType),
        sourceMatter(sourceMatter), targetMatter(targetMatter) {}
};

struct WaterGravityFlowEvent {
  Position source;
  Position target;
  int amount;
  int targetTerrainId; // For soft-empty conversion check
  EntityTypeComponent sourceType;
  EntityTypeComponent targetType;
  MatterContainer sourceMatter;
  MatterContainer targetMatter;

  WaterGravityFlowEvent(Position source, Position target, int amount,
                        int targetTerrainId, EntityTypeComponent sourceType,
                        EntityTypeComponent targetType,
                        MatterContainer sourceMatter,
                        MatterContainer targetMatter)
      : source(source), target(target), amount(amount),
        targetTerrainId(targetTerrainId), sourceType(sourceType),
        targetType(targetType), sourceMatter(sourceMatter),
        targetMatter(targetMatter) {}
};

struct TerrainPhaseConversionEvent {
  Position position;
  int terrainId;
  EntityTypeComponent newType;
  MatterContainer newMatter;
  StructuralIntegrityComponent newStructuralIntegrity;

  TerrainPhaseConversionEvent(
      Position position, int terrainId, EntityTypeComponent newType,
      MatterContainer newMatter,
      StructuralIntegrityComponent newStructuralIntegrity)
      : position(position), terrainId(terrainId), newType(newType),
        newMatter(newMatter), newStructuralIntegrity(newStructuralIntegrity) {}
};

struct TerrainRemoveVelocityEvent {
  entt::entity entity;

  explicit TerrainRemoveVelocityEvent(entt::entity entity) : entity(entity) {}
};

struct TerrainRemoveMovingComponentEvent {
  entt::entity entity;

  explicit TerrainRemoveMovingComponentEvent(entt::entity entity)
      : entity(entity) {}
};

struct VaporCreationEvent {
  Position position;
  int amount;
  bool targetExists;

  VaporCreationEvent(Position position, int amount, bool targetExists)
      : position(position), amount(amount), targetExists(targetExists) {}
};

// Materialise liquid water at a coordinate. Used by sources that do not
// drain from another cell — e.g., `SpringWaterSystem`, scripted weather,
// future rain. The handler chooses one of three branches based on the
// current state of `position`:
//   - cell is NONE (air)   -> writes the full water-terrain scaffolding
//                              and seeds an initial gravity velocity if
//                              the cell below is empty.
//   - cell is liquid water -> additive merge: WaterMatter += amount.
//   - cell holds vapor     -> retry-then-abort: re-enqueue with
//                              retryCount + 1 up to
//                              WATER_VAPOR_CONFLICT_RETRY_LIMIT, then
//                              warn-log and drop.
// `retryCount` is bounded to prevent unbounded re-dispatch on a sealed
// vapor pocket.
struct WaterCreationEvent {
  Position position;
  int amount;
  int retryCount;

  WaterCreationEvent(Position position, int amount, int retryCount = 0)
      : position(position), amount(amount), retryCount(retryCount) {}
};

struct VaporMergeUpEvent {
  Position source;
  Position target;
  int amount;
  entt::entity sourceEntity;

  VaporMergeUpEvent(Position source, Position target, int amount,
                    entt::entity sourceEntity)
      : source(source), target(target), amount(amount),
        sourceEntity(sourceEntity) {}
};

struct VaporMergeSidewaysEvent {
  Position source;
  Position target;
  int amount;
  int sourceTerrainId;

  VaporMergeSidewaysEvent(Position source, Position target, int amount,
                          int sourceTerrainId)
      : source(source), target(target), amount(amount),
        sourceTerrainId(sourceTerrainId) {}
};

struct AddVaporToTileAboveEvent {
  Position sourcePos; // Position of the evaporating water (x, y, z)
  int amount;         // Amount of vapor to add
  int terrainAboveId; // Terrain ID at z+1

  AddVaporToTileAboveEvent(Position sourcePos, int amount, int terrainAboveId)
      : sourcePos(sourcePos), amount(amount), terrainAboveId(terrainAboveId) {}
};

struct MoveGasEntityEvent {
  entt::entity entity;
  bool forceApplyNewVelocity;
  Position position;
  float forceX, forceY, rhoEnv, rhoGas;

  MoveGasEntityEvent(entt::entity entity, Position position, float forceX,
                     float forceY, float rhoEnv, float rhoGas)
      : entity(entity), position(position), forceX(forceX), forceY(forceY),
        rhoEnv(rhoEnv), rhoGas(rhoGas) {
    forceApplyNewVelocity = false;
  }

  void setForceApplyNewVelocity() { forceApplyNewVelocity = true; }
};

struct DeleteOrConvertTerrainEvent {
  entt::entity terrain;
  Position position;

  DeleteOrConvertTerrainEvent(entt::entity terrain, Position position)
      : terrain(terrain), position(position) {}
};

// Issued by the async ecosystem worker when a plant sits on a grass cell that
// could donate water. The worker only reads thread-safe terrain-grid state
// (cell type + terrain id) before enqueueing; the handler runs on the main
// thread and is the only one that touches `PlantResources` and writes back
// to the grass cell's `MatterContainer`.
struct PlantWaterUptakeEvent {
  Position grassCell;
  entt::entity plantEntity;

  PlantWaterUptakeEvent(Position grassCell, entt::entity plantEntity)
      : grassCell(grassCell), plantEntity(plantEntity) {}
};

#endif // ECOSYSTEM_EVENTS_HPP