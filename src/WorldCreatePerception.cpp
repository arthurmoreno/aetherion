#include "Logger.hpp"
#include "World.hpp"
#include "WorldClientAPI/ProcessOptionalQueries.hpp"

// Compute perception area boundaries
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


void World::processOptionalQueries(const std::vector<QueryCommand>& commands,
                                   PerceptionResponse& response) {
    _processOptionalQueries(commands, response, registry, dbHandler.get());
}

// ---------------------------------------------------------------------------
// Helper: populate response.itemsEntities from the observer entity's inventory
// ---------------------------------------------------------------------------
void World::buildInventoryItems(entt::entity entity, PerceptionResponse& response) {
    auto itemEnumView = registry.view<ItemTypeComponent>();
    if (!registry.all_of<Inventory>(entity)) {
        return;
    }

    const Inventory& inventory = registry.get<Inventory>(entity);
    for (int itemID : inventory.itemIDs) {
        entt::entity itemEntity = static_cast<entt::entity>(itemID);

        if (itemEnumView.contains(itemEntity)) {
            EntityInterface item_entity_interface;
            item_entity_interface.entityId = itemID;

            const ItemTypeComponent& itemTypeC = itemEnumView.get<ItemTypeComponent>(itemEntity);
            item_entity_interface.setComponent<ItemTypeComponent>(itemTypeC);

            auto foodItemView = registry.view<FoodItem>();
            if (itemTypeC.mainType == static_cast<int>(ItemEnum::FOOD) &&
                foodItemView.contains(itemEntity)) {
                const FoodItem& foodItem = foodItemView.get<FoodItem>(itemEntity);
                item_entity_interface.setComponent<FoodItem>(foodItem);
            }

            response.itemsEntities.emplace(item_entity_interface.entityId,
                                           std::move(item_entity_interface));
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: build the EntityInterface for the observer entity (self-view)
// ---------------------------------------------------------------------------
EntityInterface World::buildEntityInterface(entt::entity entity) {
    EntityInterface entity_interface = createEntityInterface(registry, entity);

    if (registry.all_of<ConsoleLogsComponent>(entity)) {
        const ConsoleLogsComponent& consoleLogs = registry.get<ConsoleLogsComponent>(entity);
        entity_interface.setComponent<ConsoleLogsComponent>(consoleLogs);
        // TODO: Do the flushing here
    }
    if (registry.all_of<ParentsComponent>(entity)) {
        const ParentsComponent& parents = registry.get<ParentsComponent>(entity);
        entity_interface.setComponent<ParentsComponent>(parents);
    }

    return entity_interface;
}

// ---------------------------------------------------------------------------
// Helper: build the EntityInterface for a single visible terrain voxel
// ---------------------------------------------------------------------------
static EntityInterface buildTerrainEntityInterface(VoxelGrid* voxelGrid, int x, int y, int z,
                                                   int virtualTerrainId) {
    EntityInterface entity_interface;
    entity_interface.entityId = virtualTerrainId;

    EntityTypeComponent terrainEtc = voxelGrid->getTerrainEntityTypeComponent(x, y, z);
    Position pos = voxelGrid->terrainGridRepository->getPosition(x, y, z);
    MatterContainer matterContainer =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

    entity_interface.setComponent<EntityTypeComponent>(terrainEtc);
    entity_interface.setComponent<Position>(pos);
    entity_interface.setComponent<MatterContainer>(matterContainer);

    return entity_interface;
}

// ---------------------------------------------------------------------------
// Helper: query terrain in region, run occlusion logic, fill voxelGridView and
//         terrainEntities map
// ---------------------------------------------------------------------------
void World::buildTerrainView(int x_min, int y_min, int z_min, int x_max, int y_max, int z_max,
                             VoxelGridView& voxelGridView,
                             std::unordered_map<int, EntityInterface>& terrainEntities,
                             const Position& observerPos) {
    int terrainVirtualIdCounter = -1000;

    std::vector<VoxelGridCoordinates> terrainCoords =
        voxelGrid->getAllTerrainInRegion(x_min, y_min, z_min, x_max, y_max, z_max);

    for (const auto& coord : terrainCoords) {
        const int x = coord.x;
        const int y = coord.y;
        const int z = coord.z;

        int terrainId = voxelGrid->getTerrain(x, y, z);
        if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
            continue;
        }

        bool isCurrentTerrainOccluded = false;
        int neighborTerrainId = voxelGrid->getTerrain(x + 1, y + 1, z + 1);

        if (neighborTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
            int neighboorEntityId = neighborTerrainId;

            bool hasValidNeighbor;
            bool isMainTypeTerrain;
            bool isSubTypeOccluding;

            if (neighboorEntityId != -2 && neighboorEntityId != 0) {
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

            bool isNeighboorOccluded = voxelGridView.getTerrainVoxel(x + 1, y + 1, z + 1) == -3;

            bool isOneLevelBelow = (z == observerPos.z - 1);
            bool isAdjacentInCross =
                (x == observerPos.x && y == observerPos.y) ||
                (x == observerPos.x + 1 && y == observerPos.y) ||
                (x == observerPos.x - 1 && y == observerPos.y) ||
                (x == observerPos.x && y == observerPos.y + 1) ||
                (x == observerPos.x && y == observerPos.y - 1);

            bool isTerrainNearPlayer = isMainTypeTerrain && isOneLevelBelow && isAdjacentInCross;

            if (isTerrainNearPlayer) {
                isCurrentTerrainOccluded = false;
            } else if (hasValidNeighbor &&
                       ((isMainTypeTerrain && isSubTypeOccluding) || isNeighboorOccluded)) {
                isCurrentTerrainOccluded = true;
            }
        }

        if (isCurrentTerrainOccluded) {
            voxelGridView.setTerrainVoxel(x, y, z, -3);
        } else {
            int virtualTerrainId = (terrainId == -1) ? terrainVirtualIdCounter-- : terrainId;
            voxelGridView.setTerrainVoxel(x, y, z, virtualTerrainId);
            terrainEntities[virtualTerrainId] = buildTerrainEntityInterface(voxelGrid, x, y, z, virtualTerrainId);
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: build the EntityInterface for a single non-terrain entity
// ---------------------------------------------------------------------------
static EntityInterface buildNonTerrainEntityInterface(
    entt::entity entity,
    entt::view<entt::get_t<Position, EntityTypeComponent, PerceptionComponent>> allView,
    entt::view<entt::get_t<Velocity>> velocityView,
    entt::view<entt::get_t<MovingComponent>> movingComponentView,
    entt::view<entt::get_t<HealthComponent>> healthView,
    entt::view<entt::get_t<Inventory>> inventoryView) {
    EntityInterface entity_interface;
    entity_interface.entityId = static_cast<int>(entity);

    const Position& pos = allView.get<Position>(entity);
    const EntityTypeComponent& etc = allView.get<EntityTypeComponent>(entity);

    entity_interface.setComponent<Position>(pos);
    entity_interface.setComponent<EntityTypeComponent>(etc);

    if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) && velocityView.contains(entity)) {
        const Velocity& velocity = velocityView.get<Velocity>(entity);
        entity_interface.setComponent<Velocity>(velocity);
    }

    if (etc.mainType != static_cast<int>(EntityEnum::TERRAIN) &&
        movingComponentView.contains(entity)) {
        const MovingComponent& movingComponent = movingComponentView.get<MovingComponent>(entity);
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

    return entity_interface;
}

// ---------------------------------------------------------------------------
// Helper: populate response.world_view.entities with non-terrain entities
// ---------------------------------------------------------------------------
void World::buildNonTerrainEntities(
    const std::vector<int>& entitiesIds,
    std::unordered_map<int, EntityInterface>& terrainEntities,
    PerceptionResponse& response,
    entt::view<entt::get_t<Position, EntityTypeComponent, PerceptionComponent>> allView) {
    std::vector<entt::entity> entitiesInView;

#pragma omp parallel for
    for (const auto& entityId : entitiesIds) {
        if (entityId != -1 && entityId != -2 && entityId != -3) {
            entitiesInView.push_back(static_cast<entt::entity>(entityId));
        }
    }

    auto velocityView = registry.view<Velocity>();
    auto movingComponentView = registry.view<MovingComponent>();
    auto healthView = registry.view<HealthComponent>();
    auto inventoryView = registry.view<Inventory>();
    auto tileEffectsView = registry.view<TileEffectsList>();
    auto tileEffectCompView = registry.view<TileEffectComponent>();

    response.world_view.entities.reserve(entitiesInView.size() + terrainEntities.size());

    for (auto& [id, entity] : terrainEntities) {
        response.world_view.entities.emplace(id, std::move(entity));
    }

    std::for_each(entitiesInView.begin(), entitiesInView.end(), [&](entt::entity entity) {
        if (!registry.valid(entity)) {
            spdlog::get("console")->info("[createPerceptionResponse] Invalid entity: {}",
                                         static_cast<int>(entity));
            return;
        }

        EntityInterface entity_interface = buildNonTerrainEntityInterface(
            entity, allView, velocityView, movingComponentView, healthView, inventoryView);
        response.world_view.entities.emplace(entity_interface.entityId,
                                             std::move(entity_interface));
    });
}

// ---------------------------------------------------------------------------
// Orchestrator: acquires locks, validates entity, delegates to helpers
// ---------------------------------------------------------------------------
std::vector<char> World::createPerceptionResponseC(int entityId,
                                                   const std::vector<QueryCommand>& commands) {
    auto logger = Logger::getLogger();

    // Acquire shared lock to prevent entity destruction during perception creation
    std::shared_lock<std::shared_mutex> lifecycleLock(entityLifecycleMutex);

    // Protect registry access against concurrent destruction/updates
    std::lock_guard<std::mutex> lock(registryMutex);

    entt::entity entity = static_cast<entt::entity>(entityId);
    if (!registry.valid(entity)) {
        throw std::runtime_error("Invalid entity ID: " + std::to_string(entityId));
    }

    PerceptionResponse response{};
    response.ticks = gameClock.getTicks();

    auto allView = registry.view<Position, EntityTypeComponent, PerceptionComponent>();
    if (!allView.contains(entity)) {
        throw std::runtime_error("Entity does not have Position or PerceptionComponent");
    }

    const Position& pos = allView.get<Position>(entity);
    const PerceptionComponent& perception = allView.get<PerceptionComponent>(entity);

    auto [x_min, x_max, y_min, y_max, z_min, z_max] = computePerceptionArea(pos, perception);

    const int view_width = x_max - x_min + 1;
    const int view_height = y_max - y_min + 1;
    const int view_depth = z_max - z_min + 1;

    if (view_width <= 0 || view_height <= 0 || view_depth <= 0) {
        throw std::runtime_error("Invalid dimensions for VoxelGridView");
    }

    response.world_view.width = width;
    response.world_view.height = height;
    response.world_view.depth = depth;

    // Build observer entity interface (self-view)
    response.entity = buildEntityInterface(entity);

    // Populate observer's inventory items
    buildInventoryItems(entity, response);

    // Build terrain portion of the view
    VoxelGridView voxelGridView;
    voxelGridView.initVoxelGridView(view_width, view_height, view_depth, x_min, y_min, z_min);

    std::unordered_map<int, EntityInterface> terrainEntities;
    buildTerrainView(x_min, y_min, z_min, x_max, y_max, z_max, voxelGridView, terrainEntities, pos);

    // Collect non-terrain entity IDs visible in the region
    std::vector<int> entitiesIds =
        voxelGrid->getAllEntityIdsInRegion(x_min, y_min, z_min, x_max, y_max, z_max, voxelGridView);

    response.world_view.voxelGridView = voxelGridView;

    // Build non-terrain entities and merge terrain entities into the response
    buildNonTerrainEntities(entitiesIds, terrainEntities, response, allView);

    processOptionalQueries(commands, response);

    return response.serializeFlatBuffer();
}

nb::bytes World::createPerceptionResponse(int entityId, nb::list optionalQueries) {
    std::vector<QueryCommand> commands = toCommandList(optionalQueries);
    std::vector<char> serialized_data = createPerceptionResponseC(entityId, commands);
    return nb::bytes(serialized_data.data(), serialized_data.size());
}

