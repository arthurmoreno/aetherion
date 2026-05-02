// Implementation of the water-simulation cluster of physics mutators.
//
// All declarations live in `physics/PhysicsMutators.hpp` so callers
// (`PhysicsEngine.cpp`, `EcosystemEngine.cpp`, ...) keep including a single
// header. Bodies are moved here one at a time, byte-identical to the prior
// inline definition (only the `inline` keyword is dropped on the signature
// line).

#include "physics/PhysicsMutators.hpp"

void _handleWaterSpreadEvent(VoxelGrid &voxelGrid,
                                    const WaterSpreadEvent &event) {
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Re-read current repository state to avoid TOCTOU races
  MatterContainer currentSource =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.source.x, event.source.y, event.source.z);
  MatterContainer currentTarget =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.target.x, event.target.y, event.target.z);

  // Validate that the transfer is still possible: source has enough water
  // and target does not currently contain vapor (flow into vapor is invalid).
  if (currentSource.WaterMatter < event.amount) {
    spdlog::get("console")->warn("[_handleWaterSpreadEvent] Source no longer "
                                 "has required amount of water.");
    return; // Source no longer has required amount
  }
  if (currentTarget.WaterVapor > 0) {
    spdlog::get("console")->warn("[_handleWaterSpreadEvent] Target currently "
                                 "has vapor; aborting transfer.");
    return; // Target currently has vapor; abort transfer
  }

  // Apply transfer using up-to-date state
  currentTarget.WaterMatter += event.amount;
  currentSource.WaterMatter -= event.amount;

  // Update both voxels atomically
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.target.x, event.target.y, event.target.z, currentTarget);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.source.x, event.source.y, event.source.z, currentSource);
}

void
setGravityFlowWaterTargetDefaults(VoxelGrid &voxelGrid,
                                  const Position &targetPos,
                                  const PhysicsStats &physicsStats) {
  EntityTypeComponent targetType = {};
  targetType.mainType = static_cast<int>(EntityEnum::TERRAIN);
  targetType.subType0 = static_cast<int>(TerrainEnum::WATER);
  targetType.subType1 = 0;

  StructuralIntegrityComponent targetStructuralIntegrity = {};
  targetStructuralIntegrity.canStackEntities = false;
  targetStructuralIntegrity.maxLoadCapacity = -1;
  targetStructuralIntegrity.matterState = MatterState::LIQUID;

  voxelGrid.terrainGridRepository->setPosition(targetPos.x, targetPos.y,
                                               targetPos.z, targetPos);
  voxelGrid.terrainGridRepository->setTerrainEntityType(
      targetPos.x, targetPos.y, targetPos.z, targetType);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      targetPos.x, targetPos.y, targetPos.z, targetStructuralIntegrity);
  voxelGrid.terrainGridRepository->setPhysicsStats(targetPos.x, targetPos.y,
                                                   targetPos.z, physicsStats);
}

void setGravityFlowEmptySourceDefaults(VoxelGrid &voxelGrid,
                                              const Position &sourcePos) {
  EntityTypeComponent emptyType = {};
  emptyType.mainType = static_cast<int>(EntityEnum::TERRAIN);
  emptyType.subType0 = static_cast<int>(TerrainEnum::EMPTY);
  emptyType.subType1 = 0;

  MatterContainer emptyMatter = {};
  emptyMatter.TerrainMatter = 0;
  emptyMatter.WaterMatter = 0;
  emptyMatter.WaterVapor = 0;
  emptyMatter.BioMassMatter = 0;

  StructuralIntegrityComponent emptyStructuralIntegrity = {};
  emptyStructuralIntegrity.canStackEntities = false;
  emptyStructuralIntegrity.maxLoadCapacity = -1;
  emptyStructuralIntegrity.matterState = MatterState::GAS;

  PhysicsStats emptyPhysicsStats = {};

  voxelGrid.terrainGridRepository->setTerrainId(
      sourcePos.x, sourcePos.y, sourcePos.z,
      static_cast<int>(TerrainIdTypeEnum::NONE), false);
  voxelGrid.terrainGridRepository->setTerrainEntityType(
      sourcePos.x, sourcePos.y, sourcePos.z, emptyType);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      sourcePos.x, sourcePos.y, sourcePos.z, emptyMatter);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      sourcePos.x, sourcePos.y, sourcePos.z, emptyStructuralIntegrity);
  voxelGrid.terrainGridRepository->setPhysicsStats(
      sourcePos.x, sourcePos.y, sourcePos.z, emptyPhysicsStats);
}

void
_handleTerrainPhaseConversionEvent(VoxelGrid &voxelGrid,
                                   const TerrainPhaseConversionEvent &event) {
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Re-read current repository state to avoid TOCTOU races and validate
  // conversion
  EntityTypeComponent currentType =
      voxelGrid.terrainGridRepository->getTerrainEntityType(
          event.position.x, event.position.y, event.position.z);
  MatterContainer currentMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.position.x, event.position.y, event.position.z);
  StructuralIntegrityComponent currentSI =
      voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(
          event.position.x, event.position.y, event.position.z);

  // Basic validation: avoid converting to water if target currently has vapor,
  // and avoid converting to vapor if target currently has liquid water.
  if (event.newMatter.WaterMatter > 0) {
    if (currentMatter.WaterVapor > 0) {
      return; // conflict: target currently contains vapor
    }
  }
  if (event.newMatter.WaterVapor > 0) {
    if (currentMatter.WaterMatter > 0) {
      return; // conflict: target currently contains liquid water
    }
  }

  // Optionally could validate structural integrity preconditions here
  (void)currentSI; // keep unused-variable warnings away if not used

  // Apply terrain phase conversion (safe under lock)
  voxelGrid.terrainGridRepository->setTerrainEntityType(
      event.position.x, event.position.y, event.position.z, event.newType);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.position.x, event.position.y, event.position.z, event.newMatter);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      event.position.x, event.position.y, event.position.z,
      event.newStructuralIntegrity);
}
