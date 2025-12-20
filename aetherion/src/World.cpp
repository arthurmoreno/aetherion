#include "World.hpp"

// #include <pybind11/stl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <execution>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "PerceptionResponse_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "voxelgrid/VoxelGrid.hpp"
#include "physics/PhysicsMutators.hpp"

World::World(int width, int height, int depth)
    : voxelGrid(new VoxelGrid(registry)),
      pyRegistry(registry, dispatcher),
      // Update to use just SQLite file path parameter
      dbHandler(std::make_unique<GameDBHandler>("./data/game.sqlite")),
      physicsEngine(new PhysicsEngine(registry, dispatcher, voxelGrid)),
      lifeEngine(new LifeEngine(registry, dispatcher, voxelGrid)),
      ecosystemEngine(new EcosystemEngine()),
      metabolismSystem(new MetabolismSystem(registry, voxelGrid)),
      combatSystem(new CombatSystem(registry, voxelGrid)),
      effectsSystem(new EffectsSystem(registry, voxelGrid)),
      healthSystem(new HealthSystem(registry, voxelGrid)) {
    // Initialize World dimensions
    this->width = width;
    this->height = height;
    this->depth = depth;

    voxelGrid->initializeGrids();
    voxelGrid->width = width;
    voxelGrid->height = height;
    voxelGrid->depth = depth;

    // Register event handlers
    physicsEngine->registerEventHandlers(dispatcher);
    physicsEngine->registerVoxelGrid(voxelGrid);
    lifeEngine->registerEventHandlers(dispatcher);
    ecosystemEngine->registerEventHandlers(dispatcher);
    ecosystemEngine->waterSimManager_->initializeProcessors(registry, *voxelGrid, dispatcher);

    if (!Py_IsInitialized()) {
        std::cout << "Python was not initialized! Starting python interpreter." << std::endl;
        // nb::scoped_interpreter guard{};
    }
    Logger::getLogger()->info("World created with an empty voxel grid!");
}

World::~World() {
    delete voxelGrid;         // Clean up the VoxelGrid
    delete physicsEngine;     // Clean up the physics engine
    delete lifeEngine;        // Clean up the physics engine
    delete ecosystemEngine;   // Clean up the ecosystem
    delete metabolismSystem;  // Clean up the metabolismSystem
    delete combatSystem;
    delete effectsSystem;
    delete healthSystem;
    // openvdb::uninitialize();
}

void World::removeEntity(entt::entity entity) { destroyEntityHandleWithLifecycleLock(entity); }

// Destroy only the EnTT entity handle. Caller must hold appropriate lifecycle locks.
void World::destroyEntityHandle(entt::entity entity) {
    // Delegate to physics mutator which handles validation, grid cleanup and destruction.
    destroyEntityWithGridCleanup(registry, *voxelGrid, dispatcher, entity, true);
}

// Acquire the lifecycle mutex exclusively and destroy the entity handle.
// This helper is useful for callers that don't already hold `entityLifecycleMutex`.
void World::destroyEntityHandleWithLifecycleLock(entt::entity entity) {
    std::unique_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);
    // Delegate to the existing destroyEntityHandle which assumes caller holds lifecycle guarantees
    destroyEntityHandle(entity);
}

void World::initializeVoxelGrid() {
    // Initializes the grid with default GridData
    voxelGrid->initializeGrids();
}

void World::setVoxel(int x, int y, int z, const GridData& data) {
    voxelGrid->setVoxel(x, y, z, data);
}

GridData World::getVoxel(int x, int y, int z) const { return voxelGrid->getVoxel(x, y, z); }

void World::setTerrain(int x, int y, int z, const EntityInterface& entityInterface) {
    // Create a new entity in the EnTT registry
    // entt::entity entity = registry.create();

    // Assign Position and Velocity components from the EntityInterface
    // if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
    //     registry.emplace<Position>(entity, entityInterface.getComponent<Position>());
    // }

    // int terrainID = static_cast<int>(entity);
    // voxelGrid->setTerrain(x, y, z, terrainID);
    throw std::runtime_error("World::setTerrain not implemented yet");
}

// Create an entity in the EnTT registry with data from EntityInterface
entt::entity World::createEntity(const EntityInterface& entityInterface) {
    // Create a new entity in the EnTT registry
    entt::entity entity = registry.create();

    // Assign Position and Velocity components from the EntityInterface
    if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
        registry.emplace<Position>(entity, entityInterface.getComponent<Position>());
    }
    if (entityInterface.hasComponent(ComponentFlag::VELOCITY)) {
        registry.emplace<Velocity>(entity, entityInterface.getComponent<Velocity>());
    }
    if (entityInterface.hasComponent(ComponentFlag::HEALTH)) {
        registry.emplace<HealthComponent>(entity, entityInterface.getComponent<HealthComponent>());
    }

    // Add the entity to the VoxelGrid with its position

    if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
        GridData gridData = {1, static_cast<int>(entity), 0,
                             0.0f};  // Example data, can be extended
        auto pos = entityInterface.getComponent<Position>();
        voxelGrid->setVoxel(static_cast<int>(pos.x), static_cast<int>(pos.y),
                            static_cast<int>(pos.z), gridData);
    }

    return entity;  // Return the created entity
}

// Create an entity from a Python class and introspect its components
entt::entity World::createEntityFromPython(nb::object pyEntity) {
    entt::entity newEntity = registry.create();

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "entity_type") && !pyEntity.attr("entity_type").is_none()) {
        nb::object pyEntityTypeComp = pyEntity.attr("entity_type");
        EntityTypeComponent entityTypeComp =
            nb::cast<EntityTypeComponent>(pyEntityTypeComp);  // Convert Python Velocity to C++
        registry.emplace<EntityTypeComponent>(newEntity, entityTypeComp);
    }

    // Check if the Python entity has a position attribute
    if (nb::hasattr(pyEntity, "physics_stats") && !pyEntity.attr("physics_stats").is_none()) {
        nb::object pyPhysicsStats = pyEntity.attr("physics_stats");
        PhysicsStats physics_stats =
            nb::cast<PhysicsStats>(pyPhysicsStats);  // Convert Python PhysicsStats to C++
        registry.emplace<PhysicsStats>(newEntity, physics_stats);
    }

    // Check if the Python entity has a position attribute
    if (nb::hasattr(pyEntity, "position") && !pyEntity.attr("position").is_none()) {
        nb::object pyPosition = pyEntity.attr("position");
        Position pos = nb::cast<Position>(pyPosition);  // Convert Python Position to C++
        registry.emplace<Position>(newEntity, pos);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "velocity") && !pyEntity.attr("velocity").is_none()) {
        nb::object pyVelocity = pyEntity.attr("velocity");
        Velocity vel = nb::cast<Velocity>(pyVelocity);  // Convert Python Velocity to C++
        registry.emplace<Velocity>(newEntity, vel);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "structural_integrity") &&
        !pyEntity.attr("structural_integrity").is_none()) {
        nb::object pyStructuralIntegrityComponent = pyEntity.attr("structural_integrity");
        StructuralIntegrityComponent sic =
            nb::cast<StructuralIntegrityComponent>(pyStructuralIntegrityComponent);
        registry.emplace<StructuralIntegrityComponent>(newEntity, sic);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "health") && !pyEntity.attr("health").is_none()) {
        nb::object pyHealthComp = pyEntity.attr("health");
        HealthComponent healthComp =
            nb::cast<HealthComponent>(pyHealthComp);  // Convert Python Velocity to C++
        registry.emplace<HealthComponent>(newEntity, healthComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "perception") && !pyEntity.attr("perception").is_none()) {
        nb::object pyPerceptionComp = pyEntity.attr("perception");
        PerceptionComponent perceptionComp =
            nb::cast<PerceptionComponent>(pyPerceptionComp);  // Convert Python Velocity to C++
        registry.emplace<PerceptionComponent>(newEntity, perceptionComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "inventory") && !pyEntity.attr("inventory").is_none()) {
        nb::object pyInventoryComp = pyEntity.attr("inventory");
        Inventory inventoryComp =
            nb::cast<Inventory>(pyInventoryComp);  // Convert Python Velocity to C++
        registry.emplace<Inventory>(newEntity, inventoryComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "console_logs") && !pyEntity.attr("console_logs").is_none()) {
        nb::object pyConsoleLogsComp = pyEntity.attr("console_logs");
        ConsoleLogsComponent consoleLogsComp =
            nb::cast<ConsoleLogsComponent>(pyConsoleLogsComp);  // Convert Python Velocity to C++
        registry.emplace<ConsoleLogsComponent>(newEntity, consoleLogsComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "fruit_growth") && !pyEntity.attr("fruit_growth").is_none()) {
        nb::object pyFruitGrowthComp = pyEntity.attr("fruit_growth");
        FruitGrowth fruitGrowthComp =
            nb::cast<FruitGrowth>(pyFruitGrowthComp);  // Convert Python Velocity to C++
        registry.emplace<FruitGrowth>(newEntity, fruitGrowthComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "matter_container") && !pyEntity.attr("matter_container").is_none()) {
        nb::object pyMatterContainerComp = pyEntity.attr("matter_container");
        MatterContainer matterContainerComp =
            nb::cast<MatterContainer>(pyMatterContainerComp);  // Convert Python Velocity to C++
        registry.emplace<MatterContainer>(newEntity, matterContainerComp);
    }

    // Optionally handle behavior or other components
    if (nb::hasattr(pyEntity, "behavior") && !pyEntity.attr("behavior").is_none()) {
        nb::object pyBehavior = pyEntity.attr("behavior");
        // Store the behavior function for later use if needed
    }

    // Optionally handle behavior or other components
    if (nb::hasattr(pyEntity, "on_take_item_behavior") &&
        !pyEntity.attr("on_take_item_behavior").is_none()) {
        nb::object pyOnTakeItemBehavior = pyEntity.attr("on_take_item_behavior");
        registry.emplace<OnTakeItemBehavior>(newEntity, pyOnTakeItemBehavior);
    }

    // Optionally handle behavior or other components
    if (nb::hasattr(pyEntity, "on_use_item_behavior") &&
        !pyEntity.attr("on_use_item_behavior").is_none()) {
        nb::object pyOnUseItemBehavior = pyEntity.attr("on_use_item_behavior");
        registry.emplace<OnUseItemBehavior>(newEntity, pyOnUseItemBehavior);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "digestion_comp") && !pyEntity.attr("digestion_comp").is_none()) {
        nb::object pyDigestionComp = pyEntity.attr("digestion_comp");
        DigestionComponent digestionComp = nb::cast<DigestionComponent>(pyDigestionComp);
        registry.emplace<DigestionComponent>(newEntity, digestionComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "metabolism_comp") && !pyEntity.attr("metabolism_comp").is_none()) {
        nb::object pyMetabolismComp = pyEntity.attr("metabolism_comp");
        MetabolismComponent metabolismComp = nb::cast<MetabolismComponent>(pyMetabolismComp);
        registry.emplace<MetabolismComponent>(newEntity, metabolismComp);
    }

    // Check if the Python entity has a velocity attribute
    if (nb::hasattr(pyEntity, "drop_rates") && !pyEntity.attr("drop_rates").is_none()) {
        nb::object pyDropRatesComp = pyEntity.attr("drop_rates");
        DropRates dropRatesComp = nb::cast<DropRates>(pyDropRatesComp);
        registry.emplace<DropRates>(newEntity, dropRatesComp);
    }

    // Optionally handle behavior or other components
    if (nb::hasattr(pyEntity, "grid_type")) {
        nb::object pyGridType = pyEntity.attr("grid_type");
        GridType gridType = nb::cast<GridType>(pyGridType);
        nb::object pyPosition = pyEntity.attr("position");
        Position pos = nb::cast<Position>(pyPosition);

        int entityID = static_cast<int>(newEntity);
        if (gridType == GridType::TERRAIN) {
            voxelGrid->setTerrain(pos.x, pos.y, pos.z, entityID);
        } else if (gridType == GridType::ENTITY) {
            voxelGrid->setEntity(pos.x, pos.y, pos.z, entityID);
        }
    }

    return newEntity;
}

// Get entities based on their type
nb::dict World::getEntitiesByType(int entityMainType, int entitySubType0) {
    // Acquire shared lock to prevent entity destruction during entity queries
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    nb::dict entitiesMetadata;

    // Iterate through all entities that have the EntityTypeComponent
    auto view = registry.view<EntityTypeComponent>();
    for (auto entity : view) {
        // Retrieve the entity type component
        EntityTypeComponent* entityTypeComp = registry.try_get<EntityTypeComponent>(entity);

        // If the entity's type matches the input type
        if (entityTypeComp && entityTypeComp->mainType == entityMainType &&
            entityTypeComp->subType0 == entitySubType0) {
            // Retrieve additional entity components, such as position or other
            // metadata
            EntityInterface entityInterface = createEntityInterface(registry, entity);

            // Store the entity interface in the dictionary with the entity ID as the
            // key
            entitiesMetadata[nb::int_(static_cast<int>(entity))] = nb::cast(entityInterface);
        }
    }

    return entitiesMetadata;
}

// Get entity IDs based on their type
nb::list World::getEntityIdsByType(int entityMainType, int entitySubType0) {
    // Acquire shared lock to prevent entity destruction during entity ID queries
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    nb::list entityIds;

    // Iterate through all entities that have the EntityTypeComponent
    auto view = registry.view<EntityTypeComponent, PerceptionComponent>();
    for (auto entity : view) {
        // Retrieve the entity type component
        EntityTypeComponent* entityTypeComp = registry.try_get<EntityTypeComponent>(entity);
        PerceptionComponent* perceptionComp = registry.try_get<PerceptionComponent>(entity);

        // If the entity's type matches the input type
        if (entityTypeComp && perceptionComp && entityTypeComp->mainType == entityMainType &&
            entityTypeComp->subType0 == entitySubType0) {
            // Add the entity ID to the list
            entityIds.append(nb::int_(static_cast<int>(entity)));
        }
    }

    return entityIds;
}

nb::dict World::createPerceptionResponses(nb::dict entitiesWithQueries) {
    const size_t BATCH_NUMBER = 16;
    nb::dict perceptionResponses;

    // Acquire shared lock to prevent entity destruction during perception creation
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    // We'll collect a list of (entityId, vectorOfCommands) that we can process in threads
    struct Job {
        int entityId;
        std::vector<QueryCommand> commands;
    };
    std::vector<Job> jobs;
    jobs.reserve(entitiesWithQueries.size());

    {
        // Acquire GIL only while reading Python dict
        nb::gil_scoped_acquire gil;

        for (auto item : entitiesWithQueries) {
            int entityId = nb::cast<int>(item.first);
            nb::list optionalQueries = nb::cast<nb::list>(item.second);

            // Convert to native data
            std::vector<QueryCommand> commands = toCommandList(optionalQueries);

            // Optionally clear the Python list if needed
            optionalQueries.attr("clear")();

            // Store the job in a vector
            jobs.push_back({entityId, std::move(commands)});
        }
    }  // GIL is released automatically here when gil_scoped_acquire goes out of scope

    std::vector<std::future<std::vector<std::pair<int, std::vector<char>>>>> futures;
    futures.reserve(BATCH_NUMBER);  // We'll have up to 8 futures

    {
        // Release GIL so we can spawn threads without Python locking
        nb::gil_scoped_release gil;

        // Fixed number of batches
        const size_t numBatches = BATCH_NUMBER;
        // Compute how many jobs per batch (rounded up)
        const size_t batchSize = (jobs.size() + numBatches - 1) / numBatches;

        // For each batch, spawn one async task
        for (size_t batchIndex = 0; batchIndex < numBatches; ++batchIndex) {
            // Calculate subrange [start, end)
            const size_t start = batchIndex * batchSize;
            if (start >= jobs.size()) {
                break;  // no more jobs
            }
            const size_t end = std::min(start + batchSize, jobs.size());

            // Capture this slice in an async task
            futures.push_back(std::async(std::launch::async, [this, start, end, &jobs]() {
                // Each batch processes its own slice of jobs
                std::vector<std::pair<int, std::vector<char>>> batchResult;
                batchResult.reserve(end - start);

                for (size_t i = start; i < end; ++i) {
                    auto& job = jobs[i];
                    std::vector<char> serializedResponse;

                    try {
                        serializedResponse = createPerceptionResponseC(job.entityId, job.commands);
                    } catch (const std::exception& e) {
                        // Log the error for debugging
                        Logger::getLogger()->error(
                            "Failed to create perception response for entity " +
                            std::to_string(job.entityId) + ": " + e.what());

                        // Create an empty response or null response to handle later
                        serializedResponse.clear();
                        // throw std::runtime_error("Failed to create perception response");
                    }

                    batchResult.emplace_back(job.entityId, std::move(serializedResponse));
                }
                return batchResult;
            }));
        }
    }

    // Reacquire GIL to populate `perceptionResponses` from all batch results
    {
        nb::gil_scoped_acquire gil;

        // Collect results from each batch
        for (auto& fut : futures) {
            std::vector<std::pair<int, std::vector<char>>> batchResult = fut.get();
            // Insert each (entityId, serializedResponse) into the final dict
            for (auto& [entityId, serializedResponse] : batchResult) {
                nb::bytes resp(serializedResponse.data(), serializedResponse.size());
                perceptionResponses[nb::int_(entityId)] = resp;
            }
        }
    }

    return perceptionResponses;
}

EntityInterface World::getEntityById(int entityId) {
    // Convert the entity ID to the entt::entity type
    entt::entity entity = static_cast<entt::entity>(entityId);

    // Acquire shared lock to prevent entity destruction during perception
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    // CRITICAL: Always check entity validity first
    if (!registry.valid(entity)) {
        // Logger::getLogger()->error("[getEntityById] Entity " + std::to_string(entityId) + " is
        // invalid");
        throw std::runtime_error("Entity " + std::to_string(entityId) + " is no longer valid");
    }

    // Logger::getLogger()->debug("[getEntityById] Entity " + std::to_string(entityId) + " is valid,
    // checking components");

    // Check if entity has required components BEFORE accessing them
    if (!registry.all_of<Position>(entity)) {
        // Logger::getLogger()->error("[getEntityById] Entity " + std::to_string(entityId) + "
        // missing Position component");
        throw std::runtime_error("Entity " + std::to_string(entityId) +
                                 " does not have Position component");
    }

    // TODO: Make this a more robust check.
    Position position = registry.get<Position>(entity);
    int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
    if (entityIdVoxel != entityId) {
        std::cout << "Warning: Entity " << entityId << " is not at its recorded voxel position ("
                  << position.x << "," << position.y << "," << position.z
                  << "). Actual voxel entity: " << entityIdVoxel << std::endl;
        throw std::runtime_error("Entity Position mismatch with VoxelGrid");
    }

    EntityInterface entityInterface = createEntityInterface(registry, entity);

    return entityInterface;
}

// Helper function to get bounds for perception
int World::getPerceptionBounds(int pos, int perception) const { return pos - perception; }

struct SerializableEntity {
    int entityId;
    float posX, posY, posZ;
    float velX, velY, velZ;
    int health;
    int entityType;
};

int World::getTerrain(int x, int y, int z) { return voxelGrid->getTerrain(x, y, z); }

int World::getEntity(int x, int y, int z) { return voxelGrid->getEntity(x, y, z); }

void World::dispatchMoveSolidEntityEventById(int entityId,
                                             std::vector<DirectionEnum> directionsToApply) {
    // Acquire shared lock to prevent entity destruction during movement dispatch
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    entt::entity entity = static_cast<entt::entity>(entityId);

    // TODO: Make this a more robust check.
    Position position = registry.get<Position>(entity);
    int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
    if (entityIdVoxel != entityId) {
        std::string errorMessage = "Entity id on EntityInterface: " + std::to_string(entityId) +
                                   " Position on EntityInterface: (" + std::to_string(position.x) +
                                   "," + std::to_string(position.y) + "," +
                                   std::to_string(position.z) + ")" +
                                   "Entity id on VoxelGrid: " + std::to_string(entityIdVoxel);
        throw std::runtime_error(errorMessage);
    }

    // Safely get PhysicsStats
    if (auto* physicsStats = registry.try_get<PhysicsStats>(entity)) {
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        float deltaZ = 0.0f;

        for (const auto& direction : directionsToApply) {
            switch (direction) {
                case DirectionEnum::LEFT:
                    deltaX -= physicsStats->forceX;
                    break;
                case DirectionEnum::RIGHT:
                    deltaX += physicsStats->forceX;
                    break;
                case DirectionEnum::UP:
                    deltaY -= physicsStats->forceY;
                    break;
                case DirectionEnum::DOWN:
                    deltaY += physicsStats->forceY;
                    break;
                case DirectionEnum::UPWARD:
                    deltaZ += physicsStats->forceZ;
                    break;
                case DirectionEnum::DOWNWARD:
                    deltaZ -= physicsStats->forceZ;
                    break;
                default:
                    std::cerr << "Unknown direction: " << static_cast<int>(direction) << "\n";
                    break;
            }
        }

        dispatcher.enqueue<MoveSolidEntityEvent>(entity, deltaX, deltaY, deltaZ);
    } else {
        std::cout << "Entity does not have PhysicsStats component.\n";
    }
}

void World::dispatchMoveSolidEntityEventByPosition(int x, int y, int z, GridType gridType,
                                                   float deltaX, float deltaY, float deltaZ) {
    if (gridType == GridType::ENTITY) {
        // Get the entity at the given position in the voxel grid
        int entityID = voxelGrid->getEntity(x, y, z);

        if (entityID != -1) {
            entt::entity entity = static_cast<entt::entity>(entityID);

            dispatcher.enqueue<MoveSolidEntityEvent>(entity, deltaX, deltaY, deltaZ);
        } else {
            std::cout << "No entity found at the given coordinates.\n";
        }
    } else if (gridType == GridType::TERRAIN) {
        // If it's terrain, we might not be moving it, but you could handle other
        // events
        std::cout << "Terrain movement not supported.\n";
    } else {
        std::cout << "Event not dispatched.\n";
    }
}

void World::dispatchTakeItemEventById(int entityId, int hoveredEntityId, int selectedEntityId) {
    // Acquire shared lock to prevent entity destruction during item take dispatch
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    entt::entity entity = static_cast<entt::entity>(entityId);

    // TODO: Make this a more robust check.
    Position position = registry.get<Position>(entity);
    int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
    if (entityIdVoxel != entityId) {
        std::string errorMessage = "Entity id on EntityInterface: " + std::to_string(entityId) +
                                   " Position on EntityInterface: (" + std::to_string(position.x) +
                                   "," + std::to_string(position.y) + "," +
                                   std::to_string(position.z) + ")" +
                                   "Entity id on VoxelGrid: " + std::to_string(entityIdVoxel);
        throw std::runtime_error(errorMessage);
    }

    // Safely get PhysicsStats
    if (auto* inventory = registry.try_get<Inventory>(entity)) {
        nb::object pyRegistryObj = nb::cast(&pyRegistry);
        dispatcher.enqueue<TakeItemEvent>(entity, pyRegistryObj, voxelGrid, hoveredEntityId,
                                          selectedEntityId);
    } else {
        std::cout << "Entity does not have Inventory component.\n";
    }
}

void World::dispatchUseItemEventById(int entityId, int itemSlot, int hoveredEntityId,
                                     int selectedEntityId) {
    // Acquire shared lock to prevent entity destruction during item use dispatch
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    entt::entity entity = static_cast<entt::entity>(entityId);

    // TODO: Make this a more robust check.
    Position position = registry.get<Position>(entity);
    int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
    if (entityIdVoxel != entityId) {
        std::cout << "Warning: Entity " << entityId << " is not at its recorded voxel position ("
                  << position.x << "," << position.y << "," << position.z
                  << "). Actual voxel entity: " << entityIdVoxel << std::endl;
        throw std::runtime_error("Entity Position mismatch with VoxelGrid");
    }

    // Safely get PhysicsStats
    if (auto* inventory = registry.try_get<Inventory>(entity)) {
        nb::object pyRegistryObj = nb::cast(&pyRegistry);
        dispatcher.enqueue<UseItemEvent>(entity, pyRegistryObj, voxelGrid, itemSlot,
                                         hoveredEntityId, selectedEntityId);
    } else {
        std::cout << "Entity does not have Inventory component.\n";
    }
}

void World::dispatchSetEntityToDebug(int entityId) {
    entt::entity entity = static_cast<entt::entity>(entityId);
    dispatcher.enqueue<SetEcoEntityToDebug>(entity);
    dispatcher.enqueue<SetPhysicsEntityToDebug>(entity);
}

void World::addPythonSystem(nb::object system) {
    // Optional: Validate that the system has an 'update' method
    if (!nb::hasattr(system, "update")) {
        throw std::runtime_error("Python system must have an 'update' method.");
    }

    // Optionally, perform additional validations or initializations

    pythonSystems.emplace_back(system);
}

nb::object World::getPythonSystem(size_t index) const {
    if (index >= pythonSystems.size()) {
        throw std::out_of_range("Python system index out of range.");
    }
    return pythonSystems[index];
}

void World::addPythonScript(std::string& key, nb::object script) {
    // Optional: Validate that the script has an 'update' method
    if (!nb::hasattr(script, "run")) {
        throw std::runtime_error("Python script must have an 'update' method.");
    }

    // Optionally, perform additional validations or initializations

    pythonScripts[key] = script;
}

void World::runPythonScript(std::string& key) {
    if (pythonScripts.find(key) != pythonScripts.end()) {
        nb::gil_scoped_acquire acquire;
        nb::object pyRegistryObj = nb::cast(&pyRegistry);

        nb::object script = pythonScripts[key];

        try {
            script.attr("run")(pyRegistryObj, voxelGrid);
        } catch (const nb::cast_error& e) {
            std::cerr << "Error in Python script run: " << e.what() << std::endl;
        }
    } else {
        throw std::runtime_error("Python script key not found.");
    }
}

void World::registerPythonEventHandler(const std::string& eventType, nb::object callback) {
    // Validate that the callback is callable
    if (!nb::hasattr(callback, "__call__")) {
        throw std::runtime_error("Python callback must be callable");
    }

    // Store the callback
    pythonEventCallbacks[eventType].push_back(callback);

    // Register C++ handlers if this is the first callback for this event type
    if (pythonEventCallbacks[eventType].size() == 1) {
        // if (eventType == "MoveSolidEntityEvent") {
        //     dispatcher.sink<MoveSolidEntityEvent>().connect<&World::onMoveSolidEntityEventPython>(*this);
        if (eventType == "TakeItemEvent") {
            dispatcher.sink<TakeItemEvent>().connect<&World::onTakeItemEventPython>(*this);
        } else if (eventType == "UseItemEvent") {
            dispatcher.sink<UseItemEvent>().connect<&World::onUseItemEventPython>(*this);
        }
        // Add more event types as needed
    }
}

void World::onTakeItemEventPython(const TakeItemEvent& event) {
    nb::gil_scoped_acquire acquire;

    auto it = pythonEventCallbacks.find("TakeItemEvent");
    if (it != pythonEventCallbacks.end()) {
        nb::dict eventData;
        eventData["entity_id"] = nb::int_(static_cast<int>(event.entity));
        eventData["hovered_entity_id"] = nb::int_(event.hoveredEntityId);
        eventData["selected_entity_id"] = nb::int_(event.selectedEntityId);
        eventData["event_type"] = nb::str("TakeItemEvent");

        for (const auto& callback : it->second) {
            try {
                callback(eventData, nb::cast(&pyRegistry));
            } catch (const nb::cast_error& e) {
                Logger::getLogger()->error("Error in Python TakeItemEvent callback: " +
                                           std::string(e.what()));
            }
        }
    }
}

void World::onUseItemEventPython(const UseItemEvent& event) {
    nb::gil_scoped_acquire acquire;

    auto it = pythonEventCallbacks.find("UseItemEvent");
    if (it != pythonEventCallbacks.end()) {
        nb::dict eventData;
        eventData["entity_id"] = nb::int_(static_cast<int>(event.entity));
        eventData["item_slot"] = nb::int_(event.itemSlot);
        eventData["hovered_entity_id"] = nb::int_(event.hoveredEntityId);
        eventData["selected_entity_id"] = nb::int_(event.selectedEntityId);
        eventData["event_type"] = nb::str("UseItemEvent");

        for (const auto& callback : it->second) {
            try {
                callback(eventData, nb::cast(&pyRegistry));
            } catch (const nb::cast_error& e) {
                Logger::getLogger()->error("Error in Python UseItemEvent callback: " +
                                           std::string(e.what()));
            }
        }
    }
}

void safeExecute(const std::function<void()>& func, const std::string& taskName) {
    try {
        func();  // Execute the task
    } catch (const std::exception& e) {
        Logger::getLogger()->error(taskName + " async task crashed: " + e.what());
        // Optionally, implement additional error handling here (e.g., logging, retry mechanisms)
    } catch (...) {
        Logger::getLogger()->error(taskName + " async task crashed with an unknown error.");
        // Handle non-standard exceptions if necessary
    }
}

void World::update() {
    // std::cout << "World update started!" << std::endl;

    gameClock.tick();
    // Placeholder for the world update logic (e.g., physics, AI, etc.)

    std::lock_guard<std::mutex> lock(registryMutex);

    healthSystem->processHealth(registry, *voxelGrid, dispatcher);
    dispatcher.update();
    physicsEngine->processPhysics(registry, *voxelGrid, dispatcher, gameClock);
    if (!processMetabolismAsync) {
        metabolismSystem->processMetabolism(registry, *voxelGrid, dispatcher);
    }
    ecosystemEngine->processEcosystem(registry, *voxelGrid, dispatcher, gameClock);
    effectsSystem->processEffects(registry, *voxelGrid, dispatcher);

    // Acquire GIL before executing Python code
    nb::gil_scoped_acquire acquire;
    nb::object pyRegistryObj = nb::cast(&pyRegistry);

    // Update Python systems
    for (auto& system : pythonSystems) {
        try {
            system.attr("update")(pyRegistryObj, voxelGrid);
        } catch (const nb::cast_error& e) {
            std::cerr << "Error in Python system update: " << e.what() << std::endl;
        }
    }

    bool hasEntitiesToDelete = !lifeEngine->entitiesToDelete.empty();
    bool hasAnyCleanup = hasEntitiesToDelete;

    // Check if any async tasks are still running
    bool anyAsyncTasksRunning =
        (physicsFuture.valid() &&
         physicsFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) ||
        (ecosystemFuture.valid() &&
         ecosystemFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) ||
        (metabolismFuture.valid() &&
         metabolismFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready);

    // Only perform cleanup if we have cleanup work AND no async tasks are running
    if (hasAnyCleanup && !anyAsyncTasksRunning) {
        // SAFE to do cleanup - no async tasks are accessing the data
        if (hasEntitiesToDelete) {
            // Acquire EXCLUSIVE lock to prevent any perception operations during entity destruction
            std::unique_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

            // Locking contract: acquire `entityLifecycleMutex` (exclusive) first,
            // then acquire a `TerrainGridLock` if modifying terrain. This prevents
            // deadlocks with perception readers and other terrain operations.

            // std::cout << "\n=== ENTITY DELETION DEBUG ===" << std::endl;
            // std::cout << "Total entities to delete: " << lifeEngine->entitiesToDelete.size()
            //           << std::endl;

            for (const auto& [entity, softKill] : lifeEngine->entitiesToDelete) {
                int entityId = static_cast<int>(entity);
                bool isSpecialId = entityId == -1 || entityId == -2;
                bool isValidEntity = registry.valid(entity);

                // std::cout << "\n--- Processing deletion request ---" << std::endl;
                // std::cout << "Entity handle: " << static_cast<uint32_t>(entity) << std::endl;
                // std::cout << "Entity ID (cast): " << entityId << std::endl;
                // std::cout << "Is special ID: " << isSpecialId << std::endl;
                // std::cout << "Registry valid: " << isValidEntity << std::endl;
                // std::cout << "Soft kill: " << softKill << std::endl;

                if (!isSpecialId && isValidEntity) {
                    // Get entity details before destruction
                    if (registry.all_of<Position, EntityTypeComponent>(entity)) {
                        auto [pos, type] = registry.get<Position, EntityTypeComponent>(entity);
                        // std::cout << "Entity position: (" << pos.x << "," << pos.y << "," << pos.z
                        //           << ")" << std::endl;
                        // std::cout << "Entity type: " << type.mainType << "," << type.subType0
                        //           << std::endl;

                        // Check what's actually in the voxel grid at this position
                        int gridEntity = voxelGrid->getEntity(pos.x, pos.y, pos.z);
                        // std::cout << "Grid entity at position: " << gridEntity << std::endl;

                        if (gridEntity != entityId) {
                            std::cout << "ERROR: Grid mismatch! Grid has " << gridEntity
                                      << " but trying to delete " << entityId << std::endl;
                        }
                    }

                    // Decide whether to remove from grid (true for hard kills)
                    const bool shouldRemoveFromGrid = !softKill;

                    // Caller holds `entityLifecycleMutex` (see above). Delegate full
                    // destruction and grid cleanup into a single helper that will
                    // acquire `TerrainGridLock` when needed.
                    destroyEntityWithGridCleanup(registry, *voxelGrid, dispatcher, entity,
                                                  shouldRemoveFromGrid);
                    // std::cout << "Destroyed entity " << entityId << std::endl;
                } else {
                    if (isSpecialId) {
                        std::cout << "Skipping special ID: " << entityId << std::endl;
                    } else if (!isValidEntity) {
                        std::cout << "Entity " << entityId << " already invalid, skipping"
                                  << std::endl;
                    }

                    std::ostringstream ossMessage;
                    ossMessage << "Warning: Attempted to delete invalid or special entity ID "
                               << entityId << ".";
                    spdlog::get("console")->warn(ossMessage.str());
                }
            }

            // std::cout << "=== END ENTITY DELETION DEBUG ===\n" << std::endl;
            lifeEngine->entitiesToDelete.clear();
        }
    }

    // Handle async task lifecycle - always check for completed tasks and launch new ones
    // Handle Physics Async Task
    if (!physicsFuture.valid() ||
        physicsFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        // Optionally handle exceptions from the previous task
        if (physicsFuture.valid()) {
            try {
                physicsFuture.get();  // This will rethrow any exception from the async task
            } catch (const std::exception& e) {
                std::cerr << "PhysicsEngine async task crashed: " << e.what() << std::endl;
                // Implement additional error handling here (e.g., retry limits, state cleanup)
            }
        }

        // Launch a new async task using the standalone safeExecute
        physicsFuture = std::async(
            std::launch::async, safeExecute,
            [this]() {
                physicsEngine->processPhysicsAsync(registry, *voxelGrid, dispatcher, gameClock);
            },
            "PhysicsEngine");
    }

    // Handle Ecosystem Async Task
    if (!ecosystemFuture.valid() ||
        ecosystemFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        if (ecosystemFuture.valid()) {
            try {
                ecosystemFuture.get();
            } catch (const std::exception& e) {
                std::cerr << "EcosystemEngine async task crashed: " << e.what() << std::endl;
                // Handle the exception
            }
        }

        ecosystemFuture = std::async(
            std::launch::async, safeExecute,
            [this]() {
                ecosystemEngine->processEcosystemAsync(registry, *voxelGrid, dispatcher, gameClock);
            },
            "EcosystemEngine");
    }

    if (processMetabolismAsync) {
        // Handle Metabolism Async Task
        if (!metabolismFuture.valid() ||
            metabolismFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            if (metabolismFuture.valid()) {
                try {
                    metabolismFuture.get();
                } catch (const std::exception& e) {
                    std::cerr << "MetabolismSystem async task crashed: " << e.what() << std::endl;
                    // Handle the exception
                }
            }

            metabolismFuture = std::async(
                std::launch::async, safeExecute,
                [this]() {
                    metabolismSystem->processMetabolismAsync(registry, *voxelGrid, dispatcher);
                },
                "MetabolismSystem");
        }
    }
}

void World::putTimeSeries(const std::string& seriesName, long long timestamp, double value) {
    // Logger::getLogger()->debug("[World::putTimeSeries] Called");
    dbHandler->putTimeSeries(seriesName, timestamp, value);
}

std::vector<std::pair<uint64_t, double>> World::queryTimeSeries(const std::string& seriesName,
                                                                long long start, long long end) {
    return dbHandler->queryTimeSeries(seriesName, start, end);
}

void World::executeSQL(const std::string& sql) { dbHandler->executeSQL(sql); }