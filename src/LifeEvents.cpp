#include "LifeEvents.hpp"

#include <iostream>

#include "physics/PhysicsMutators.hpp"  // For softKillEntity and dropEntityItems

void LifeEngine::onKillEntity(const KillEntityEvent& event) {
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
    if (entitiesScheduledForDeletion.find(event.entity) != entitiesScheduledForDeletion.end()) {
        // std::cout << "Entity " << entityId
        //           << " already scheduled for deletion, ignoring duplicate KillEntityEvent"
        //           << std::endl;
        return;  // Skip duplicate deletion requests
    }

    // Add to set to prevent future duplicates
    if (event.softKill) {
        softKillEntity(registry, *voxelGrid, dispatcher, event.entity);
    } else {
        // std::cout << "Deleting entity hard kill: " << entityId << std::endl;
    }

    dropEntityItems(registry, *voxelGrid, event.entity);

    if (entityId != -1 && entityId != -2) {
        entitiesToDelete.emplace_back(event.entity, event.softKill);
        entitiesScheduledForDeletion.insert(event.entity);
        // std::cout << "Added entity " << entityId
        //         << " to deletion queue (softKill: " << event.softKill << ")" << std::endl;
    }
}

void LifeEngine::onTerrainRemoveVelocityEvent(const TerrainRemoveVelocityEvent& event) {
    incLifeMetric(LIFE_REMOVE_VELOCITY);
    int entityId = static_cast<int>(event.entity);
    // std::cout << "Removing Velocity component from entity: " << entityId << std::endl;

    if (entityId != -1 && entityId != -2) {
        entitiesToRemoveVelocity.emplace_back(event.entity, false);
    }
}

void LifeEngine::onTerrainRemoveMovingComponentEvent(
    const TerrainRemoveMovingComponentEvent& event) {
    incLifeMetric(LIFE_REMOVE_MOVING_COMPONENT);
    int entityId = static_cast<int>(event.entity);
    // std::cout << "Removing MovingComponent from entity: " << entityId << std::endl;

    if (entityId != -1 && entityId != -2) {
        entitiesToRemoveMovingComponent.emplace_back(event.entity, false);
    }
}

void LifeEngine::incLifeMetric(const std::string& metricName) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    lifeMetrics_[metricName]++;
}

// Flush current metrics to GameDB via provided handler and reset counters
void LifeEngine::flushLifeMetrics(GameDBHandler* dbHandler) {
    if (dbHandler == nullptr) return;

    std::lock_guard<std::mutex> lock(metricsMutex_);
    auto now = std::chrono::system_clock::now();
    long long ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    for (const auto& kv : lifeMetrics_) {
        const std::string& name = kv.first;
        uint64_t value = kv.second;
        dbHandler->putTimeSeries(name, ts, static_cast<double>(value));
    }

    // reset counters
    for (auto& kv : lifeMetrics_) {
        kv.second = 0;
    }
}

// Register event handlers
void LifeEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<KillEntityEvent>().connect<&LifeEngine::onKillEntity>(*this);
    dispatcher.sink<TerrainRemoveVelocityEvent>()
        .connect<&LifeEngine::onTerrainRemoveVelocityEvent>(*this);
    dispatcher.sink<TerrainRemoveMovingComponentEvent>()
        .connect<&LifeEngine::onTerrainRemoveMovingComponentEvent>(*this);
}