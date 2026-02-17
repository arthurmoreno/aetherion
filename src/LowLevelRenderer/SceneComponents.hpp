#ifndef SCENE_COMPONENTS_HPP
#define SCENE_COMPONENTS_HPP

#include <nanobind/nanobind.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/string.h>

#include <entt/entt.hpp>

// -----------------------------------------------------------------------------
// Hierarchy component: attached to every scene-graph node (entity)
// -----------------------------------------------------------------------------
struct Hierarchy {
    entt::entity parent{entt::null};
    entt::entity first_child{entt::null};
    entt::entity next_sibling{entt::null};
    entt::entity prev_sibling{entt::null};
};

struct NodePython {
    nb::object instance;  // The actual Python object instance
};

#endif  // SCENE_COMPONENTS_HPP