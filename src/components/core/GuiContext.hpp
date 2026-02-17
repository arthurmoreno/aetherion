#pragma once

#include <nanobind/nanobind.h>

#include <memory>

#include "EntityInterface.hpp"
#include "World.hpp"

namespace nb = nanobind;

/**
 * @brief Shared context passed to all GUI programs
 *
 * Contains all data needed for rendering GUI programs. This structure acts as
 * a dependency injection container, providing programs with access to world state,
 * physics settings, inventory data, and entity interfaces without tight coupling.
 */
struct GuiContext {
    // World state metrics
    int worldTicks;
    float availableFps;
    std::shared_ptr<World> worldPtr;

    // Python data bindings (bidirectional communication with Python layer)
    nb::dict physicsChanges;  // Physics settings that can be modified via GUI
    nb::dict inventoryData;   // Current player inventory state
    nb::list& consoleLogs;    // Console log messages to display
    nb::list& entitiesData;   // Entity data for stats windows
    nb::list& commands;       // Output: GUI-generated commands (e.g., item transfers)
    nb::dict statistics;      // AI statistics for visualization
    nb::dict& sharedData;     // Shared state dictionary for inter-module communication

    // Entity interfaces for inspection
    std::shared_ptr<EntityInterface> entityInterfacePtr;          // Generic entity interface
    std::shared_ptr<EntityInterface> hoveredEntityInterfacePtr;   // Currently hovered entity
    std::shared_ptr<EntityInterface> selectedEntityInterfacePtr;  // Currently selected entity
};
