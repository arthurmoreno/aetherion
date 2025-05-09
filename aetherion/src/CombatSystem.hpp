#ifndef COMBAT_SYSTEM_HPP
#define COMBAT_SYSTEM_HPP

#include <entt/entt.hpp>

#include "VoxelGrid.hpp"
#include "components/CombatComponents.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/PhysicsComponents.hpp"

class CombatSystem {
   public:
    CombatSystem() = default;
    CombatSystem(entt::registry& reg, VoxelGrid* voxelGrid) : registry(reg) {}

    // Method to process physics-related events
    void processCombat(entt::registry& registry, VoxelGrid& voxelGrid);
    void processCombatAsync(entt::registry& registry, VoxelGrid& voxelGrid,
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

#endif  // COMBAT_SYSTEM_HPP