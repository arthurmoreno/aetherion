
#include "EffectsSystem.hpp"

#include <iostream>

void EffectsSystem::processEffects(entt::registry& registry, VoxelGrid& voxelGrid,
                                   entt::dispatcher& dispatcher) {
    auto tileEffectsListView = registry.view<TileEffectsList>();
    for (auto entity : tileEffectsListView) {
        auto& tileEffectsList = registry.get<TileEffectsList>(entity);

        for (auto tileEffectID_IT = tileEffectsList.tileEffectsIDs.begin();
             tileEffectID_IT != tileEffectsList.tileEffectsIDs.end();) {
            const auto tileEffect = static_cast<entt::entity>(*tileEffectID_IT);
            bool shouldDeleteTileEffect{false};
            auto& tileEffectsComp = registry.get<TileEffectComponent>(tileEffect);

            tileEffectsComp.effectRemainingTime -= 1;
            if (tileEffectsComp.effectRemainingTime <= 0) {
                shouldDeleteTileEffect = true;
                dispatcher.enqueue<KillEntityEvent>(tileEffect);
            }

            if (shouldDeleteTileEffect) {
                tileEffectID_IT = tileEffectsList.tileEffectsIDs.erase(tileEffectID_IT);
            } else {
                ++tileEffectID_IT;
            }
        }

        if (tileEffectsList.tileEffectsIDs.empty()) {
            registry.remove<TileEffectsList>(entity);

            Position pos = registry.get<Position>(entity);
            EntityTypeComponent type =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z);
            MatterContainer matterContainer =
                voxelGrid.terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z);

            bool isEmptyTerrain = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                                  type.subType0 == static_cast<int>(TerrainEnum::EMPTY));

            bool emptyMatter = (matterContainer.TerrainMatter == 0 && matterContainer.WaterMatter == 0 &&
                                matterContainer.WaterVapor == 0 && matterContainer.BioMassMatter == 0);
            if (isEmptyTerrain && emptyMatter) {
                std::cout << "[EffectsSystem] Empty terrain with no matter container at (" << pos.x << ", "
                          << pos.y << ", " << pos.z << ")\n";
                dispatcher.enqueue<KillEntityEvent>(entity);
            }
        }
    }
}
