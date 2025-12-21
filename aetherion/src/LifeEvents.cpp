#include "LifeEvents.hpp"
#include "physics/PhysicsMutators.hpp" // For softKillEntity and dropEntityItems

#include <iostream>

void LifeEngine::onKillEntity(const KillEntityEvent& event) {
    int entityId = static_cast<int>(event.entity);

    // Check if entity is already scheduled for deletion
    if (entitiesScheduledForDeletion.find(event.entity) != entitiesScheduledForDeletion.end()) {
        // std::cout << "Entity " << entityId
        //           << " already scheduled for deletion, ignoring duplicate KillEntityEvent"
        //           << std::endl;
        return;  // Skip duplicate deletion requests
    }

    // Add to set to prevent future duplicates
    entitiesScheduledForDeletion.insert(event.entity);

    if (event.softKill) {
        softKillEntity(registry, *voxelGrid, dispatcher, event.entity);
    } else {
        // std::cout << "Deleting entity hard kill: " << entityId << std::endl;
    }

    dropEntityItems(registry, *voxelGrid, event.entity);

    if (entityId != -1 && entityId != -2) {
        entitiesToDelete.emplace_back(event.entity, event.softKill);
            // std::cout << "Added entity " << entityId
            //         << " to deletion queue (softKill: " << event.softKill << ")" << std::endl;
    }
}

// Register event handlers
void LifeEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<KillEntityEvent>().connect<&LifeEngine::onKillEntity>(*this);
}