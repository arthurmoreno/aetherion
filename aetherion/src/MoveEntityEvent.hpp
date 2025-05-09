#ifndef MOVE_ENTITY_EVENT_HPP
#define MOVE_ENTITY_EVENT_HPP

#include <entt/entt.hpp>

struct MoveGasEntityEvent {
    entt::entity entity;
    bool forceApplyNewVelocity;
    float forceX, forceY, rhoEnv, rhoGas;

    MoveGasEntityEvent(entt::entity entity, float forceX, float forceY, float rhoEnv, float rhoGas)
        : entity(entity), forceX(forceX), forceY(forceY), rhoEnv(rhoEnv), rhoGas(rhoGas) {
        forceApplyNewVelocity = false;
    }

    void setForceApplyNewVelocity() { forceApplyNewVelocity = true; }
};

struct MoveSolidEntityEvent {
    entt::entity entity;
    float forceX, forceY, forceZ;

    MoveSolidEntityEvent(entt::entity entity, float forceX, float forceY, float forceZ)
        : entity(entity), forceX(forceX), forceY(forceY), forceZ(forceZ) {}
};

#endif  // MOVE_ENTITY_EVENT_HPP