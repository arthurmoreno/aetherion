#ifndef DNA_COMPONENTS_HPP
#define DNA_COMPONENTS_HPP

#include <entt/entt.hpp>
#include <vector>

struct ParentsComponent {
    std::vector<int> parents;
};

#endif