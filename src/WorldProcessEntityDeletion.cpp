#include "World.hpp"

#include "physics/PhysicsMutators.hpp"

static void processVelocityRemovals(entt::registry &registry,
                                    LifeEngine &lifeEngine) {
  auto console = spdlog::get("console");
  if (!console)
    console = spdlog::stdout_color_mt("console");

  console->debug("--- PHASE 1: Removing Velocity components ---");
  int velocityRemoved = 0;
  int velocitySkipped = 0;
  for (const auto &[entity, _] : lifeEngine.entitiesToRemoveVelocity) {
    int entityId = static_cast<int>(entity);
    bool valid = registry.valid(entity);
    bool hasVelocity = valid && registry.all_of<Velocity>(entity);

    if (hasVelocity) {
      registry.remove<Velocity>(entity);
      console->debug("[Velocity] Removed from entity {}", entityId);
      velocityRemoved++;
    } else {
      console->debug("[Velocity] Skipped entity {} (valid={}, hasVelocity={})",
                     entityId, valid, hasVelocity);
      velocitySkipped++;
    }
  }
  console->debug("[Velocity Summary] Removed: {}, Skipped: {}", velocityRemoved,
                 velocitySkipped);
  lifeEngine.entitiesToRemoveVelocity.clear();
}

namespace {

struct MovingRemovalDecision {
  int entity_id;
  bool valid;
  bool has_moving;
};

struct MovingRemovalStats {
  int removed = 0;
  int skipped = 0;
};

enum class DeletionIssueReason {
  kDestroyException = 1,
  kSpecialId = 2,
  kForceDestroyException = 3,
};

struct EntityDeletionDecision {
  entt::entity entity;
  int entity_id;
  bool soft_kill;
  bool is_special_id;
  bool is_valid_entity;
};

struct EntityDeletionStats {
  int successful_deletions = 0;
  int skipped_deletions = 0;
  int grid_mismatches = 0;
  std::map<int, int> deletion_reasons;
};

MovingRemovalDecision inspectMovingRemoval(entt::registry &registry,
                                           entt::entity entity) {
  const bool valid = registry.valid(entity);
  const bool hasMoving = valid && registry.all_of<MovingComponent>(entity);
  return {static_cast<int>(entity), valid, hasMoving};
}

void recordMovingRemoval(spdlog::logger &console,
                         const MovingRemovalDecision &decision,
                         MovingRemovalStats &stats) {
  console.debug("[Moving] Removed from entity {}", decision.entity_id);
  stats.removed++;
}

void recordMovingSkip(spdlog::logger &console,
                      const MovingRemovalDecision &decision,
                      MovingRemovalStats &stats) {
  console.debug("[Moving] Skipped entity {} (valid={}, hasMoving={})",
                decision.entity_id, decision.valid, decision.has_moving);
  stats.skipped++;
}

void processSingleMovingComponentRemoval(entt::registry &registry,
                                         spdlog::logger &console,
                                         entt::entity entity,
                                         MovingRemovalStats &stats) {
  const MovingRemovalDecision decision = inspectMovingRemoval(registry, entity);

  if (decision.has_moving) {
    registry.remove<MovingComponent>(entity);
    recordMovingRemoval(console, decision, stats);
    return;
  }

  recordMovingSkip(console, decision, stats);
}

EntityDeletionDecision inspectEntityDeletion(entt::registry &registry,
                                             entt::entity entity,
                                             bool softKill) {
  const int entityId = static_cast<int>(entity);
  return {
      entity,
      entityId,
      softKill,
      entityId == -1 || entityId == -2,
      registry.valid(entity),
  };
}

void recordDeletionIssue(EntityDeletionStats &stats, int entityId,
                         DeletionIssueReason reason) {
  stats.deletion_reasons[entityId] = static_cast<int>(reason);
}

void eraseScheduledDeletion(spdlog::logger &console, LifeEngine &lifeEngine,
                            entt::entity entity, bool invalidEntityPath) {
  if (invalidEntityPath) {
    console.debug(
        "[Deletion] Before erase (invalid) - entitiesScheduledForDeletion "
        "size: {}",
        lifeEngine.entitiesScheduledForDeletion.size());
  } else {
    console.debug("[Deletion] Before erase - entitiesScheduledForDeletion "
                  "size: {}",
                  lifeEngine.entitiesScheduledForDeletion.size());
  }

  const size_t erased = lifeEngine.entitiesScheduledForDeletion.erase(entity);
  if (invalidEntityPath) {
    console.debug("[Deletion] Erased {} entities from scheduled deletion "
                  "(invalid). New size: {}",
                  erased, lifeEngine.entitiesScheduledForDeletion.size());
  } else {
    console.debug(
        "[Deletion] Erased {} entities from scheduled deletion. New size: {}",
        erased, lifeEngine.entitiesScheduledForDeletion.size());
  }
}

void logEntityDeletionGridState(entt::registry &registry, VoxelGrid &voxelGrid,
                                spdlog::logger &console,
                                const EntityDeletionDecision &decision,
                                EntityDeletionStats &stats) {
  if (registry.all_of<Position, EntityTypeComponent>(decision.entity)) {
    auto [pos, type] =
        registry.get<Position, EntityTypeComponent>(decision.entity);
    console.debug("[Deletion] Entity {} position: ({},{},{}), type: {}/{}",
                  decision.entity_id, pos.x, pos.y, pos.z, type.mainType,
                  type.subType0);

    const int gridEntity = voxelGrid.getEntity(pos.x, pos.y, pos.z);
    console.debug("[Deletion] Grid entity at ({},{},{}): {}", pos.x, pos.y,
                  pos.z, gridEntity);

    if (gridEntity != decision.entity_id) {
      console.error("[Deletion] MISMATCH! Grid has {} but trying to delete {}. "
                    "This may cause duplicates on retry!",
                    gridEntity, decision.entity_id);
      stats.grid_mismatches++;
    }
    return;
  }

  console.debug("[Deletion] Entity {} lacks Position/EntityTypeComponent",
                decision.entity_id);
}

void destroyValidDeletionTarget(entt::registry &registry, VoxelGrid &voxelGrid,
                                entt::dispatcher &dispatcher,
                                LifeEngine &lifeEngine, spdlog::logger &console,
                                const EntityDeletionDecision &decision,
                                EntityDeletionStats &stats) {
  logEntityDeletionGridState(registry, voxelGrid, console, decision, stats);

  const bool shouldRemoveFromGrid = !decision.soft_kill;
  console.debug("[Deletion] Entity {} - shouldRemoveFromGrid: {}",
                decision.entity_id, shouldRemoveFromGrid);

  try {
    destroyEntityWithGridCleanup(registry, voxelGrid, dispatcher,
                                 decision.entity, shouldRemoveFromGrid);
    eraseScheduledDeletion(console, lifeEngine, decision.entity, false);
    console.debug("[Deletion] Successfully destroyed entity {}",
                  decision.entity_id);
    stats.successful_deletions++;
  } catch (const std::exception &e) {
    console.error("[Deletion] EXCEPTION while destroying entity {}: {}",
                  decision.entity_id, e.what());
    stats.skipped_deletions++;
    recordDeletionIssue(stats, decision.entity_id,
                        DeletionIssueReason::kDestroyException);
  }
}

void destroyInvalidDeletionTarget(entt::registry &registry,
                                  LifeEngine &lifeEngine,
                                  spdlog::logger &console,
                                  const EntityDeletionDecision &decision,
                                  EntityDeletionStats &stats) {
  console.warn("[Deletion] Entity {} already invalid - forcing cleanup",
               decision.entity_id);
  try {
    freeEntity(registry, decision.entity);
    eraseScheduledDeletion(console, lifeEngine, decision.entity, true);
    console.info("[Deletion] Force-destroyed invalid entity {}",
                 decision.entity_id);
    stats.successful_deletions++;
  } catch (const std::exception &e) {
    console.error("[Deletion] EXCEPTION while force-destroying entity {}: {}",
                  decision.entity_id, e.what());
    stats.skipped_deletions++;
    recordDeletionIssue(stats, decision.entity_id,
                        DeletionIssueReason::kForceDestroyException);
  }
}

void processSingleEntityDeletion(entt::registry &registry, VoxelGrid &voxelGrid,
                                 entt::dispatcher &dispatcher,
                                 LifeEngine &lifeEngine,
                                 spdlog::logger &console,
                                 const EntityDeletionDecision &decision,
                                 EntityDeletionStats &stats) {
  console.info(
      "[Deletion] Processing entity {}: specialId={}, valid={}, softKill={}",
      decision.entity_id, decision.is_special_id, decision.is_valid_entity,
      decision.soft_kill);

  if (!decision.is_special_id && decision.is_valid_entity) {
    destroyValidDeletionTarget(registry, voxelGrid, dispatcher, lifeEngine,
                               console, decision, stats);
    return;
  }

  if (decision.is_special_id) {
    console.warn("[Deletion] Skipping special ID: {}", decision.entity_id);
    recordDeletionIssue(stats, decision.entity_id,
                        DeletionIssueReason::kSpecialId);
    return;
  }

  if (!decision.is_valid_entity) {
    destroyInvalidDeletionTarget(registry, lifeEngine, console, decision,
                                 stats);
  }
}

const char *deletionIssueReasonToString(int reason) {
  if (reason == static_cast<int>(DeletionIssueReason::kDestroyException))
    return "Exception";
  if (reason == static_cast<int>(DeletionIssueReason::kSpecialId))
    return "Special ID";
  if (reason == static_cast<int>(DeletionIssueReason::kForceDestroyException))
    return "Force destroy exception";
  return "";
}

void logEntityDeletionSummary(spdlog::logger &console,
                              const EntityDeletionStats &stats) {
  console.debug(
      "[Deletion Summary] Successful: {}, Skipped: {}, Grid Mismatches: {}",
      stats.successful_deletions, stats.skipped_deletions,
      stats.grid_mismatches);

  if (!stats.deletion_reasons.empty()) {
    console.warn("[Deletion Reasons] {} entities had issues:",
                 stats.deletion_reasons.size());
    for (const auto &[id, reason] : stats.deletion_reasons) {
      console.warn("  - Entity {}: {}", id,
                   deletionIssueReasonToString(reason));
    }
  }
}

} // namespace

static void processMovingComponentRemovals(entt::registry &registry,
                                           LifeEngine &lifeEngine) {
  auto console = spdlog::get("console");
  if (!console)
    console = spdlog::stdout_color_mt("console");

  console->debug("--- PHASE 2: Removing MovingComponent ---");
  MovingRemovalStats stats;
  for (const auto &[entity, _] : lifeEngine.entitiesToRemoveMovingComponent) {
    processSingleMovingComponentRemoval(registry, *console, entity, stats);
  }
  console->debug("[Moving Summary] Removed: {}, Skipped: {}", stats.removed,
                 stats.skipped);
  lifeEngine.entitiesToRemoveMovingComponent.clear();
}

static void processEntityDeletionQueue(entt::registry &registry,
                                       VoxelGrid &voxelGrid,
                                       entt::dispatcher &dispatcher,
                                       LifeEngine &lifeEngine) {
  auto console = spdlog::get("console");
  if (!console)
    console = spdlog::stdout_color_mt("console");

  console->debug("--- PHASE 3: Full entity deletion ---");
  EntityDeletionStats stats;

  for (const auto &[entity, softKill] : lifeEngine.entitiesToDelete) {
    const EntityDeletionDecision decision =
        inspectEntityDeletion(registry, entity, softKill);
    processSingleEntityDeletion(registry, voxelGrid, dispatcher, lifeEngine,
                                *console, decision, stats);
  }

  logEntityDeletionSummary(*console, stats);

  // Clear the deletion queue - CRITICAL: Only clear if we've processed
  // everything
  console->debug("[Deletion] Clearing entitiesToDelete queue");
  console->debug(
      "[Deletion] Before final clear - entitiesScheduledForDeletion size: {}",
      lifeEngine.entitiesScheduledForDeletion.size());
  lifeEngine.entitiesToDelete.clear();
  lifeEngine.entitiesScheduledForDeletion.clear();
  console->debug(
      "[Deletion] After final clear - entitiesScheduledForDeletion size: {}",
      lifeEngine.entitiesScheduledForDeletion.size());
}

void World::processEntityDeletion() {
  // Acquire EXCLUSIVE lock to prevent any perception operations during entity
  // destruction
  std::unique_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

  // Locking contract: acquire `entityLifecycleMutex` (exclusive) first,
  // then acquire a `TerrainGridLock` if modifying terrain. This prevents
  // deadlocks with perception readers and other terrain operations.

  auto console = spdlog::get("console");
  if (!console)
    console = spdlog::stdout_color_mt("console");

  console->debug("\n========== ENTITY DELETION PHASE START ==========");
  console->debug("Total entities in entitiesToDelete: {}",
                 lifeEngine->entitiesToDelete.size());
  console->debug("Total entities in entitiesToRemoveVelocity: {}",
                 lifeEngine->entitiesToRemoveVelocity.size());
  console->debug("Total entities in entitiesToRemoveMovingComponent: {}",
                 lifeEngine->entitiesToRemoveMovingComponent.size());

  processVelocityRemovals(registry, *lifeEngine);
  processMovingComponentRemovals(registry, *lifeEngine);
  processEntityDeletionQueue(registry, *voxelGrid, dispatcher, *lifeEngine);

  console->debug("========== ENTITY DELETION PHASE COMPLETE ==========\n");
}
