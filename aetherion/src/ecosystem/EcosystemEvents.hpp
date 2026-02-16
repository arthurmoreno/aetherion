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

    EvaporateWaterEntityEvent(entt::entity entity, Position position, float sunIntensity)
        : entity(entity), position(position), sunIntensity(sunIntensity) {}
};

struct CondenseWaterEntityEvent {
    Position vaporPos;  // Position of the vapor (x, y, z)
    int condensationAmount;
    int terrainBelowId;  // Terrain ID at z-1 for handler decision

    CondenseWaterEntityEvent(Position vaporPos, int condensationAmount, int terrainBelowId)
        : vaporPos(vaporPos),
          condensationAmount(condensationAmount),
          terrainBelowId(terrainBelowId) {}
};

struct WaterFallEntityEvent {
    entt::entity entity;
    Position sourcePos;  // Position of the falling water (x, y, z)
    Position position;
    int fallingAmount;

    WaterFallEntityEvent(entt::entity entity, Position sourcePos, Position position, int fallingAmount)
        : entity(entity), sourcePos(sourcePos), position(position), fallingAmount(fallingAmount) {}
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

    WaterSpreadEvent(Position source, Position target, int amount, DirectionEnum direction,
                     EntityTypeComponent sourceType, EntityTypeComponent targetType,
                     MatterContainer sourceMatter, MatterContainer targetMatter)
        : source(source),
          target(target),
          amount(amount),
          direction(direction),
          sourceType(sourceType),
          targetType(targetType),
          sourceMatter(sourceMatter),
          targetMatter(targetMatter) {}
};

struct WaterGravityFlowEvent {
    Position source;
    Position target;
    int amount;
    int targetTerrainId;  // For soft-empty conversion check
    EntityTypeComponent sourceType;
    EntityTypeComponent targetType;
    MatterContainer sourceMatter;
    MatterContainer targetMatter;

    WaterGravityFlowEvent(Position source, Position target, int amount, int targetTerrainId,
                          EntityTypeComponent sourceType, EntityTypeComponent targetType,
                          MatterContainer sourceMatter, MatterContainer targetMatter)
        : source(source),
          target(target),
          amount(amount),
          targetTerrainId(targetTerrainId),
          sourceType(sourceType),
          targetType(targetType),
          sourceMatter(sourceMatter),
          targetMatter(targetMatter) {}
};

struct TerrainPhaseConversionEvent {
    Position position;
    int terrainId;
    EntityTypeComponent newType;
    MatterContainer newMatter;
    StructuralIntegrityComponent newStructuralIntegrity;

    TerrainPhaseConversionEvent(Position position, int terrainId, EntityTypeComponent newType,
                                MatterContainer newMatter,
                                StructuralIntegrityComponent newStructuralIntegrity)
        : position(position),
          terrainId(terrainId),
          newType(newType),
          newMatter(newMatter),
          newStructuralIntegrity(newStructuralIntegrity) {}
};

struct TerrainRemoveVelocityEvent {
    entt::entity entity;

    explicit TerrainRemoveVelocityEvent(entt::entity entity) : entity(entity) {}
};

struct TerrainRemoveMovingComponentEvent {
    entt::entity entity;

    explicit TerrainRemoveMovingComponentEvent(entt::entity entity) : entity(entity) {}
};

struct VaporCreationEvent {
    Position position;
    int amount;
    bool targetExists;

    VaporCreationEvent(Position position, int amount, bool targetExists)
        : position(position), amount(amount), targetExists(targetExists) {}
};

struct CreateVaporEntityEvent {
    Position position;
    float rhoEnv;
    float rhoVapor;

    CreateVaporEntityEvent(Position position, float rhoEnv, float rhoVapor)
        : position(position), rhoEnv(rhoEnv), rhoVapor(rhoVapor) {}
};

struct VaporMergeUpEvent {
    Position source;
    Position target;
    int amount;
    entt::entity sourceEntity;

    VaporMergeUpEvent(Position source, Position target, int amount, entt::entity sourceEntity)
        : source(source), target(target), amount(amount), sourceEntity(sourceEntity) {}
};

struct VaporMergeSidewaysEvent {
    Position source;
    Position target;
    int amount;
    int sourceTerrainId;

    VaporMergeSidewaysEvent(Position source, Position target, int amount, int sourceTerrainId)
        : source(source), target(target), amount(amount), sourceTerrainId(sourceTerrainId) {}
};

struct AddVaporToTileAboveEvent {
    Position sourcePos;  // Position of the evaporating water (x, y, z)
    int amount;          // Amount of vapor to add
    int terrainAboveId;  // Terrain ID at z+1

    AddVaporToTileAboveEvent(Position sourcePos, int amount, int terrainAboveId)
        : sourcePos(sourcePos), amount(amount), terrainAboveId(terrainAboveId) {}
};

struct MoveGasEntityEvent {
    entt::entity entity;
    bool forceApplyNewVelocity;
    Position position;
    float forceX, forceY, rhoEnv, rhoGas;

    MoveGasEntityEvent(entt::entity entity, Position position, float forceX, float forceY,
                       float rhoEnv, float rhoGas)
        : entity(entity),
          position(position),
          forceX(forceX),
          forceY(forceY),
          rhoEnv(rhoEnv),
          rhoGas(rhoGas) {
        forceApplyNewVelocity = false;
    }

    void setForceApplyNewVelocity() { forceApplyNewVelocity = true; }
};

struct DeleteOrConvertTerrainEvent {
    entt::entity terrain;

    DeleteOrConvertTerrainEvent(entt::entity terrain) : terrain(terrain) {}
};

#endif  // ECOSYSTEM_EVENTS_HPP