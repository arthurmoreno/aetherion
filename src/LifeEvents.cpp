#include "LifeEvents.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>

#include "diag/ThrottledLog.hpp"
#include "physics/PhysicsMutators.hpp" // For softKillEntity and dropEntityItems

void LifeEngine::onKillEntity(const KillEntityEvent &event) {
  incLifeMetric(LIFE_KILL_ENTITY);
  if (event.softKill) {
    incLifeMetric(LIFE_SOFT_KILL_ENTITY);
  } else {
    incLifeMetric(LIFE_HARD_KILL_ENTITY);
  }

  int entityId = static_cast<int>(event.entity);

  if (!registry.valid(event.entity)) {
    return;
  }

  // Check if entity is already scheduled for deletion
  if (entitiesScheduledForDeletion.find(event.entity) !=
      entitiesScheduledForDeletion.end()) {
    // Cumulative count of KillEntityEvents that fired for an entity
    // already queued for reap. A non-trivial rate here points at a system
    // (HealthSystem, MetabolismSystem, EffectsSystem, terrain repository)
    // re-firing without checking the entity's pending-destroy state.
    static size_t _dupKills = 0;
    static aetherion::diag::ThrottledLog _dupLog{std::chrono::seconds(1)};
    _dupKills++;
    _dupLog.fire([&](spdlog::logger &log) {
      log.info("[kill-dedup] cumulative_duplicate_kills={} latest_entity={}",
               _dupKills, entityId);
    });
    return; // Skip duplicate deletion requests
  }

  // Add to set to prevent future duplicates
  if (event.softKill) {
    softKillEntity(registry, *voxelGrid, sink, event.entity);
  } else {
    // std::cout << "Deleting entity hard kill: " << entityId << std::endl;
  }

  dropEntityItems(registry, sink, *voxelGrid, event.entity);

  if (entityId != -1 && entityId != -2) {
    entitiesToDelete.emplace_back(event.entity, event.softKill);
    entitiesScheduledForDeletion.insert(event.entity);
    // std::cout << "Added entity " << entityId
    //         << " to deletion queue (softKill: " << event.softKill << ")" <<
    //         std::endl;
  }
}

void LifeEngine::onTerrainRemoveVelocityEvent(
    const TerrainRemoveVelocityEvent &event) {
  incLifeMetric(LIFE_REMOVE_VELOCITY);
  int entityId = static_cast<int>(event.entity);
  // std::cout << "Removing Velocity component from entity: " << entityId <<
  // std::endl;

  if (entityId != -1 && entityId != -2) {
    entitiesToRemoveVelocity.emplace_back(event.entity, false);
  }
}

void LifeEngine::onTerrainRemoveMovingComponentEvent(
    const TerrainRemoveMovingComponentEvent &event) {
  incLifeMetric(LIFE_REMOVE_MOVING_COMPONENT);
  int entityId = static_cast<int>(event.entity);
  // std::cout << "Removing MovingComponent from entity: " << entityId <<
  // std::endl;

  if (entityId != -1 && entityId != -2) {
    entitiesToRemoveMovingComponent.emplace_back(event.entity, false);
  }
}

void LifeEngine::incLifeMetric(const std::string &metricName) {
  std::lock_guard<std::mutex> lock(metricsMutex_);
  lifeMetrics_[metricName]++;
}

// Flush current metrics to GameDB via provided handler and reset counters
void LifeEngine::flushLifeMetrics(GameDBHandler *dbHandler) {
  if (dbHandler == nullptr)
    return;

  std::lock_guard<std::mutex> lock(metricsMutex_);
  auto now = std::chrono::system_clock::now();
  long long ts =
      std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
          .count();

  for (const auto &kv : lifeMetrics_) {
    const std::string &name = kv.first;
    uint64_t value = kv.second;
    dbHandler->putTimeSeries(name, ts, static_cast<double>(value));
  }

  // reset counters
  for (auto &kv : lifeMetrics_) {
    kv.second = 0;
  }
}

// Register event handlers
void LifeEngine::registerEventHandlers(entt::dispatcher &disp) {
  disp.sink<KillEntityEvent>().connect<&LifeEngine::onKillEntity>(*this);
  disp.sink<TerrainRemoveVelocityEvent>()
      .connect<&LifeEngine::onTerrainRemoveVelocityEvent>(*this);
  disp.sink<TerrainRemoveMovingComponentEvent>()
      .connect<&LifeEngine::onTerrainRemoveMovingComponentEvent>(*this);
}