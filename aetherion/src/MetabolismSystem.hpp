#ifndef METABOLISM_SYSTEM_HPP
#define METABOLISM_SYSTEM_HPP

#include <entt/entt.hpp>

#include "LifeEvents.hpp"
#include "VoxelGrid.hpp"
#include "components/DnaComponents.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/MetabolismComponents.hpp"
#include "components/PerceptionComponent.hpp"
#include "components/PhysicsComponents.hpp"

class MetabolismSystem {
   public:
    MetabolismSystem() = default;
    MetabolismSystem(entt::registry& reg, VoxelGrid* voxelGrid) : registry(reg) {}

    // Method to process physics-related events
    void processMetabolism(entt::registry& registry, VoxelGrid& voxelGrid,
                           entt::dispatcher& dispatcher);
    void processMetabolismAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                                entt::dispatcher& dispatcher);

    // Register the event handler
    void registerEventHandlers(entt::dispatcher& dispatcher);

    bool isProcessingComplete() const;

   private:
    int chunkDigestionTime = 10;
    float chunkMass = 1;
    entt::registry& registry;
    VoxelGrid* voxelGrid;

    // Mutex for thread safety
    std::mutex metabolismMutex;
    bool processingComplete = true;  // Flag to indicate processing state

    int lastEntitiesCount{};
    const int MAX_ENTITIES{300};
};

#endif  // METABOLISM_SYSTEM_HPP