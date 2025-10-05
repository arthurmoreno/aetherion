#ifndef LIFE_EVENTS_HPP
#define LIFE_EVENTS_HPP

#include <nanobind/nanobind.h>

#include <entt/entt.hpp>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "EntityInterface.hpp"
#include "ItemConfigurationManager.hpp"
#include "VoxelGrid.hpp"
#include "components/ItemsComponents.hpp"
#include "components/LifecycleComponents.hpp"

namespace nb = nanobind;

class LifeEngine {
   public:
    std::vector<std::tuple<entt::entity, bool>> entitiesToDelete;
    std::unordered_set<entt::entity> entitiesScheduledForDeletion;

    LifeEngine() = default;
    LifeEngine(entt::registry& reg, entt::dispatcher& disp, VoxelGrid* voxelGrid)
        : registry(reg), dispatcher(disp), voxelGrid(voxelGrid) {}

    // Handle entity movement event
    void onKillEntity(const KillEntityEvent& event);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);

    void removeEntityFromGrid(entt::entity entity);

   private:
    entt::registry& registry;
    entt::dispatcher& dispatcher;
    VoxelGrid* voxelGrid;

    void dropItems(entt::entity entity);
};

#endif  // LIFE_EVENTS_HPP