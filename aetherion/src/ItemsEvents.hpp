#ifndef ITEMS_EVENTS_HPP
#define ITEMS_EVENTS_HPP

#include <nanobind/nanobind.h>

#include <entt/entt.hpp>

#include "VoxelGrid.hpp"

namespace nb = nanobind;

struct TakeItemEvent {
    entt::entity entity;
    nb::object pyRegistryObj;
    VoxelGrid* voxelGrid;

    int hoveredEntityId;
    int selectedEntityId;

    TakeItemEvent(entt::entity entity, nb::object pyRegistryObj, VoxelGrid* voxelGrid,
                  int hoveredEntityId, int selectedEntityId)
        : entity(entity),
          pyRegistryObj(pyRegistryObj),
          voxelGrid(voxelGrid),
          hoveredEntityId(hoveredEntityId),
          selectedEntityId(selectedEntityId) {}
};

struct OnTakeItemBehavior {
    nb::object behavior;
};

struct UseItemEvent {
    entt::entity entity;
    nb::object pyRegistryObj;
    VoxelGrid* voxelGrid;

    int itemSlot;
    int hoveredEntityId;
    int selectedEntityId;

    UseItemEvent(entt::entity entity, nb::object pyRegistryObj, VoxelGrid* voxelGrid, int itemSlot,
                 int hoveredEntityId, int selectedEntityId)
        : entity(entity),
          pyRegistryObj(pyRegistryObj),
          voxelGrid(voxelGrid),
          itemSlot(itemSlot),
          hoveredEntityId(hoveredEntityId),
          selectedEntityId(selectedEntityId) {}
};

struct OnUseItemBehavior {
    nb::object behavior;
};

#endif  // ITEMS_EVENTS_HPP