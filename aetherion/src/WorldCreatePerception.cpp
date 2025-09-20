#include "Logger.hpp"
#include "World.hpp"

// Step 1: Retrieve entity's position and perception component
std::pair<const Position&, const PerceptionComponent&> getEntityPositionAndPerception(
    entt::registry& registry, entt::entity entity) {
    auto allView = registry.view<Position, EntityTypeComponent, PerceptionComponent>();
    if (!allView.contains(entity)) {
        throw std::runtime_error("Entity does not have Position or PerceptionComponent");
    }

    const Position& pos = allView.get<Position>(entity);
    const PerceptionComponent& perception = allView.get<PerceptionComponent>(entity);

    return {pos, perception};
}

// Step 2: Compute perception area boundaries
std::tuple<int, int, int, int, int, int> computePerceptionArea(
    const Position& pos, const PerceptionComponent& perception) {
    int x_min = pos.x - perception.getPerceptionArea();
    int x_max = pos.x + perception.getPerceptionArea();
    int y_min = pos.y - perception.getPerceptionArea();
    int y_max = pos.y + perception.getPerceptionArea();
    int z_min = pos.z - perception.getZPerceptionArea();
    int z_max = pos.z + perception.getZPerceptionArea();

    return {x_min, x_max, y_min, y_max, z_min, z_max};
}

// Step 3: Initialize the VoxelGridView
VoxelGridView initializeVoxelGridView(int x_min, int x_max, int y_min, int y_max, int z_min,
                                      int z_max) {
    int view_width = x_max - x_min + 1;
    int view_height = y_max - y_min + 1;
    int view_depth = z_max - z_min + 1;

    if (view_width <= 0 || view_height <= 0 || view_depth <= 0) {
        throw std::runtime_error("Invalid dimensions for VoxelGridView");
    }

    VoxelGridView voxelGridView;
    voxelGridView.initVoxelGridView(view_width, view_height, view_depth, x_min, y_min, z_min);
    return voxelGridView;
}

// Step 4: Populate EntityInterface with additional components
void populateEntityInterface(entt::registry& registry, entt::entity entity,
                             EntityInterface& entity_interface, PerceptionResponse& response) {
    // Handle Inventory
    if (registry.all_of<Inventory>(entity)) {
        const Inventory& inventory = registry.get<Inventory>(entity);
        for (int itemID : inventory.itemIDs) {
            entt::entity itemEntity = static_cast<entt::entity>(itemID);

            auto itemEnumView = registry.view<ItemEnum>();
            if (itemEnumView.contains(itemEntity)) {
                EntityInterface item_entity_interface;
                item_entity_interface.entityId = itemID;

                const ItemEnum& itemEnum = itemEnumView.get<ItemEnum>(itemEntity);
                item_entity_interface.setComponent<ItemEnum>(itemEnum);

                auto foodItemView = registry.view<FoodItem>();
                if (itemEnum == ItemEnum::FOOD && foodItemView.contains(itemEntity)) {
                    const FoodItem& foodItem = foodItemView.get<FoodItem>(itemEntity);
                    item_entity_interface.setComponent<FoodItem>(foodItem);
                }

                // Add item_entity_interface to response.itemsEntities
                response.itemsEntities.emplace(item_entity_interface.entityId,
                                               std::move(item_entity_interface));
            }
        }
    }

    // Handle ConsoleLogsComponent
    if (registry.all_of<ConsoleLogsComponent>(entity)) {
        const ConsoleLogsComponent& consoleLogs = registry.get<ConsoleLogsComponent>(entity);
        entity_interface.setComponent<ConsoleLogsComponent>(consoleLogs);
        // TODO: Do the flushing here
    }
}

// Step 5: Process terrain voxels
std::vector<int> processTerrainVoxels(entt::registry& registry, VoxelGrid& voxelGrid, int x_min,
                                      int x_max, int y_min, int y_max, int z_min, int z_max,
                                      const Position& pos, VoxelGridView& voxelGridView) {
    std::vector<int> terrainsIds;
    // auto logger = Logger::getLogger();

    // Use efficient region-based terrain query instead of individual coordinate lookups
    std::vector<VoxelGridCoordinates> terrainCoords =
        voxelGrid.getAllTerrainInRegion(x_min, y_min, z_min, x_max, y_max, z_max);

    // Reserve space for efficiency
    terrainsIds.reserve(terrainCoords.size());

    // Process each terrain voxel found in the region
    for (const auto& coord : terrainCoords) {
        int x = coord.x;
        int y = coord.y;
        int z = coord.z;

        // Get terrain ID at this coordinate
        int terrainId = voxelGrid.getTerrain(x, y, z);
        if (terrainId != 0) {  // Assuming 0 means no terrain
            // Use terrain ID directly
            int entity_id = terrainId;

            bool isCurrentTerrainOccluded = false;
            int neighborTerrainId = voxelGrid.getTerrain(x + 1, y + 1, z + 1);
            if (neighborTerrainId != 0) {
                // Use neighbor terrain ID directly
                int neighborEntityId = neighborTerrainId;
                entt::entity neighborTerrainEntity = static_cast<entt::entity>(neighborEntityId);

                // Guarded access to EntityTypeComponent
                const EntityTypeComponent* terrainEtcPtr =
                    registry.try_get<EntityTypeComponent>(neighborTerrainEntity);
                if (!terrainEtcPtr) {
                    std::cout << "perception: neighbor terrain entity " << neighborEntityId
                              << " missing EntityTypeComponent at (" << (x + 1) << ", " << (y + 1)
                              << ", " << (z + 1) << ")\n";
                    // logger->debug(
                    //     "perception: neighbor terrain entity {} missing EntityTypeComponent at "
                    //     "({}, {}, {})",
                    //     neighborEntityId, x + 1, y + 1, z + 1);
                    continue;
                }
                const EntityTypeComponent& terrainEtc = *terrainEtcPtr;

                // Define meaningful boolean variables for complex conditions
                bool hasValidNeighbor = (neighborEntityId != -1);
                bool isMainTypeTerrain = (terrainEtc.mainType == 0);
                bool isSubTypeOccluding = terrainEtc.subType0 != 1 &&
                                          (terrainEtc.subType1 == 0 || terrainEtc.subType1 == 1);

                // First, check if the terrain is exactly one level below the player
                bool isOneLevelBelow = (z == pos.z - 1);

                // Then, check if the terrain position is directly below or adjacent in
                // a cross pattern
                bool isAdjacentInCross = (x == pos.x && y == pos.y) ||      // Directly below
                                         (x == pos.x + 1 && y == pos.y) ||  // +X direction
                                         (x == pos.x - 1 && y == pos.y) ||  // -X direction
                                         (x == pos.x && y == pos.y + 1) ||  // +Y direction
                                         (x == pos.x && y == pos.y - 1);    // -Y direction

                // Combine the two conditions with isMainTypeTerrain
                bool isTerrainNearPlayer =
                    isMainTypeTerrain && isOneLevelBelow && isAdjacentInCross;

                // This is necessary because we might use transparency during rendering
                // to show player below terrain
                if (isTerrainNearPlayer) {
                    isCurrentTerrainOccluded = false;
                } else if (hasValidNeighbor && ((isMainTypeTerrain && isSubTypeOccluding))) {
                    isCurrentTerrainOccluded = true;
                }
                std::cout << "perception: occlusion check at (" << x << ", " << y << ", " << z
                          << ") id=" << entity_id << " neighbor_id=" << neighborEntityId
                          << " hasNeighbor=" << hasValidNeighbor
                          << " mainTerrain=" << isMainTypeTerrain
                          << " subOccluding=" << isSubTypeOccluding
                          << " neighOccluded=" << isCurrentTerrainOccluded
                          << " nearPlayer=" << isTerrainNearPlayer
                          << " -> occluded=" << isCurrentTerrainOccluded << "\n";
                // logger->debug(
                //     "perception: terrain ({}, {}, {}) id={} neighbor_id={} near_player={} "
                //     "occluding={} -> occluded={}",
                //     x, y, z, entity_id, neighborEntityId, isTerrainNearPlayer,
                //     isSubTypeOccluding, isCurrentTerrainOccluded);
            }

            if (isCurrentTerrainOccluded) {
                voxelGridView.setTerrainVoxel(x, y, z, -2);
            } else {
                voxelGridView.setTerrainVoxel(x, y, z, entity_id);
                terrainsIds.emplace_back(entity_id);
            }
        }
    }

    return terrainsIds;
}

// Step 7: Combine terrain and entity IDs
std::vector<int> combineEntityIds(const std::vector<int>& terrainsIds,
                                  const std::vector<int>& entitiesIds) {
    std::vector<int> combinedIds = terrainsIds;
    combinedIds.insert(combinedIds.end(), entitiesIds.begin(), entitiesIds.end());
    return combinedIds;
}

// Step 8: Create EntityInterfaces for entities in view
void createEntityInterfacesForEntitiesInView(
    entt::registry& registry, const std::vector<int>& combinedIds,
    std::unordered_map<int, EntityInterface>& entitiesMap) {
    // Create the views
    auto allView = registry.view<Position, EntityTypeComponent>();
    auto velocityView = registry.view<Velocity>();
    auto movingComponentView = registry.view<MovingComponent>();
    auto healthView = registry.view<HealthComponent>();
    auto inventoryView = registry.view<Inventory>();
    auto matterContainerView = registry.view<MatterContainer>();

    // Preallocate space
    entitiesMap.reserve(combinedIds.size());

    for (int entityId : combinedIds) {
        if (entityId != -1) {
            entt::entity entity = static_cast<entt::entity>(entityId);
            if (!allView.contains(entity)) {
                continue;
            }

            EntityInterface entity_interface;
            entity_interface.entityId = entityId;

            const Position& pos = allView.get<Position>(entity);
            const EntityTypeComponent& etc = allView.get<EntityTypeComponent>(entity);

            entity_interface.setComponent<Position>(pos);
            entity_interface.setComponent<EntityTypeComponent>(etc);

            if (etc.mainType != 0 && velocityView.contains(entity)) {
                const Velocity& velocity = velocityView.get<Velocity>(entity);
                entity_interface.setComponent<Velocity>(velocity);
            }

            if (etc.mainType != 0 && movingComponentView.contains(entity)) {
                const MovingComponent& movingComponent =
                    movingComponentView.get<MovingComponent>(entity);
                entity_interface.setComponent<MovingComponent>(movingComponent);
            }

            if (etc.mainType != 0 && healthView.contains(entity)) {
                const HealthComponent& health = healthView.get<HealthComponent>(entity);
                entity_interface.setComponent<HealthComponent>(health);
            }

            if (etc.mainType != 0 && etc.mainType == 1 && inventoryView.contains(entity)) {
                const Inventory& inventory = inventoryView.get<Inventory>(entity);
                entity_interface.setComponent<Inventory>(inventory);
            }

            if (etc.mainType == 0 && matterContainerView.contains(entity)) {
                const MatterContainer& matterContainer =
                    matterContainerView.get<MatterContainer>(entity);
                entity_interface.setComponent<MatterContainer>(matterContainer);
            }

            // Emplace into the map
            entitiesMap.emplace(entity_interface.entityId, std::move(entity_interface));
        }
    }
}

// // Main method
// nb::bytes World::createPerceptionResponse(int entityId, nb::dict optionalQueries) {
//     entt::entity entity = static_cast<entt::entity>(entityId);
//     PerceptionResponse response;

//     // Step 1: Retrieve the entity's position and perception component
//     auto [pos, perception] = getEntityPositionAndPerception(registry, entity);

//     // Step 2: Compute the perception area boundaries
//     auto [x_min, x_max, y_min, y_max, z_min, z_max] = computePerceptionArea(pos, perception);

//     // Step 3: Initialize the VoxelGridView
//     VoxelGridView voxelGridView = initializeVoxelGridView(x_min, x_max, y_min, y_max, z_min,
//     z_max);

//     // Step 4: Create EntityInterface for the entity
//     EntityInterface entity_interface = createEntityInterface(registry, entity);

//     // Populate entity interface with additional components
//     populateEntityInterface(registry, entity, entity_interface, response);

//     response.entity = entity_interface;

//     // Step 5: Process terrain voxels in the perception area
//     std::vector<int> terrainsIds = processTerrainVoxels(registry, *voxelGrid, x_min, x_max,
//     y_min, y_max, z_min, z_max, pos, voxelGridView);

//     // Step 6: Get entities within the perception area
//     std::vector<int> entitiesIds = voxelGrid->getAllEntityIdsInRegion(
//         x_min, y_min, z_min, x_max, y_max, z_max, voxelGridView);

//     response.world_view.voxelGridView = voxelGridView;

//     // Step 7: Combine terrain and entity IDs
//     std::vector<int> combinedIds = combineEntityIds(terrainsIds, entitiesIds);

//     // Step 8: Create EntityInterfaces for entities in view
//     createEntityInterfacesForEntitiesInView(registry, combinedIds, response.world_view.entities);

//     // Step 9: Serialize the response
//     std::vector<char> serialized_data = response.serializeFlatBuffer();
//     return nb::bytes(serialized_data.data(), serialized_data.size());
// }

void addTimeSeriesDataToResponse(std::shared_ptr<MapOfMapsOfDoubleResponse> response,
                                 const std::string& seriesName, uint64_t start, uint64_t end,
                                 GameDBHandler* dbHandler) {
    // Query the time series data
    std::vector<std::pair<uint64_t, double>> result =
        dbHandler->queryTimeSeries(seriesName, start, end);

    // Logger::getLogger()->debug("[World::processOptionalQueries] Querying {} data from {} to {}",
    //                           seriesName, start, end);

    // Create a nested map for the time series data
    std::map<std::string, double> timeSeriesMap;

    // Fill the inner map with timestamp -> value pairs
    for (const auto& pair : result) {
        std::string timestampKey = std::to_string(static_cast<double>(pair.first));
        timeSeriesMap[timestampKey] = pair.second;
    }

    // Add to the response
    response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

void World::processOptionalQueries(const std::vector<QueryCommand>& commands,
                                   PerceptionResponse& response) {
    for (const auto& cmd : commands) {
        // Process the command type
        if (cmd.type == "query_entities_data") {
            // Make sure entity_type_id is present
            auto it = cmd.params.find("entity_type_id");
            if (it != cmd.params.end()) {
                int entity_type_id = std::stoi(it->second);

                auto mapOfMapsResponse = std::make_shared<MapOfMapsResponse>();

                // Example: we iterate over some ECS registry
                auto view = registry.view<MetabolismComponent, DigestionComponent, HealthComponent,
                                          EntityTypeComponent>();

                for (auto entity : view) {
                    if (registry.all_of<MetabolismComponent, DigestionComponent, HealthComponent,
                                        EntityTypeComponent>(entity)) {
                        auto& healthComp = view.get<HealthComponent>(entity);
                        int entityId = static_cast<int>(entity);

                        auto entityIdString = std::to_string(entityId);
                        auto entityHealthLevelString = std::to_string(healthComp.healthLevel);

                        // Fill in your mapOfMaps data
                        mapOfMapsResponse->mapOfMaps[entityIdString] = {
                            {"ID", entityIdString},
                            {"Name", "Squirrel"},
                            {"Health", entityHealthLevelString}};
                    }
                }

                response.queryResponses.emplace(1, mapOfMapsResponse);
                std::cout << "Processing 'query_entities_data' with entity_type_id: "
                          << entity_type_id << std::endl;
            } else {
                std::cerr << "Error: 'query_entities_data' missing 'entity_type_id' parameter.\n";
            }
        } else if (cmd.type == "move") {
            // Example handling for 'move' command
            int x = 0, y = 0;
            auto itx = cmd.params.find("x");
            if (itx != cmd.params.end()) {
                x = std::stoi(itx->second);
            }
            auto ity = cmd.params.find("y");
            if (ity != cmd.params.end()) {
                y = std::stoi(ity->second);
            }

            std::cout << "Processing 'move' command to position (" << x << ", " << y << ")\n";
            // Perform the move operation as needed
        } else if (cmd.type == "get_ai_statistics") {
            // Example handling for 'get_ai_statistics' command
            // Logger::getLogger()->debug(
            //     "[World::processOptionalQueries] Processing 'get_ai_statistics' command");
            auto it = cmd.params.find("start");
            long long start = 0;
            if (it != cmd.params.end()) {
                start = std::stoll(it->second);
            }
            auto it2 = cmd.params.find("end");
            long long end = 0;
            if (it2 != cmd.params.end()) {
                end = std::stoll(it2->second);
            }

            auto mapOfMapsOfDoubleResponse = std::make_shared<MapOfMapsOfDoubleResponse>();

            std::vector<std::string> seriesNames = {"population_size",   "inference_queue_size",
                                                    "action_queue_size", "population_mean",
                                                    "population_max",    "population_min"};
            for (const auto& seriesName : seriesNames) {
                addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start, end,
                                            dbHandler.get());
            }

            response.queryResponses.emplace(2, mapOfMapsOfDoubleResponse);

            // Logger::getLogger()->debug(
            //     "[World::processOptionalQueries] Processing 'get_ai_statistics' command");
        } else {
            // Handle unknown command types
            std::cerr << "Error: Unknown command type '" << cmd.type << "'." << std::endl;
        }
    }
}

std::vector<char> World::createPerceptionResponseC(int entityId,
                                                   const std::vector<QueryCommand>& commands) {
    auto logger = Logger::getLogger();
    // logger->info("createPerceptionResponse: start entity={} cmds={} ticks={}", entityId,
    //              commands.size(), gameClock.getTicks());

    // Acquire shared lock to prevent entity destruction during perception creation
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    // Protect registry access against concurrent destruction/updates
    std::lock_guard<std::mutex> lock(registryMutex);

    entt::entity entity = static_cast<entt::entity>(entityId);
    // Ensure the entity is valid
    if (!registry.valid(entity)) {
        throw std::runtime_error("Invalid entity ID: " + std::to_string(entityId));
    }

    PerceptionResponse response{};
    response.ticks = gameClock.getTicks();
    // WorldView world_view;  // Create an instance of WorldView

    // Retrieve the entity's position and perception component once
    // auto view = registry.view<Position, PerceptionComponent>();
    auto allView = registry.view<Position, EntityTypeComponent, PerceptionComponent>();
    if (!allView.contains(entity)) {
        throw std::runtime_error("Entity does not have Position or PerceptionComponent");
    }

    const Position& pos = allView.get<Position>(entity);
    const PerceptionComponent& perception = allView.get<PerceptionComponent>(entity);
    const EntityTypeComponent& etc = allView.get<EntityTypeComponent>(entity);
    // std::cout << "perception: entity pos=(" << pos.x << ", " << pos.y << ", " << pos.z
    //           << ") area_xy=" << perception.getPerceptionArea()
    //           << " area_z=" << perception.getZPerceptionArea() << "\n";
    // logger->info("perception: entity pos=({}, {}, {}) area_xy={} area_z={}", pos.x, pos.y, pos.z,
    //               perception.getPerceptionArea(), perception.getZPerceptionArea());

    // Get the bounds of the perception area (horizontal and vertical)
    // int x_min = pos.x - perception.getPerceptionArea();
    // int x_max = pos.x + perception.getPerceptionArea();
    // int y_min = pos.y - perception.getPerceptionArea();
    // int y_max = pos.y + perception.getPerceptionArea();
    // int z_min = pos.z - perception.getZPerceptionArea();
    // int z_max = pos.z + perception.getZPerceptionArea();
    auto [x_min, x_max, y_min, y_max, z_min, z_max] = computePerceptionArea(pos, perception);

    // Compute dimensions based on perception area
    int view_width = x_max - x_min + 1;
    int view_height = y_max - y_min + 1;
    int view_depth = z_max - z_min + 1;

    if (view_width <= 0 || view_height <= 0 || view_depth <= 0) {
        throw std::runtime_error("Invalid dimensions for VoxelGridView");
    }

    // Initialize the voxel grid with dimensions and offsets
    response.world_view.width = width;
    response.world_view.height = height;
    response.world_view.depth = depth;

    EntityInterface entity_interface = createEntityInterface(registry, entity);

    auto itemEnumView = registry.view<ItemTypeComponent>();
    if (registry.all_of<Inventory>(entity)) {
        const Inventory& inventory = registry.get<Inventory>(entity);
        for (int itemID : inventory.itemIDs) {
            entt::entity itemEntity = static_cast<entt::entity>(itemID);

            if (itemEnumView.contains(itemEntity)) {
                EntityInterface item_entity_interface;
                item_entity_interface.entityId = itemID;

                const ItemTypeComponent& itemTypeC =
                    itemEnumView.get<ItemTypeComponent>(itemEntity);
                item_entity_interface.setComponent<ItemTypeComponent>(itemTypeC);

                auto foodItemView = registry.view<FoodItem>();
                if (itemTypeC.mainType == static_cast<int>(ItemEnum::FOOD) &&
                    foodItemView.contains(itemEntity)) {
                    const FoodItem& foodItem = foodItemView.get<FoodItem>(itemEntity);
                    // item_entity_interface.setFoodItem(foodItem);
                    item_entity_interface.setComponent<FoodItem>(foodItem);
                }

                response.itemsEntities.emplace(item_entity_interface.entityId,
                                               std::move(item_entity_interface));
            }
        }
    }
    if (registry.all_of<ConsoleLogsComponent>(entity)) {
        const ConsoleLogsComponent& consoleLogs = registry.get<ConsoleLogsComponent>(entity);
        // entity_interface.setConsoleLogsComponent(consoleLogs);
        entity_interface.setComponent<ConsoleLogsComponent>(consoleLogs);
        // TODO: Do the flushing here:
    }
    if (registry.all_of<ParentsComponent>(entity)) {
        const ParentsComponent& consoleLogs = registry.get<ParentsComponent>(entity);
        entity_interface.setComponent<ParentsComponent>(consoleLogs);
    }
    response.entity = entity_interface;

    VoxelGridView voxelGridView = VoxelGridView();

    int terrainVirtualIdCounter = -1000;
    std::unordered_map<int, EntityInterface> terrainEntities;
    voxelGridView.initVoxelGridView(view_width, view_height, view_depth, x_min, y_min, z_min);

    auto entityTypeView = registry.view<EntityTypeComponent>();

    std::vector<int> terrainsIds;

    // Use efficient region-based terrain query instead of individual coordinate lookups
    std::vector<VoxelGridCoordinates> terrainCoords =
        voxelGrid->getAllTerrainInRegion(x_min, y_min, z_min, x_max, y_max, z_max);

    // Reserve space for efficiency
    terrainsIds.reserve(terrainCoords.size());

    // Process each terrain voxel found in the region
    for (const auto& coord : terrainCoords) {
        int x = coord.x;
        int y = coord.y;
        int z = coord.z;

        // Get terrain ID at this coordinate
        int terrainId = voxelGrid->getTerrain(x, y, z);
        if (terrainId !=
            static_cast<int>(TerrainIdTypeEnum::NONE)) {  // Assuming -2 means no terrain

            // std::cout << "createPerceptionResponse -> Inside terrain check. terrainId="
            //           << terrainId << "\n";

            // Use terrain ID directly
            int entity_id = terrainId;

            bool isCurrentTerrainOccluded = false;
            int neighborTerrainId = voxelGrid->getTerrain(x + 1, y + 1, z + 1);
            if (neighborTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
                // Use neighbor terrain ID directly
                int neighboorEntityId = neighborTerrainId;

                bool hasValidNeighbor;
                bool isMainTypeTerrain;
                bool isSubTypeOccluding;

                if (neighboorEntityId != -2 && neighboorEntityId != 0) {
                    // entt::entity neighboorTerrainEntity =
                    //     static_cast<entt::entity>(neighboorEntityId);

                    // if (entityTypeView.contains(neighboorTerrainEntity)) {
                    //     std::ostringstream oss;
                    //     oss << "createPerceptionResponse: Entity (ID: " << neighboorEntityId
                    //         << ") contains EntityTypeComponent in EnTT (migration error) while "
                    //            "retrieving terrains in view.";
                    //     throw std::runtime_error(oss.str());
                    // }
                    // const EntityTypeComponent& terrainEtc =
                    //     entityTypeView.get<EntityTypeComponent>(neighboorTerrainEntity);
                    EntityTypeComponent terrainEtc =
                        voxelGrid->getTerrainEntityTypeComponent(x + 1, y + 1, z + 1);

                    hasValidNeighbor =
                        (neighboorEntityId != static_cast<int>(TerrainIdTypeEnum::NONE));
                    isMainTypeTerrain = (terrainEtc.mainType == 0);
                    isSubTypeOccluding =
                        terrainEtc.subType0 != static_cast<int>(TerrainEnum::EMPTY) &&
                        terrainEtc.subType0 != static_cast<int>(TerrainEnum::WATER) &&
                        (terrainEtc.subType1 == 0 || terrainEtc.subType1 == 1);

                } else {
                    hasValidNeighbor = false;
                    isMainTypeTerrain = false;
                    isSubTypeOccluding = false;
                }

                // Correctly check neighbor occlusion state within the view
                bool isNeighboorOccluded = voxelGridView.getTerrainVoxel(x + 1, y + 1, z + 1) == -3;
                // First, check if the terrain is exactly one level below the player
                bool isOneLevelBelow = (z == pos.z - 1);

                // Then, check if the terrain position is directly below or adjacent in
                // a cross pattern
                bool isAdjacentInCross = (x == pos.x && y == pos.y) ||      // Directly below
                                         (x == pos.x + 1 && y == pos.y) ||  // +X direction
                                         (x == pos.x - 1 && y == pos.y) ||  // -X direction
                                         (x == pos.x && y == pos.y + 1) ||  // +Y direction
                                         (x == pos.x && y == pos.y - 1);    // -Y direction

                // Combine the two conditions with isMainTypeTerrain
                bool isTerrainNearPlayer =
                    isMainTypeTerrain && isOneLevelBelow && isAdjacentInCross;

                // This is necessary because we might use transparency during rendering
                // to show player bellow terrain
                if (isTerrainNearPlayer) {
                    isCurrentTerrainOccluded = false;
                } else if (hasValidNeighbor &&
                           ((isMainTypeTerrain && isSubTypeOccluding) || isNeighboorOccluded)) {
                    // std::cout << "perception: occlusion true due to neighbor at (" << (x + 1)
                    //           << ", " << (y + 1) << ", " << (z + 1) << ") id="
                    //           << neighboorEntityId << " isNeighboorOccluded=" <<
                    //           isNeighboorOccluded
                    //           << " isMainTypeTerrain=" << isMainTypeTerrain
                    //           << " isSubTypeOccluding=" << isSubTypeOccluding
                    //           << " hasValidNeighbor=" << hasValidNeighbor << "\n";
                    isCurrentTerrainOccluded = true;
                }
                // std::cout << "perception: occlusion check at (" << x << ", " << y << ", " << z
                //           << ") id=" << entity_id << " neighbor_id=" << neighboorEntityId
                //           << " hasNeighbor=" << hasValidNeighbor
                //           << " mainTerrain=" << isMainTypeTerrain
                //           << " subOccluding=" << isSubTypeOccluding
                //           << " neighOccluded=" << isNeighboorOccluded
                //           << " nearPlayer=" << isTerrainNearPlayer
                //           << " -> occluded=" << isCurrentTerrainOccluded << "\n";
                // logger->info(
                //     "perception: occlusion check at ({}, {}, {}) id={} neighbor_id={} "
                //     "hasNeighbor={} mainTerrain={} subOccluding={} neighOccluded={} nearPlayer={}
                //     "
                //     "-> occluded={}",
                //     x, y, z, entity_id, neighboorEntityId, hasValidNeighbor, isMainTypeTerrain,
                //     isSubTypeOccluding, isNeighboorOccluded, isTerrainNearPlayer,
                //     isCurrentTerrainOccluded);
            }

            if (isCurrentTerrainOccluded) {
                voxelGridView.setTerrainVoxel(x, y, z, -3);
            } else {
                int virtualTerrainId;
                if (terrainId == -1) {
                    virtualTerrainId = terrainVirtualIdCounter--;
                } else {
                    virtualTerrainId = terrainId;
                }

                EntityInterface entity_interface;
                entity_interface.entityId = virtualTerrainId;

                EntityTypeComponent terrainEtc = voxelGrid->getTerrainEntityTypeComponent(x, y, z);
                Position pos = voxelGrid->terrainGridRepository->getPosition(x, y, z);
                MatterContainer matterContainer =
                    voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

                entity_interface.setComponent<EntityTypeComponent>(terrainEtc);
                entity_interface.setComponent<Position>(pos);
                entity_interface.setComponent<MatterContainer>(matterContainer);

                voxelGridView.setTerrainVoxel(x, y, z, virtualTerrainId);
                // std::cout << "perception: visible terrain (" << x << ", " << y << ", " << z
                //           << ") terr_id=" << terrainId << " virt_id=" << virtualTerrainId <<
                //           "\n";
                // logger->info("perception: visible terrain ({}, {}, {}) terr_id={} virt_id={}", x,
                //               y, z, terrainId, virtualTerrainId);
                // response.world_view.entities.emplace(entity_interface.entityId,
                //                                      std::move(entity_interface));

                // terrainsIds.emplace_back(virtualTerrainId);

                terrainEntities[virtualTerrainId] = std::move(entity_interface);
            }
        }
    }

    std::vector<int> entitiesIds =
        voxelGrid->getAllEntityIdsInRegion(x_min, y_min, z_min, x_max, y_max, z_max, voxelGridView);

    response.world_view.voxelGridView = voxelGridView;

    // Create a new vector to hold the merged results
    std::vector<int> combinedIds = terrainsIds;

    // for (const auto& terrainId : terrainsIds) {
    //     if (terrainId != -1 && terrainId != -2 && terrainId != -3) {
    //         combinedIds.push_back(terrainId);
    //     } else if (terrainId == -1) {
    //         EntityTypeComponent etc = voxelGrid->getTerrainEntityTypeComponentById(terrainId);
    //     }
    // }

    // Use std::vector::insert to append entitiesIds to combinedIds
    combinedIds.insert(combinedIds.end(), entitiesIds.begin(), entitiesIds.end());

    // std::vector<EntityInterface> entityInterfaces;
    std::vector<entt::entity> entitiesInView;

#pragma omp parallel for
    for (const auto& entityId : entitiesIds) {
        if (entityId != -1 && entityId != -2 && entityId != -3) {
            entitiesInView.push_back(static_cast<entt::entity>(entityId));
        }
    }

    // Create the views
    auto velocityView = registry.view<Velocity>();
    auto movingComponentView = registry.view<MovingComponent>();
    auto healthView = registry.view<HealthComponent>();
    auto inventoryView = registry.view<Inventory>();
    auto matterContainerView = registry.view<MatterContainer>();
    auto tileEffectsView = registry.view<TileEffectsList>();
    auto tileEffectCompView = registry.view<TileEffectComponent>();

    // Preallocate space if possible
    response.world_view.entities.reserve(entitiesInView.size() + terrainEntities.size());

    for (const auto& [id, entity] : terrainEntities) {
        response.world_view.entities.emplace(id, std::move(entity));
    }

    // openvdb::Int32Grid::ConstAccessor accessor = voxelGrid->terrainGrid->getConstAccessor();

    // std::cout << "createPerceptionResponse -> before entities for_each loop.\n";

    std::for_each(entitiesInView.begin(), entitiesInView.end(), [&](entt::entity entity) {
        // Assuming entitiesInView comes from allView, so no need for validity
        // checks
        if (!registry.valid(entity)) {
            return;
        }

        EntityInterface entity_interface;
        entity_interface.entityId = static_cast<int>(entity);

        const Position& pos = allView.get<Position>(entity);
        const EntityTypeComponent& etc = allView.get<EntityTypeComponent>(entity);

        // if (!isCurrentTerrainOccluded) {
        entity_interface.setComponent<Position>(pos);
        entity_interface.setComponent<EntityTypeComponent>(etc);

        if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) &&
            velocityView.contains(entity)) {
            const Velocity& velocity = velocityView.get<Velocity>(entity);
            entity_interface.setComponent<Velocity>(velocity);
        }

        if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) &&
            movingComponentView.contains(entity)) {
            const MovingComponent& movingComponent =
                movingComponentView.get<MovingComponent>(entity);
            entity_interface.setComponent<MovingComponent>(movingComponent);
        }

        if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) && healthView.contains(entity)) {
            const HealthComponent& health = healthView.get<HealthComponent>(entity);
            entity_interface.setComponent<HealthComponent>(health);
        }

        // Inventory for plants to show their fruits
        if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) &&
            etc.mainType == static_cast<int>(EntityEnum::PLANT) && inventoryView.contains(entity)) {
            const Inventory& inventory = inventoryView.get<Inventory>(entity);
            entity_interface.setComponent<Inventory>(inventory);
        }

        // if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        //     inventoryView.contains(entity)) {
        //     const Inventory& inventory = inventoryView.get<Inventory>(entity);
        //     entity_interface.setComponent<Inventory>(inventory);

        //     for (auto itemEntityID : inventory.itemIDs) {
        //         entt::entity itemEntity = static_cast<entt::entity>(itemEntityID);

        //         EntityInterface item_entity_interface;
        //         item_entity_interface.entityId = itemEntityID;

        //         if (itemEnumView.contains(itemEntity)) {
        //             const ItemTypeComponent& itemTypeC =
        //                 itemEnumView.get<ItemTypeComponent>(itemEntity);
        //             item_entity_interface.setComponent<ItemTypeComponent>(itemTypeC);
        //         }

        //         response.world_view.entities.emplace(item_entity_interface.entityId,
        //                                              std::move(item_entity_interface));
        //     }
        // }

        // if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        //     matterContainerView.contains(entity)) {
        //     const MatterContainer& matterContainer =
        //         matterContainerView.get<MatterContainer>(entity);
        //     entity_interface.setComponent<MatterContainer>(matterContainer);
        // }

        // if (etc.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        //     tileEffectsView.contains(entity)) {
        //     const TileEffectsList& tileEffectList = tileEffectsView.get<TileEffectsList>(entity);
        //     entity_interface.setComponent<TileEffectsList>(tileEffectList);

        //     for (auto tileEffectEntityID : tileEffectList.tileEffectsIDs) {
        //         entt::entity tileEffectEntity = static_cast<entt::entity>(tileEffectEntityID);

        //         EntityInterface tile_effect_entt_interface;
        //         tile_effect_entt_interface.entityId = tileEffectEntityID;

        //         if (entityTypeView.contains(tileEffectEntity)) {
        //             const EntityTypeComponent& etc =
        //                 entityTypeView.get<EntityTypeComponent>(tileEffectEntity);
        //             tile_effect_entt_interface.setComponent<EntityTypeComponent>(etc);
        //         }

        //         if (tileEffectCompView.contains(tileEffectEntity)) {
        //             const TileEffectComponent& tec =
        //                 tileEffectCompView.get<TileEffectComponent>(tileEffectEntity);
        //             tile_effect_entt_interface.setComponent<TileEffectComponent>(tec);
        //         }

        //         response.world_view.entities.emplace(tile_effect_entt_interface.entityId,
        //                                              std::move(tile_effect_entt_interface));
        //     }
        // }

        // Use emplace for efficiency
        response.world_view.entities.emplace(entity_interface.entityId,
                                             std::move(entity_interface));
    });

    processOptionalQueries(commands, response);

    std::vector<char> serialized_data = response.serializeFlatBuffer();
    return serialized_data;
}

nb::bytes World::createPerceptionResponse(int entityId, nb::list optionalQueries) {
    std::vector<QueryCommand> commands = toCommandList(optionalQueries);
    std::vector<char> serialized_data = createPerceptionResponseC(entityId, commands);
    return nb::bytes(serialized_data.data(), serialized_data.size());
}
