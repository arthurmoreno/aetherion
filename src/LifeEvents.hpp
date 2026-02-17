#ifndef LIFE_EVENTS_HPP
#define LIFE_EVENTS_HPP

#include <nanobind/nanobind.h>

#include <chrono>
#include <entt/entt.hpp>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "EntityInterface.hpp"
#include "GameDBHandler.hpp"
#include "ItemConfigurationManager.hpp"
#include "components/ItemsComponents.hpp"
#include "components/LifecycleComponents.hpp"
#include "voxelgrid/VoxelGrid.hpp"

namespace nb = nanobind;

// Life event time series metric names
inline const std::string LIFE_KILL_ENTITY = "life_kill_entity";
inline const std::string LIFE_SOFT_KILL_ENTITY = "life_soft_kill_entity";
inline const std::string LIFE_HARD_KILL_ENTITY = "life_hard_kill_entity";
inline const std::string LIFE_REMOVE_VELOCITY = "life_remove_velocity";
inline const std::string LIFE_REMOVE_MOVING_COMPONENT = "life_remove_moving_component";

class LifeEngine {
   public:
    std::vector<std::tuple<entt::entity, bool>> entitiesToDelete;
    std::vector<std::tuple<entt::entity, bool>> entitiesToRemoveVelocity;
    std::vector<std::tuple<entt::entity, bool>> entitiesToRemoveMovingComponent;
    std::unordered_set<entt::entity> entitiesScheduledForDeletion;

    LifeEngine() = default;
    LifeEngine(entt::registry& reg, entt::dispatcher& disp, VoxelGrid* voxelGrid)
        : registry(reg), dispatcher(disp), voxelGrid(voxelGrid) {}

    // Handle entity movement event
    void onKillEntity(const KillEntityEvent& event);
    void onTerrainRemoveVelocityEvent(const TerrainRemoveVelocityEvent& event);
    void onTerrainRemoveMovingComponentEvent(const TerrainRemoveMovingComponentEvent& event);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);

    // Metrics flush to game DB
    void flushLifeMetrics(GameDBHandler* dbHandler);

    // Increment a named metric
    void incLifeMetric(const std::string& metricName);

   private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
    VoxelGrid* voxelGrid;

    // Monitoring counters for life events
    std::unordered_map<std::string, uint64_t> lifeMetrics_;
    std::mutex metricsMutex_;
};

#endif  // LIFE_EVENTS_HPP