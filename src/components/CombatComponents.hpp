#ifndef COMBAT_COMPONENTS_HPP
#define COMBAT_COMPONENTS_HPP

#include <entt/entt.hpp>

struct MeeleAttackComponent {
    int weapon;
    int hoveredEntity;
    int selectedEntity;
};

#endif  // COMBAT_COMPONENTS_HPP