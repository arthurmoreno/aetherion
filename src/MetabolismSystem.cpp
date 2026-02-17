#include "MetabolismSystem.hpp"

#include <chrono>
#include <thread>

// Function to clone an entity and all of its components
entt::entity clone_entity(entt::registry& registry, entt::entity source_entity) {
    // Create a new entity in the target registry
    entt::entity new_entity = registry.create();

    // Clone the components from the source entity to the new entity
    if (registry.any_of<EntityTypeComponent>(source_entity)) {
        const auto& etc = registry.get<EntityTypeComponent>(source_entity);
        registry.emplace<EntityTypeComponent>(new_entity, etc);
    }

    if (registry.any_of<PhysicsStats>(source_entity)) {
        const auto& ps = registry.get<PhysicsStats>(source_entity);
        registry.emplace<PhysicsStats>(new_entity, ps);
    }

    if (registry.any_of<Position>(source_entity)) {
        const auto& position = registry.get<Position>(source_entity);
        registry.emplace<Position>(new_entity, position);
    }

    if (registry.any_of<Velocity>(source_entity)) {
        const auto& velocity = registry.get<Velocity>(source_entity);
        registry.emplace<Velocity>(new_entity, velocity);
    }

    if (registry.any_of<HealthComponent>(source_entity)) {
        const auto& health = registry.get<HealthComponent>(source_entity);
        registry.emplace<HealthComponent>(new_entity, health);
    }

    if (registry.any_of<PerceptionComponent>(source_entity)) {
        const auto& perception = registry.get<PerceptionComponent>(source_entity);
        registry.emplace<PerceptionComponent>(new_entity, perception);
    }

    if (registry.any_of<DigestionComponent>(source_entity)) {
        const auto& digestion = registry.get<DigestionComponent>(source_entity);
        registry.emplace<DigestionComponent>(new_entity, digestion);
    }

    if (registry.any_of<MetabolismComponent>(source_entity)) {
        const auto& metabolism = registry.get<MetabolismComponent>(source_entity);
        registry.emplace<MetabolismComponent>(new_entity, metabolism);
    }

    if (registry.any_of<Inventory>(source_entity)) {
        const auto& inventory = registry.get<Inventory>(source_entity);
        registry.emplace<Inventory>(new_entity, inventory);
    }

    return new_entity;
}

void MetabolismSystem::processMetabolism(entt::registry& registry, VoxelGrid& voxelGrid,
                                         entt::dispatcher& dispatcher) {
    processingComplete = false;
    int currentEntitiesCount = 0;

    auto view = registry.view<MetabolismComponent, DigestionComponent, EntityTypeComponent>();

    for (auto entity : view) {
        if (!registry.valid(entity)) {
            continue;
        }
        currentEntitiesCount += 1;

        auto& metabolism = view.get<MetabolismComponent>(entity);
        auto& digestionComp = view.get<DigestionComponent>(entity);
        auto& etc = view.get<EntityTypeComponent>(entity);

        // Print the energy reserve
        // std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity) << "): "
        //           << metabolism.energyReserve << std::endl;

        if (metabolism.energyReserve < 0) {
            // Check if the entity has a HealthComponent
            if (registry.any_of<HealthComponent>(entity)) {
                auto& health = registry.get<HealthComponent>(entity);
                health.healthLevel -= 10;

                if (health.healthLevel <= 0) {
                    // Kill the entity
                    dispatcher.enqueue<KillEntityEvent>(entity);
                }
                metabolism.energyReserve += 10;

                // Print the updated energy reserve
                // std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
                //           << "): " << metabolism.energyReserve << std::endl;
            }
        }

        // TODO: move this constant to the configuration manager
        if (metabolism.energyReserve > 100 && etc.mainType == 2 && etc.subType0 != 1 &&
            lastEntitiesCount < MAX_ENTITIES) {
            std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
                      << ") is above the threshold. Cloning the entity and generating new beast."
                      << std::endl;

            metabolism.energyReserve -= 60;

            // Clone the entity
            entt::entity cloned = clone_entity(registry, entity);

            // Clone Position component if it exists
            if (registry.any_of<Position>(cloned)) {
                auto& position = registry.get<Position>(cloned);
                position.y += 1;

                ParentsComponent parentsComponent;
                parentsComponent.parents.push_back(static_cast<uint32_t>(entity));
                registry.emplace<ParentsComponent>(cloned, parentsComponent);

                auto entityId = entt::to_integral(cloned);

                // TODO: create better logic to determine where the new entity will be placed

                voxelGrid.setEntity(position.x, position.y, position.z, entityId);
            } else {
                std::cerr << "Error: Cloned entity does not have a Position component."
                          << std::endl;
            }

            if (registry.any_of<MetabolismComponent>(cloned)) {
                auto& metabolism = registry.get<MetabolismComponent>(cloned);
                metabolism.energyReserve = 10;
            }

            if (registry.any_of<Inventory>(cloned)) {
                auto& inventory = registry.get<Inventory>(cloned);
                inventory.itemIDs.clear();
            }
        } else {
            // Print the updated energy reserve
            // std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
            //             << "): " << metabolism.energyReserve << " It didn't had enough energy to
            //             reproduce." << std::endl;
        }

        if (!digestionComp.digestingItems.empty()) {
            std::vector<int> itemsToRemove;

            for (auto& item : digestionComp.digestingItems) {
                item.processingTime += 1;

                if (item.processingTime % chunkDigestionTime == 0) {
                    double digestedMass = chunkMass * item.convertionEfficiency;
                    double energyInChunk = digestedMass * item.energyDensity;
                    item.mass -= digestedMass;

                    metabolism.energyReserve += energyInChunk;

                    if (registry.any_of<HealthComponent>(entity)) {
                        auto& health = registry.get<HealthComponent>(entity);
                        double healthInChunk = energyInChunk * item.energyHealthRatio;
                        if (healthInChunk > 0 &&
                            health.healthLevel + healthInChunk < health.maxHealth) {
                            health.healthLevel += healthInChunk;
                        } else if (healthInChunk > 0) {
                            health.healthLevel = health.maxHealth;
                        }

                        // Print the updated energy reserve
                        // std::cout << "Energy reserve of the entity (" <<
                        // static_cast<uint32_t>(entity) << "): "
                        //         << metabolism.energyReserve << std::endl;
                    }

                    if (item.mass <= 0) {
                        itemsToRemove.push_back(item.foodItemID);
                    }
                }

                // Print processing time and mass
                // std::cout << "item.processingTime: " << item.processingTime
                //           << "; item.mass: " << item.mass << std::endl;
            }

            // Remove items that have been fully digested
            for (int foodItemID : itemsToRemove) {
                digestionComp.removeItem(foodItemID);
            }
        }
    }

    processingComplete = true;
    lastEntitiesCount = currentEntitiesCount;
}

void MetabolismSystem::processMetabolismAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                                              entt::dispatcher& dispatcher) {
    std::scoped_lock lock(metabolismMutex);  // Ensure exclusive access
    processingComplete = false;
    int currentEntitiesCount = 0;

    auto view = registry.view<MetabolismComponent, DigestionComponent, EntityTypeComponent>();

    for (auto entity : view) {
        if (!registry.valid(entity)) {
            continue;
        }
        currentEntitiesCount += 1;

        auto& metabolism = view.get<MetabolismComponent>(entity);
        auto& digestionComp = view.get<DigestionComponent>(entity);
        auto& etc = view.get<EntityTypeComponent>(entity);

        // Print the energy reserve
        // std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity) << "): "
        //           << metabolism.energyReserve << std::endl;

        if (metabolism.energyReserve < 0) {
            // Check if the entity has a HealthComponent
            if (registry.any_of<HealthComponent>(entity)) {
                auto& health = registry.get<HealthComponent>(entity);
                health.healthLevel -= 10;

                if (health.healthLevel <= 0) {
                    // Kill the entity
                    dispatcher.enqueue<KillEntityEvent>(entity);
                }
                metabolism.energyReserve += 10;

                // Print the updated energy reserve
                std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
                          << "): " << metabolism.energyReserve << std::endl;
            }
        }

        // TODO: move this constant to the configuration manager
        if (metabolism.energyReserve > 300 && etc.mainType == 2 && etc.subType0 != 1 &&
            lastEntitiesCount < MAX_ENTITIES) {
            std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
                      << ") is above the threshold. Cloning the entity and generating new beast."
                      << std::endl;

            metabolism.energyReserve -= 100;

            // Clone the entity
            entt::entity cloned = clone_entity(registry, entity);

            // Clone Position component if it exists
            if (registry.any_of<Position>(cloned)) {
                auto& position = registry.get<Position>(cloned);
                position.y += 1;

                ParentsComponent parentsComponent;
                parentsComponent.parents.push_back(static_cast<uint32_t>(entity));
                registry.emplace<ParentsComponent>(cloned, parentsComponent);

                auto entityId = entt::to_integral(cloned);

                // TODO: create better logic to determine where the new entity will be placed

                voxelGrid.setEntity(position.x, position.y, position.z, entityId);
            } else {
                std::cerr << "Error: Cloned entity does not have a Position component."
                          << std::endl;
            }

            if (registry.any_of<MetabolismComponent>(cloned)) {
                auto& metabolism = registry.get<MetabolismComponent>(cloned);
                metabolism.energyReserve = 100;
            }

            if (registry.any_of<Inventory>(cloned)) {
                auto& inventory = registry.get<Inventory>(cloned);
                inventory.itemIDs.clear();
            }
        } else {
            // Print the updated energy reserve
            // std::cout << "Energy reserve of the entity (" << static_cast<uint32_t>(entity)
            //             << "): " << metabolism.energyReserve << " It didn't had enough energy to
            //             reproduce." << std::endl;
        }

        if (!digestionComp.digestingItems.empty()) {
            std::vector<int> itemsToRemove;

            for (auto& item : digestionComp.digestingItems) {
                item.processingTime += 1;

                if (item.processingTime % chunkDigestionTime == 0) {
                    double digestedMass = chunkMass * item.convertionEfficiency;
                    double energyInChunk = digestedMass * item.energyDensity;
                    item.mass -= digestedMass;

                    metabolism.energyReserve += energyInChunk;

                    if (registry.any_of<HealthComponent>(entity)) {
                        auto& health = registry.get<HealthComponent>(entity);
                        double healthInChunk = energyInChunk * item.energyHealthRatio;
                        if (healthInChunk > 0 &&
                            health.healthLevel + healthInChunk < health.maxHealth) {
                            health.healthLevel += healthInChunk;
                        } else if (healthInChunk > 0) {
                            health.healthLevel = health.maxHealth;
                        }

                        // Print the updated energy reserve
                        // std::cout << "Energy reserve of the entity (" <<
                        // static_cast<uint32_t>(entity) << "): "
                        //         << metabolism.energyReserve << std::endl;
                    }

                    if (item.mass <= 0) {
                        itemsToRemove.push_back(item.foodItemID);
                    }
                }

                // Print processing time and mass
                // std::cout << "item.processingTime: " << item.processingTime
                //           << "; item.mass: " << item.mass << std::endl;
            }

            // Remove items that have been fully digested
            for (int foodItemID : itemsToRemove) {
                digestionComp.removeItem(foodItemID);
            }
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3));

    processingComplete = true;
    lastEntitiesCount = currentEntitiesCount;
}

bool MetabolismSystem::isProcessingComplete() const { return processingComplete; }