
#include "CombatSystem.hpp"

void CombatSystem::processCombat(entt::registry& registry, VoxelGrid& voxelGrid) {
    auto meeleAttackView = registry.view<MeeleAttackComponent>();
    for (auto entity : meeleAttackView) {
        // handleMovement(registry, voxelGrid, entity);
    }
}
