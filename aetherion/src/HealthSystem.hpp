#ifndef HEALTH_SYSTEM_HPP
#define HEALTH_SYSTEM_HPP

#include <entt/entt.hpp>

#include "LifeEvents.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/PhysicsComponents.hpp"
#include "voxelgrid/VoxelGrid.hpp"

class HealthSystem {
   public:
    HealthSystem() = default;
    HealthSystem(entt::registry& reg, VoxelGrid* voxelGrid) : registry(reg) {}

    // Method to process physics-related events
    void processHealth(entt::registry& registry, VoxelGrid& voxelGrid,
                       entt::dispatcher& dispatcher);
    void processHealthAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                            entt::dispatcher& dispatcher);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);

    bool isProcessingComplete() const;

   private:
    entt::registry& registry;
    VoxelGrid* voxelGrid;

    // Mutex for thread safety
    std::mutex combatMutex;
    bool processingComplete = true;  // Flag to indicate processing state
};

#endif  // HEALTH_SYSTEM_HPP