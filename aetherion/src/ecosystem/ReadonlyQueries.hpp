#ifndef ECOSYSTEM_READONLY_QUERIES_HPP
#define ECOSYSTEM_READONLY_QUERIES_HPP

#include <entt/entt.hpp>
#include <stdexcept>
#include <tuple>

#include "components/EntityTypeComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "physics/PhysicalMath.hpp"
#include "physics/ReadonlyQueries.hpp"
#include "voxelgrid/VoxelGrid.hpp"
#include "physics/PhysicsEvents.hpp"



inline std::tuple<bool, bool> isNeighborWaterOrEmpty(entt::registry& registry, VoxelGrid& voxelGrid,
                                              const int x, const int y, const int z) {
    int terrainNeighborId = voxelGrid.getTerrain(x, y, z);
    bool isNeighborEmpty = (terrainNeighborId == static_cast<int>(TerrainIdTypeEnum::NONE));
    bool isTerrainNeighborSoftEmpty{false};
    bool isNeighborWater = false;
    // TODO: Uncomment and handle this properly when active EnTT terrains start to be used.
    if (terrainNeighborId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // auto terrainNeighbor = static_cast<entt::entity>(terrainNeighborId);
        isTerrainNeighborSoftEmpty =
            getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainNeighborId, x, y, z);
        EntityTypeComponent typeNeighbor =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
        MatterContainer matterContainerNeighbor =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
        isNeighborWater = (typeNeighbor.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                           typeNeighbor.subType0 == static_cast<int>(TerrainEnum::WATER));
    }
    isNeighborEmpty = isNeighborEmpty || isTerrainNeighborSoftEmpty;
    return std::make_tuple(isNeighborEmpty, isNeighborWater);
}



inline bool isTerrainVoxelEmptyOrSoftEmpty(entt::registry& registry, VoxelGrid& voxelGrid,
                                    entt::dispatcher& dispatcher, const int x, const int y,
                                    const int z) {
    int terrainId = voxelGrid.getTerrain(x, y, z);
    if (terrainId < static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // Invalid terrain ID
        std::ostringstream ossMessage;
        ossMessage << "[isTerrainVoxelEmptyOrSoftEmpty] Error: Invalid terrain ID " << terrainId
                   << " at (" << x << ", " << y << ", " << z << ")";
        spdlog::get("console")->error(ossMessage.str());
        throw std::runtime_error(ossMessage.str());
        dispatcher.trigger<InvalidTerrainFoundEvent>(InvalidTerrainFoundEvent{x, y, z});
        // Trigger deletion at physics engine layer. Block will not be empty immediately.
        return false;
    } else if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        // This should not happen; means vapor entity is missing in voxel grid
        // std::ostringstream ossMessage;
        // ossMessage << "[isTerrainVoxelEmptyOrSoftEmpty] Error: Vapor entity in ON_GRID_STORAGE at
        // ("
        //             << x << ", " << y << ", " << z << ")\n";
        // spdlog::get("console")->error(ossMessage.str());
        return false;
    } else if (terrainId > 0) {
        // Voxel is completely empty
        auto terrain = static_cast<entt::entity>(terrainId);
        EntityTypeComponent* type = registry.try_get<EntityTypeComponent>(terrain);
        const bool isVoxelEmpty{terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)};
        const bool isSoftEmpty{(type && isTerrainSoftEmpty(*type))};
        const bool isEmpty{isVoxelEmpty || isSoftEmpty};
        return isEmpty;
    }

    // Review this after fixing the current bug.
    return false;
}


#endif  // ECOSYSTEM_READONLY_QUERIES_HPP
