#include "LifeEvents.hpp"

#include <iostream>

void LifeEngine::dropItems(entt::entity entity) {
    if (registry.valid(entity) && registry.all_of<Position, DropRates>(entity)) {
        auto&& [pos, dropRates] = registry.get<Position, DropRates>(entity);

        auto terrainBellowId = voxelGrid->getTerrain(pos.x, pos.y, pos.z - 1);

        // TODO: TerrainRepository is not supporting Inventory yet. (Only Pure ECS entities)
        // if (terrainBellowId != -1 && terrainBellowId != -2) {
        //     entt::entity terrainBellow = static_cast<entt::entity>(terrainBellowId);

        //     Inventory* inventory = registry.try_get<Inventory>(terrainBellow);
        //     bool shouldEmplaceInventory{inventory == nullptr};
        //     if (inventory == nullptr) {
        //         inventory = new Inventory();
        //     }

        //     if (!dropRates.itemDropRates.empty()) {
        //         for (const auto& [combinedItemId, valuesTuple] : dropRates.itemDropRates) {
        //             auto [itemMainType, itemSubType0] = splitStringToInts(combinedItemId);

        //             if (itemMainType == static_cast<int>(ItemEnum::FOOD)) {
        //                 std::shared_ptr<ItemConfiguration> itemConfiguration =
        //                     getItemConfigurationOnManager(combinedItemId);
        //                 auto newFoodItem = itemConfiguration->createFoodItem(registry);

        //                 auto entityId = entt::to_integral(newFoodItem);
        //                 inventory->itemIDs.push_back(entityId);
        //             }
        //         }

        //         if (shouldEmplaceInventory) {
        //             registry.emplace<Inventory>(terrainBellow, *inventory);
        //         }
        //     }
        // }
    }
}

void LifeEngine::removeEntityFromGrid(entt::entity entity) {
    int entityId = static_cast<int>(entity);
    bool isSpecialId = entityId == -1 || entityId == -2;
    if (!isSpecialId && registry.valid(entity) && registry.all_of<Position, EntityTypeComponent>(entity)) {
        std::cout << "Removing entity from grid: " << entityId << std::endl;
        auto&& [pos, type] = registry.get<Position, EntityTypeComponent>(entity);

        // CHECK: Make sure the grid actually contains THIS entity
        int currentGridEntity = voxelGrid->getEntity(pos.x, pos.y, pos.z);
        if (currentGridEntity != entityId) {
            std::cout << "WARNING: Grid position (" << pos.x << "," << pos.y << "," << pos.z
                      << ") contains entity " << currentGridEntity
                      << " but trying to remove entity " << entityId << std::endl;
            return;  // Don't clear grid if it's not our entity
        }

        if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
            voxelGrid->deleteTerrain(pos.x, pos.y, pos.z);
        } else if (type.mainType == static_cast<int>(EntityEnum::BEAST) ||
                   type.mainType == static_cast<int>(EntityEnum::PLANT)) {
            voxelGrid->deleteEntity(pos.x, pos.y, pos.z);  // Use thread-safe deleteEntity
        }
    } else if (isSpecialId) {
        // voxelGrid->deleteTerrain(pos.x, pos.y, pos.z);
        std::cout << "Entity " << entityId << " is a special ID, skipping grid removal."
                  << std::endl;

    } else if (!isSpecialId && registry.valid(entity)) {
        std::cout << "Entity " << entityId << " is valid, proceeding with grid removal [FAKE]."
                  << std::endl;

    } else {
        std::cout << "Entity " << entityId << " is invalid, skipping grid removal." << std::endl;
    }
}

void LifeEngine::onKillEntity(const KillEntityEvent& event) {
    int entityId = static_cast<int>(event.entity);
    
    // Check if entity is already scheduled for deletion
    if (entitiesScheduledForDeletion.find(event.entity) != entitiesScheduledForDeletion.end()) {
        std::cout << "Entity " << entityId << " already scheduled for deletion, ignoring duplicate KillEntityEvent" << std::endl;
        return; // Skip duplicate deletion requests
    }
    
    // Add to set to prevent future duplicates
    entitiesScheduledForDeletion.insert(event.entity);
    
    if (event.softKill) {
        std::cout << "Deleting entity soft kill: " << entityId << std::endl;

        // Safely remove MetabolismComponent if it exists
        if (registry.all_of<MetabolismComponent>(event.entity)) {
            registry.remove<MetabolismComponent>(event.entity);
            std::cout << "Removed MetabolismComponent from entity " << entityId << std::endl;
        }
        
        // Safely remove HealthComponent if it exists
        if (registry.all_of<HealthComponent>(event.entity)) {
            registry.remove<HealthComponent>(event.entity);
            std::cout << "Removed HealthComponent from entity " << entityId << std::endl;
        }
        removeEntityFromGrid(event.entity);
    } else {
        std::cout << "Deleting entity hard kill: " << entityId << std::endl;
    }
    
    dropItems(event.entity);
    
    if (entityId != -1 && entityId != -2) {
        entitiesToDelete.emplace_back(event.entity, event.softKill);
        std::cout << "Added entity " << entityId << " to deletion queue (softKill: " << event.softKill << ")" << std::endl;
    }
}

// Register event handlers
void LifeEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<KillEntityEvent>().connect<&LifeEngine::onKillEntity>(*this);
}