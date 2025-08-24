#include "LifeEvents.hpp"

#include <iostream>

void LifeEngine::dropItems(entt::entity entity) {
    if (registry.all_of<Position, DropRates>(entity)) {
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
    if (!isSpecialId && registry.all_of<Position, EntityTypeComponent>(entity)) {
        std::cout << "Removing entity from grid: " << entt::to_integral(entity) << std::endl;
        auto&& [pos, type] = registry.get<Position, EntityTypeComponent>(entity);

        if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
            voxelGrid->deleteTerrain(pos.x, pos.y, pos.z);
        } else if (type.mainType == static_cast<int>(EntityEnum::BEAST) ||
                   type.mainType == static_cast<int>(EntityEnum::PLANT)) {
            voxelGrid->setEntity(pos.x, pos.y, pos.z, -1);
        }
    } else if (isSpecialId) {
        // voxelGrid->deleteTerrain(pos.x, pos.y, pos.z);
        std::cout << "Entity " << entityId << " is a special ID, skipping grid removal." << std::endl;

    } else if (!isSpecialId && registry.valid(entity)) {
        std::cout << "Entity " << entityId << " is valid, proceeding with grid removal [FAKE]." << std::endl;

    } else {
        std::cout << "Entity " << entityId << " is invalid, skipping grid removal." << std::endl;
    }
}

void LifeEngine::onKillEntity(const KillEntityEvent& event) {
    if (event.softKill) {
        std::cout << "Deleting entity soft kill" << std::endl;
        removeEntityFromGrid(event.entity);
    }
    dropItems(event.entity);
    int entityId = static_cast<int>(event.entity);
    if (entityId != -1 && entityId != -2) {
        entitiesToDelete.emplace_back(event.entity, event.softKill);
    }
}

// Register event handlers
void LifeEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<KillEntityEvent>().connect<&LifeEngine::onKillEntity>(*this);
}