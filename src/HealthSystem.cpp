
#include "HealthSystem.hpp"

void HealthSystem::processHealth(entt::registry& registry, VoxelGrid& voxelGrid,
                                 entt::dispatcher& dispatcher) {
    auto healthCompView = registry.view<HealthComponent>();
    for (auto entity : healthCompView) {
        auto& health = registry.get<HealthComponent>(entity);

        if (health.healthLevel <= 0) {
            // Kill the entity
            dispatcher.enqueue<KillEntityEvent>(entity, true);
        }
        // handleMovement(registry, voxelGrid, entity);
    }
}
