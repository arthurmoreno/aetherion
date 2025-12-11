#ifndef PHYSICS_VALIDATORS_H
#define PHYSICS_VALIDATORS_H

#include <entt/entt.hpp>

// Helper: Validate terrain entity ID
inline void validateTerrainEntityId(entt::entity entity) {
    int entityId = static_cast<int>(entity);
    if (entityId == -1 || entityId == -2) {
        throw std::runtime_error("Invalid terrain entity ID in createMovingComponent");
    }
}

#endif  // PHYSICS_VALIDATORS_H