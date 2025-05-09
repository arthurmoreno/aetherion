#ifndef EFFECTS_SYSTEM_HPP
#define EFFECTS_SYSTEM_HPP

#include <entt/entt.hpp>

#include "LifeEvents.hpp"
#include "VoxelGrid.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"

class EffectsSystem {
   public:
    EffectsSystem() = default;
    EffectsSystem(entt::registry& reg, VoxelGrid* voxelGrid) : registry(reg) {}

    // Method to process physics-related events
    void processEffects(entt::registry& registry, VoxelGrid& voxelGrid,
                        entt::dispatcher& dispatcher);
    void processEffectsAsync(entt::registry& registry, VoxelGrid& voxelGrid,
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

#endif  // EFFECTS_SYSTEM_HPP