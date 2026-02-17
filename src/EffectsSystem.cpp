
#include "EffectsSystem.hpp"

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

            EntityTypeComponent* type = registry.try_get<EntityTypeComponent>(entity);
            MatterContainer* matterContainer = registry.try_get<MatterContainer>(entity);
            bool isEmptyTerrain{false};
            if (type) {
                isEmptyTerrain = (type->mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                                  type->subType0 == static_cast<int>(TerrainEnum::EMPTY));
            }

            if (isEmptyTerrain && matterContainer == nullptr) {
                dispatcher.enqueue<KillEntityEvent>(entity);
            }
        }
    }
}
