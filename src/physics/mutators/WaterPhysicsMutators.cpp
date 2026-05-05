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
  // ────────────────────────────────────────────────────────────────────────
  // Pre-lock guards — refuse bad inputs before we touch the repository.
  // Each guard returns early; the dropped event is harmless (water-spread
  // is high-frequency cosmetic flow, a single missed transfer is fine).
  // ────────────────────────────────────────────────────────────────────────

  // Guard 1 — coords must be inside the world.
  // OpenVDB silently accepts out-of-bounds writes by allocating phantom
  // leaves. Repeated phantom writes corrupt the tree's internal structure
  // and a later op segfaults — the leading hypothesis for the load-
  // dependent crash we are chasing.
  const auto inBounds = [&](int x, int y, int z) {
    return x >= 0 && x < voxelGrid.width && y >= 0 && y < voxelGrid.height &&
           z >= 0 && z < voxelGrid.depth;
  };
  if (!inBounds(event.source.x, event.source.y, event.source.z) ||
      !inBounds(event.target.x, event.target.y, event.target.z)) {
    spdlog::get("console")->warn(
        "[_handleWaterSpreadEvent] out-of-bounds coord — refusing "
        "(src=({},{},{}) dst=({},{},{}) world=({},{},{}))",
        event.source.x, event.source.y, event.source.z, event.target.x,
        event.target.y, event.target.z, voxelGrid.width, voxelGrid.height,
        voxelGrid.depth);
    return;
  }

  // Guard 2 — refuse self-transfer.
  // If source == target, the function reads the same cell twice into two
  // local copies, mutates them in opposite directions, then writes both
  // back — second write wins, silently creating or destroying matter.
  if (event.source.x == event.target.x && event.source.y == event.target.y &&
      event.source.z == event.target.z) {
    spdlog::get("console")->warn(
        "[_handleWaterSpreadEvent] source == target ({},{},{}) — refusing",
        event.source.x, event.source.y, event.source.z);
    return;
  }

  // Guard 3 — amount must be a sane positive integer.
  // Negative amount transfers backwards (adds to source, removes from
  // target). Huge amount overflows `int` on the addition below. The
  // ceiling is arbitrary but well above any legitimate per-tick spread.
  constexpr int kMaxReasonableSpreadAmount = 1'000'000;
  if (event.amount <= 0 || event.amount > kMaxReasonableSpreadAmount) {
    spdlog::get("console")->warn(
        "[_handleWaterSpreadEvent] suspicious amount={} at src=({},{},{}) "
        "dst=({},{},{}) — refusing",
        event.amount, event.source.x, event.source.y, event.source.z,
        event.target.x, event.target.y, event.target.z);
    return;
  }

  // ────────────────────────────────────────────────────────────────────────
  // Acquire the lock and re-read fresh state. Post-read guards 4 and 5
  // depend on the up-to-date matter values.
  // ────────────────────────────────────────────────────────────────────────
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  // Re-read current repository state to avoid TOCTOU races
  MatterContainer currentSource =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.source.x, event.source.y, event.source.z);
  MatterContainer currentTarget =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(
          event.target.x, event.target.y, event.target.z);

  // ────────────────────────────────────────────────────────────────────────
  // Bulletproof refuse paths — no spdlog calls, no ostringstream temporaries.
  // The crash trace showed segfaults occurring on the refuse path of these
  // guards (between `after_reads` and the RAII destructor's `end` log),
  // most plausibly inside spdlog::warn or in the ostringstream destructor.
  // The diagnostic data is already in `water_debug.jsonl` via the
  // `after_reads` probe + handler RAII begin/end pairs, so the silent
  // returns lose nothing observability-wise. Re-enable the warns once the
  // upstream producer (Part B) stops emitting the bad events.
  // ────────────────────────────────────────────────────────────────────────

  // Guard 4 — source must actually be liquid water.
  if (!(currentSource.WaterMatter > 0 && currentSource.WaterVapor == 0)) {
    return;
  }

  // Guard 5 — refuse to propagate an already-violated source (Rule 4
  // third pattern: "when the source itself is inconsistent, do not
  // propagate the violation").
  if (currentSource.WaterMatter > 0 && currentSource.WaterVapor > 0) {
    return;
  }

  // Original guards (kept) — source has enough water for the transfer,
  // target is not currently a vapor cell.
  if (currentSource.WaterMatter < event.amount) {
    return; // Source no longer has required amount
  }
  if (currentTarget.WaterVapor > 0) {
    // TOCTOU race: the dispatch site (`spreadWater` in `EcosystemEngine.cpp`)
    // verifies `targetMatter.WaterVapor == 0` before enqueuing this event,
    // but between that dispatch (tick N) and this handler firing (tick N+1),
    // another handler — typically an evap that runs first in the same
    // dispatcher.update() and creates vapor above its source via
    // `addOrCreateVaporAbove` — can write WaterVapor>0 to our target.
    //
    // We refuse silently (no spdlog warn — that path historically segfaulted
    // here under heavy load; see water_debug.jsonl analysis 2026-05-03).
    // The source's water unit stays at the source — no matter is lost in
    // this branch since we return BEFORE any write.
    //
    // TODO(game-engine-polish): when the simulation matures, address the
    // upstream race directly so spread events aren't enqueued for paths
    // that will later be invalidated by sibling handlers in the same tick.
    // Options being considered: (a) re-validate target state inside the
    // handler and re-route the spread to a different free neighbor;
    // (b) order the dispatcher so spreads precede vapor-creating handlers;
    // (c) accept the loss of one spread per race as cosmetic and track
    // total dropped-spread count in PhysicsMetrics for visibility.
    return; // Target currently has vapor; abort transfer
  }

  // Apply transfer using up-to-date state
  currentTarget.WaterMatter += event.amount;
  currentSource.WaterMatter -= event.amount;

  // Update both voxels atomically
  _logIfViolatingMatterWrite("_handleWaterSpreadEvent:target", event.target.x,
                             event.target.y, event.target.z, currentTarget);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.target.x, event.target.y, event.target.z, currentTarget);

  _logIfViolatingMatterWrite("_handleWaterSpreadEvent:source", event.source.x,
                             event.source.y, event.source.z, currentSource);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.source.x, event.source.y, event.source.z, currentSource);
}

void setGravityFlowWaterTargetDefaults(VoxelGrid &voxelGrid,
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

void _handleTerrainPhaseConversionEvent(
    VoxelGrid &voxelGrid, const TerrainPhaseConversionEvent &event) {
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
  _logIfViolatingMatterWrite("_handleTerrainPhaseConversionEvent",
                             event.position.x, event.position.y,
                             event.position.z, event.newMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      event.position.x, event.position.y, event.position.z, event.newMatter);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      event.position.x, event.position.y, event.position.z,
      event.newStructuralIntegrity);
}

void setVaporSI(int x, int y, int z, VoxelGrid &voxelGrid) {
  StructuralIntegrityComponent terrainSI =
      voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
  terrainSI.canStackEntities = false;
  terrainSI.maxLoadCapacity = -1;
  terrainSI.matterState = MatterState::GAS;
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                 terrainSI);
}

void createVaporTerrainEntity(entt::registry &registry, VoxelGrid &voxelGrid,
                              int x, int y, int z, int vaporAmount) {
  (void)registry; // No EnTT entity is created — vapor lives in VDB only.
  if (!voxelGrid.terrainGridRepository) {
    spdlog::warn("createVaporTerrainEntity: missing terrainGridRepository");
    return;
  }
  Position newPosition = {x, y, z, DirectionEnum::DOWN};

  EntityTypeComponent newType = {};
  newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
  newType.subType0 = static_cast<int>(TerrainEnum::WATER);
  newType.subType1 = 0;

  MatterContainer newMatterContainer = {};
  newMatterContainer.WaterVapor = vaporAmount;
  newMatterContainer.WaterMatter = 0;

  PhysicsStats newPhysicsStats = {};
  // mass is stored in an Int32Grid in `TerrainStorage`, so fractional values
  // truncate on `setPhysicsStats`. Pre-#41 mass=0.1 worked because PhysicsStats
  // was a float-field ECS component; post-migration `int(0.1) == 0` and the
  // gas handler then computes `accelerationX = forceX / 0` → NaN, polluting
  // the velocity grid and silently stalling vapor. Use the smallest integer
  // that survives truncation while keeping vapor lighter than water (mass=20).
  newPhysicsStats.mass = 1;
  newPhysicsStats.maxSpeed = 10;
  newPhysicsStats.minSpeed = 0.0;

  StructuralIntegrityComponent newStructuralIntegrityComponent = {};
  newStructuralIntegrityComponent.canStackEntities = false;
  newStructuralIntegrityComponent.maxLoadCapacity = -1;
  newStructuralIntegrityComponent.matterState = MatterState::GAS;

  voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
  voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(
      x, y, z, newMatterContainer);
  voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
      x, y, z, newStructuralIntegrityComponent);
  voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);
  voxelGrid.terrainGridRepository->setTerrainId(
      x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
}

void _nudgeSettledWaterAfterDrain(VoxelGrid &voxelGrid, int drainedX,
                                  int drainedY, int drainedZ) {
  const int ax = drainedX;
  const int ay = drainedY;
  const int az = drainedZ + 1;

  const int aboveId = voxelGrid.getTerrain(ax, ay, az);
  if (aboveId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
    return;
  }

  const MatterState aboveMs =
      voxelGrid.terrainGridRepository->getMatterState(ax, ay, az);
  if (aboveMs != MatterState::LIQUID) {
    return;
  }

  const MatterContainer aboveMatter =
      voxelGrid.terrainGridRepository->getTerrainMatterContainer(ax, ay, az);
  if (aboveMatter.WaterMatter <= 0) {
    return;
  }

  const Velocity aboveVel =
      voxelGrid.terrainGridRepository->getVelocity(ax, ay, az);
  if (aboveVel.vx != 0.0f || aboveVel.vy != 0.0f || aboveVel.vz != 0.0f) {
    return; // already moving — the velocity-driven pass will handle it
  }

  const float gravityKick = PhysicsManager::Instance()->getGravity();
  voxelGrid.terrainGridRepository->setVelocity(
      ax, ay, az, Velocity{0.0f, 0.0f, -gravityKick});
}

void _handleWaterCreationEvent(entt::registry &registry, EventSink &sink,
                               VoxelGrid &voxelGrid,
                               const WaterCreationEvent &event) {
  (void)registry; // No EnTT entity is created — water lives in VDB only.
  if (!voxelGrid.terrainGridRepository) {
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] missing terrainGridRepository");
    return;
  }
  TerrainGridLock lock(voxelGrid.terrainGridRepository.get());

  const int x = event.position.x;
  const int y = event.position.y;
  const int z = event.position.z;

  const int64_t terrainId = voxelGrid.getTerrain(x, y, z);
  const bool destinationIsEmpty =
      terrainId == static_cast<int64_t>(TerrainIdTypeEnum::NONE);

  EntityTypeComponent destType{};
  MatterContainer destMatter{};
  if (!destinationIsEmpty) {
    destType = voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
    destMatter =
        voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
  }

  // Refusal path: cell exists but does not hold liquid water in any
  // currently-modelled form. Accept WATER-typed terrain (the obvious
  // case) and GRASS-typed terrain (water saturating soil — the engine's
  // evaporation branch in `processTileWater` already treats
  // grass + WaterMatter as a valid steady state, so additive merges into
  // grass are physically meaningful and consistent with the rest of the
  // simulation). Stone, transitory empty, plant terrain, etc. still
  // refuse: water cannot land there.
  const bool isWaterCarryingTerrain =
      destType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
      (destType.subType0 == static_cast<int>(TerrainEnum::WATER) ||
       destType.subType0 == static_cast<int>(TerrainEnum::GRASS));
  if (!destinationIsEmpty && !isWaterCarryingTerrain) {
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] Cell at ({}, {}, {}) is neither WATER "
        "nor GRASS (mainType={}, subType0={}); skipping water creation "
        "amount={}.",
        x, y, z, destType.mainType, destType.subType0, event.amount);
    return;
  }

  // Refusal path with retry: cell holds pure vapor. Re-enqueue with
  // retryCount + 1 so the surrounding vapor has a tick to disperse, abort
  // with a warning once `WATER_VAPOR_CONFLICT_RETRY_LIMIT` is reached.
  const bool destinationIsVaporOnly = !destinationIsEmpty &&
                                      destMatter.WaterVapor > 0 &&
                                      destMatter.WaterMatter <= 0;
  if (destinationIsVaporOnly) {
    if (event.retryCount < WATER_VAPOR_CONFLICT_RETRY_LIMIT) {
      sink.enqueue<WaterCreationEvent>(WaterCreationEvent{
          event.position, event.amount, event.retryCount + 1});
      return;
    }
    spdlog::get("console")->warn(
        "[_handleWaterCreationEvent] Vapor at ({}, {}, {}) did not clear "
        "after {} retries; aborting water creation amount={}.",
        x, y, z, WATER_VAPOR_CONFLICT_RETRY_LIMIT, event.amount);
    return;
  }

  // Path A: empty destination — write the full water-terrain scaffolding
  // and seed an initial gravity velocity if the cell below is empty so the
  // newly-spawned water enters the velocity-driven physics pass.
  if (destinationIsEmpty) {
    Position newPosition = {x, y, z, DirectionEnum::DOWN};

    EntityTypeComponent newType = {};
    newType.mainType = static_cast<int>(EntityEnum::TERRAIN);
    newType.subType0 = static_cast<int>(TerrainEnum::WATER);
    newType.subType1 = 0;

    PhysicsStats newPhysicsStats = {};
    newPhysicsStats.mass = 20;
    newPhysicsStats.maxSpeed = 10;
    newPhysicsStats.minSpeed = 0.0;

    StructuralIntegrityComponent newSIC = {};
    newSIC.canStackEntities = false;
    newSIC.maxLoadCapacity = -1;
    newSIC.matterState = MatterState::LIQUID;

    voxelGrid.terrainGridRepository->setPosition(x, y, z, newPosition);
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, newType);
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z,
                                                                   newSIC);
    voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, newPhysicsStats);
    voxelGrid.terrainGridRepository->setTerrainId(
        x, y, z, static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));

    MatterContainer freshMatter{};
    freshMatter.WaterMatter = event.amount;
    _logIfViolatingMatterWrite("_handleWaterCreationEvent:freshCell", x, y, z,
                               freshMatter);
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                               freshMatter);

    if (z > 0 && voxelGrid.getTerrain(x, y, z - 1) ==
                     static_cast<int64_t>(TerrainIdTypeEnum::NONE)) {
      const float gravityKick = PhysicsManager::Instance()->getGravity();
      voxelGrid.terrainGridRepository->setVelocity(
          x, y, z, Velocity{0.0f, 0.0f, -gravityKick});
    }
    return;
  }

  // Path B: existing liquid-water cell — additive matter merge.
  destMatter.WaterMatter += event.amount;
  _logIfViolatingMatterWrite("_handleWaterCreationEvent:additiveMerge", x, y, z,
                             destMatter);
  voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                             destMatter);
}
