#ifndef LIFE_CYCLE_COMPONENTS_HPP
#define LIFE_CYCLE_COMPONENTS_HPP

#include <entt/entt.hpp>

struct KillEntityEvent {
    entt::entity entity;
    bool softKill{};

    KillEntityEvent(entt::entity entity) : entity(entity) { softKill = false; }

    KillEntityEvent(entt::entity entity, bool softKill) : entity(entity), softKill(softKill) {}
};

#endif  // LIFE_CYCLE_COMPONENTS_HPP