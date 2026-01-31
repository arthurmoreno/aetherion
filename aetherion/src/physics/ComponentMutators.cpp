#include "physics/ComponentMutators.hpp"
#include "physics/PhysicsMutators.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <memory>
#include <spdlog/spdlog.h>

// Implementations moved from PhysicsMutators.hpp (Direct Component Mutators)

void updateEntityVelocity(Velocity& velocity, float newVx, float newVy, float newVz) {
	velocity.vx = newVx;
	velocity.vy = newVy;
	velocity.vz = newVz;
}

void ensurePositionComponentForTerrain(entt::registry& registry, VoxelGrid& voxelGrid,
									  entt::entity entity, bool isTerrain) {
	if (isTerrain && !registry.all_of<Position>(entity)) {
		if (!voxelGrid.terrainGridRepository) {
			spdlog::warn("ensurePositionComponentForTerrain: no terrainGridRepository available");
			throw std::runtime_error("Missing TerrainGridRepository");
		}
		std::ostringstream error;
		error << "[handleMovement] Terrain entity " << static_cast<int>(entity)
			  << " missing Position component (not fully initialized yet)";

		Position pos = voxelGrid.terrainGridRepository->getPositionOfEntt(entity);
		int entityId = static_cast<int>(entity);
		if (pos.x == -1 && pos.y == -1 && pos.z == -1) {
			std::cout << "[handleMovement] Could not find position of entity " << entityId
					  << " in TerrainGridRepository, skipping entity." << std::endl;
			throw std::runtime_error(error.str());
		}
		registry.emplace<Position>(entity, pos);
	}
}

void convertIntoSoftEmpty(entt::registry& registry, entt::entity& terrain) {
	EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
	bool shouldEmplaceTerrainType{terrainType == nullptr};
	if (terrainType == nullptr) {
		terrainType = new EntityTypeComponent();
	}
	terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
	terrainType->subType0 = static_cast<int>(TerrainEnum::EMPTY);
	terrainType->subType1 = 0;
	if (shouldEmplaceTerrainType) {
		registry.emplace<EntityTypeComponent>(terrain, *terrainType);
	}

	StructuralIntegrityComponent* terrainSI =
		registry.try_get<StructuralIntegrityComponent>(terrain);
	bool shouldEmplaceTerrainSI{terrainSI == nullptr};
	if (terrainSI == nullptr) {
		terrainSI = new StructuralIntegrityComponent();
	}
	terrainSI->canStackEntities = false;
	terrainSI->maxLoadCapacity = -1;
	terrainSI->matterState = MatterState::GAS;
	if (shouldEmplaceTerrainSI) {
		registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
	}
}

void setEmptyWaterComponentsEnTT(entt::registry& registry, entt::entity& terrain,
								 MatterState matterState) {
	EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
	bool shouldEmplaceTerrainType{terrainType == nullptr};
	if (terrainType == nullptr) {
		terrainType = new EntityTypeComponent();
	}
	terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
	terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
	terrainType->subType1 = 0;
	if (shouldEmplaceTerrainType) {
		registry.emplace<EntityTypeComponent>(terrain, *terrainType);
	}

	StructuralIntegrityComponent* terrainSI =
		registry.try_get<StructuralIntegrityComponent>(terrain);
	bool shouldEmplaceTerrainSI{terrainSI == nullptr};
	if (terrainSI == nullptr) {
		terrainSI = new StructuralIntegrityComponent();
	}
	terrainSI->canStackEntities = false;
	terrainSI->maxLoadCapacity = -1;
	terrainSI->matterState = matterState;
	if (shouldEmplaceTerrainSI) {
		registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
	}

	MatterContainer* terrainMC = registry.try_get<MatterContainer>(terrain);
	bool shouldEmplaceTerrainMC{terrainMC == nullptr};
	if (terrainMC == nullptr) {
		terrainMC = new MatterContainer();
	}
	terrainMC->TerrainMatter = 0;
	terrainMC->WaterMatter = 0;
	terrainMC->WaterVapor = 0;
	terrainMC->BioMassMatter = 0;
	if (shouldEmplaceTerrainMC) {
		registry.emplace<MatterContainer>(terrain, *terrainMC);
	}
}
