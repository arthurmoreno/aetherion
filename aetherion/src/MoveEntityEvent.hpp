#ifndef MOVE_ENTITY_EVENT_HPP
#define MOVE_ENTITY_EVENT_HPP

#include <entt/entt.hpp>


struct MoveSolidEntityEvent {
    entt::entity entity;
    float forceX, forceY, forceZ;

    MoveSolidEntityEvent(entt::entity entity, float forceX, float forceY, float forceZ)
        : entity(entity), forceX(forceX), forceY(forceY), forceZ(forceZ) {}
};

#endif  // MOVE_ENTITY_EVENT_HPP