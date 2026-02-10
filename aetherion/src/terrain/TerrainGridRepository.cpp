#include "terrain/TerrainGridRepository.hpp"

#include <openvdb/openvdb.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <functional>
#include <shared_mutex>

#include "physics/PhysicsExceptions.hpp"
#include "terrain/TerrainGridLock.hpp"

namespace {
inline openvdb::Coord C(int x, int y, int z) { return openvdb::Coord(x, y, z); }
}  // namespace

bool TerrainGridRepository::isTerrainGridLocked() const { return terrainGridLocked_.load(); }

bool TerrainGridRepository::isTerrainIdOnEnttRegistry(int terrainID) const {
    return terrainID != static_cast<int>(TerrainIdTypeEnum::NONE) &&
           terrainID != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE);
}

TerrainGridRepository::TerrainGridRepository(entt::registry& registry, TerrainStorage& storage)
    : registry_(registry), storage_(storage) {
    // Auto-mark active when Velocity or MovingComponent are emplaced
    registry_.on_construct<Velocity>().connect<&TerrainGridRepository::onConstructVelocity>(*this);
    registry_.on_construct<MovingComponent>().connect<&TerrainGridRepository::onConstructMoving>(
        *this);
    // Clean up when Velocity or MovingComponent is destroyed
    registry_.on_destroy<Velocity>().connect<&TerrainGridRepository::onDestroyVelocity>(*this);
    registry_.on_destroy<MovingComponent>().connect<&TerrainGridRepository::onDestroyMoving>(*this);
}

entt::entity TerrainGridRepository::getEntityAt(int x, int y, int z) const {
    std::shared_lock<std::shared_mutex> lock(trackingMapsMutex_);
    auto it = byCoord_.find(VoxelCoord{x, y, z});
    if (it == byCoord_.end()) return entt::null;
    return it->second;
}

Position TerrainGridRepository::getPositionOfEntt(entt::entity terrain_entity) const {
    std::shared_lock<std::shared_mutex> lock(trackingMapsMutex_);
    auto it = byEntity_.find(terrain_entity);
    if (it == byEntity_.end()) {
        // Entity not found in mapping, return invalid position
        // return Position{-1, -1, -1, DirectionEnum::UP};
        throw aetherion::InvalidEntityException(
            "Entity not found in TerrainGridRepository mapping");
    }

    const VoxelCoord& key = it->second;
    return getPosition(key.x, key.y, key.z);
}

void TerrainGridRepository::moveTerrain(MovingComponent& movingComponent) {
    // Hold the terrain grid lock for the entire move operation to ensure atomicity.
    // Inner methods that use withUniqueLock/withSharedLock will see isTerrainGridLocked()==true
    // and skip re-acquiring terrainGridMutex. Methods with takeLock param get false explicitly.
    // TerrainGridLock gridLock(this);

    // Get the current position of the entity
    std::optional<int> currentPositionEntityId =
        getTerrainIdIfExists(movingComponent.movingFromX, movingComponent.movingFromY,
                             movingComponent.movingFromZ, false);
    std::optional<int> movingToPositionEntityId = getTerrainIdIfExists(
        movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ, false);
    if (currentPositionEntityId && !movingToPositionEntityId) {
        int terrainID = currentPositionEntityId.value();

        registry_.remove<Position>(static_cast<entt::entity>(terrainID));
        registry_.remove<Velocity>(static_cast<entt::entity>(terrainID));
        registry_.remove<MovingComponent>(static_cast<entt::entity>(terrainID));

        // CRITICAL: Reserve destination FIRST to prevent race conditions
        // This must happen before copying any data to prevent other entities from trying to move
        // here
        // [COMMENTED OUT FOR TESTING - isolating inconsistent data source]
        setTerrainId(movingComponent.movingToX, movingComponent.movingToY,
                     movingComponent.movingToZ, terrainID, false);
        setTerrainId(movingComponent.movingFromX, movingComponent.movingFromY,
                     movingComponent.movingFromZ, -2, false);  // Clear old position

        // TODO: Clear moving from data.
        // [COMMENTED OUT FOR TESTING - only keeping removal from old position]
        // // Copy Position component
        // setDirection(movingComponent.movingToX, movingComponent.movingToY,
        //              movingComponent.movingToZ, movingComponent.direction);

        // // Copy structural EntityTypeComponent component
        EntityTypeComponent currentEntityType =
            getTerrainEntityType(movingComponent.movingFromX, movingComponent.movingFromY,
                                 movingComponent.movingFromZ, false);

        setTerrainEntityType(
            movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ,
            EntityTypeComponent{currentEntityType.mainType, currentEntityType.subType0,
                                currentEntityType.subType1},
            false);
        // EntityTypeComponent emptyEntityType{static_cast<int>(EntityEnum::TERRAIN),
        // static_cast<int>(TerrainEnum::EMPTY), 0}; setTerrainEntityType(
        //     movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ,
        //     emptyEntityType, false);
        // Copy structural StructuralIntegrityComponent component
        StructuralIntegrityComponent currentSIC =
            getTerrainStructuralIntegrity(movingComponent.movingFromX, movingComponent.movingFromY,
                                          movingComponent.movingFromZ, false);

        setTerrainStructuralIntegrity(movingComponent.movingToX, movingComponent.movingToY,
                                      movingComponent.movingToZ, currentSIC);
        // StructuralIntegrityComponent emptySIC{};
        // emptySIC.canStackEntities = false;
        // emptySIC.maxLoadCapacity = -1;
        // emptySIC.matterState = MatterState::GAS;
        // setTerrainStructuralIntegrity(movingComponent.movingFromX, movingComponent.movingFromY,
        // movingComponent.movingFromZ, emptySIC);

        // // I am here, on the refactor work.
        // // [These ones does not need to takeLock]
        // setCanStackEntities(movingComponent.movingToX, movingComponent.movingToY,
        //                     movingComponent.movingToZ, currentSIC.canStackEntities);
        // setMaxLoadCapacity(movingComponent.movingToX, movingComponent.movingToY,
        //                    movingComponent.movingToZ, currentSIC.maxLoadCapacity);
        // setGradient(movingComponent.movingToX, movingComponent.movingToY,
        // movingComponent.movingToZ,
        //             currentSIC.gradientVector);
        // setMatterState(movingComponent.movingToX, movingComponent.movingToY,
        //                movingComponent.movingToZ, currentSIC.matterState);

        // // Copy structural MatterContainer component
        MatterContainer currentMC = getTerrainMatterContainer(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        setTerrainMatterContainer(movingComponent.movingToX, movingComponent.movingToY,
                                  movingComponent.movingToZ, currentMC);
        // MatterContainer emptyMC{0, 0, 0, 0};
        // setTerrainMatterContainer(movingComponent.movingFromX, movingComponent.movingFromY,
        //                           movingComponent.movingFromZ, emptyMC);

        // Copy structural PhysicsStats component
        PhysicsStats currentPS =
            getPhysicsStats(movingComponent.movingFromX, movingComponent.movingFromY,
                            movingComponent.movingFromZ, false);
        setPhysicsStats(movingComponent.movingToX, movingComponent.movingToY,
                        movingComponent.movingToZ,
                        PhysicsStats{currentPS.mass, currentPS.maxSpeed, currentPS.minSpeed, 0.0f,
                                     0.0f, 0.0f, 0.0f},
                        false);
        // PhysicsStats emptyPS{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        // setPhysicsStats(movingComponent.movingFromX, movingComponent.movingFromY,
        //                 movingComponent.movingFromZ, emptyPS, false);

        int movFromTerrainId = storage_.deleteTerrain(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        Position newPosition{movingComponent.movingToX, movingComponent.movingToY,
                             movingComponent.movingToZ, movingComponent.direction};
        registry_.emplace<Position>(static_cast<entt::entity>(terrainID), newPosition);
        // Update tracking maps to keep byCoord_/byEntity_ in sync with the grid move.
        // Without this, cleanup hooks (onDestroyVelocity) look up the new Position but
        // find stale tracking map entries pointing to the old position, causing zombie entities.
        entt::entity entity = static_cast<entt::entity>(terrainID);
        VoxelCoord oldKey{movingComponent.movingFromX, movingComponent.movingFromY,
                          movingComponent.movingFromZ};
        VoxelCoord newKey{movingComponent.movingToX, movingComponent.movingToY,
                          movingComponent.movingToZ};
        removeFromTrackingMaps(oldKey, entity, true, false);
        // [COMMENTED OUT FOR TESTING]
        addToTrackingMaps(newKey, entity, true, false);
    } else {
        std::string errorMsg =
            "Invalid terrain move from (" + std::to_string(movingComponent.movingFromX) + "," +
            std::to_string(movingComponent.movingFromY) + "," +
            std::to_string(movingComponent.movingFromZ) + ") to (" +
            std::to_string(movingComponent.movingToX) + "," +
            std::to_string(movingComponent.movingToY) + "," +
            std::to_string(movingComponent.movingToZ) + "). Source: " +
            (currentPositionEntityId ? "ID=" + std::to_string(currentPositionEntityId.value())
                                     : "none") +
            ", Dest: " +
            (movingToPositionEntityId ? "ID=" + std::to_string(movingToPositionEntityId.value())
                                      : "none");
        std::cout << errorMsg << "\n";
        throw std::runtime_error(errorMsg);
    }
}

std::optional<int> TerrainGridRepository::getTerrainIdIfExists(int x, int y, int z,
                                                               bool takeLock) const {
    return withSharedLock(
        [&]() -> std::optional<int> {
            int terrainId = storage_.getTerrainIdIfExists(x, y, z);
            if (terrainId == -2) {
                return std::nullopt;
            }
            return terrainId;
        },
        takeLock);
}

bool TerrainGridRepository::checkIfTerrainHasEntity(int x, int y, int z, bool takeLock) const {
    return withSharedLock(
        [&]() {
            std::optional<int> entityId = storage_.getTerrainIdIfExists(x, y, z);
            return entityId.has_value() && entityId.value() >= 0;
        },
        takeLock);
}

void TerrainGridRepository::markActive(int x, int y, int z, entt::entity e, bool takeLock) {
    // Update TerrainStorage activation indicator based on strategy
    withUniqueLock(
        [&]() {
            if (storage_.terrainGrid) {
                storage_.terrainGrid->tree().setValue(C(x, y, z), static_cast<int>(e));
            }
        },
        takeLock);
}

void TerrainGridRepository::clearActive(int x, int y, int z, bool takeLock) {
    withUniqueLock(
        [&]() {
            if (!storage_.terrainGrid) return;
            if (storage_.useActiveMask) {
                storage_.terrainGrid->tree().setValue(C(x, y, z), 0);
            } else {
                storage_.terrainGrid->tree().setValue(C(x, y, z), storage_.bgEntityId);
            }
        },
        takeLock);
}

void TerrainGridRepository::setTerrainId(int x, int y, int z, int terrainID, bool takeLock) {
    withUniqueLock(
        [&]() {
            // std::cout << "[setTerrainId] Setting terrain ID at (" << x << ", " << y << ", " << z
            //           << ") to " << terrainID << std::endl;
            storage_.terrainGrid->tree().setValue(C(x, y, z), terrainID);
        },
        takeLock);
}

void TerrainGridRepository::addToTrackingMaps(const VoxelCoord& key, entt::entity e,
                                              bool takeTrackingLock, bool respectTerrainGridLock) {
    withTrackingMapsLock(
        [&]() {
            byCoord_[key] = e;
            byEntity_[e] = key;
        },
        takeTrackingLock, respectTerrainGridLock);
}

void TerrainGridRepository::removeFromTrackingMaps(const VoxelCoord& key, entt::entity e,
                                                   bool takeTrackingLock,
                                                   bool respectTerrainGridLock) {
    withTrackingMapsLock(
        [&]() {
            byCoord_.erase(key);
            byEntity_.erase(e);
        },
        takeTrackingLock, respectTerrainGridLock);
}

void TerrainGridRepository::onConstructVelocity(entt::registry& reg, entt::entity e) {
    // If entity has a Position, mark active in the TerrainStorage grid
    // if (!reg.valid(e)) return;
    // if (auto pos = reg.try_get<Position>(e)) {
    //     auto entityType = reg.try_get<EntityTypeComponent>(e);
    //     if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
    //         return;
    //     }

    //     withTrackingMapsLock([&]() {
    //         if (checkIfTerrainHasEntity(pos->x, pos->y, pos->z, false)) {
    //             // std::cout << "TerrainGridRepository: onConstructVelocity at (" << pos->x << ",
    //             "
    //             //           << pos->y << ", " << pos->z << ") entity=" << int(e) << "\n";
    //             VoxelCoord key{pos->x, pos->y, pos->z};
    //             addToTrackingMaps(key, e, false, false);
    //             markActive(pos->x, pos->y, pos->z, e, false);
    //         }
    //     }, true, true);
    // }
}

void TerrainGridRepository::onConstructMoving(entt::registry& reg, entt::entity e) {
    // if (!reg.valid(e)) return;
    // if (auto pos = reg.try_get<Position>(e)) {
    //     auto entityType = reg.try_get<EntityTypeComponent>(e);
    //     if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
    //         return;
    //     }
    //     withTrackingMapsLock([&]() {
    //         if (checkIfTerrainHasEntity(pos->x, pos->y, pos->z, false)) {
    //         // std::cout << "TerrainGridRepository: onConstructMoving at (" << pos->x << ", " <<
    //         // pos->y
    //         //           << ", " << pos->z << ") entity=" << int(e) << "\n";

    //             VoxelCoord key{pos->x, pos->y, pos->z};
    //             addToTrackingMaps(key, e, false, false);
    //             markActive(pos->x, pos->y, pos->z, e, false);
    //             }
    //     }, true, true);
    // }
}

void TerrainGridRepository::onDestroyVelocity(entt::registry& reg, entt::entity e) {
    // Clean up entity tracking and restore to static terrain storage
    if (!reg.valid(e)) return;

    auto pos = reg.try_get<Position>(e);
    if (!pos) return;

    auto entityType = reg.try_get<EntityTypeComponent>(e);
    if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
        return;
    }

    // std::cout << "TerrainGridRepository: onDestroyVelocity at (" << pos->x << ", "
    //           << pos->y << ", " << pos->z << ") entity=" << int(e) << "\n";

    withTrackingMapsLock(
        [&]() {
            VoxelCoord key{pos->x, pos->y, pos->z};
            removeFromTrackingMaps(key, e, false, false);
            // Set terrain ID back to ON_GRID_STORAGE (-1) to mark it as static
            setTerrainId(pos->x, pos->y, pos->z,
                         static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE), false);
            clearActive(pos->x, pos->y, pos->z, false);
        },
        true, true);
}

void TerrainGridRepository::onDestroyMoving(entt::registry& reg, entt::entity e) {
    // Clean up MovingComponent-specific state when destroyed
    // Note: This hook fires when MovingComponent is removed, either due to:
    // 1. Movement completion (handleMovingTo removes the component)
    // 2. Entity destruction (all components are destroyed)
    // TerrainGridLock lock(this);

    if (!reg.valid(e)) return;

    auto pos = reg.try_get<Position>(e);
    if (!pos) return;

    auto entityType = reg.try_get<EntityTypeComponent>(e);
    if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
        return;
    }

    // std::cout << "TerrainGridRepository: onDestroyMoving at (" << pos->x << ", "
    //           << pos->y << ", " << pos->z << ") entity=" << int(e) << "\n";

    // Only clean up if this is an entity destruction (not just component removal)
    // Check if entity still has other transient components
    bool hasVelocity = reg.all_of<Velocity>(e);

    // If entity still has Velocity, it's still active - don't deactivate
    // The onDestroyVelocity hook will handle cleanup when Velocity is also removed
    if (hasVelocity) {
        return;
    }

    withTrackingMapsLock(
        [&]() {
            VoxelCoord key{pos->x, pos->y, pos->z};
            removeFromTrackingMaps(key, e, false, false);
            // Set terrain ID back to ON_GRID_STORAGE (-1) to mark it as static
            setTerrainId(pos->x, pos->y, pos->z,
                         static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE), false);
            clearActive(pos->x, pos->y, pos->z, false);
        },
        true, true);
}

entt::entity TerrainGridRepository::ensureActive(int x, int y, int z) {
    // CRITICAL: Lock terrain grid for entire operation to prevent race conditions
    // where multiple threads try to activate the same voxel simultaneously
    // Only acquire lock if not already held by caller
    // std::unique_lock<std::shared_mutex> lock(terrainGridMutex, std::defer_lock);
    // if (!isTerrainGridLocked()) {
    //     lock.lock();
    // }

    // Check if already active (now protected by lock)
    entt::entity existing = getEntityAt(x, y, z);
    if (existing != entt::null && registry_.valid(existing)) {
        return existing;
    } else if (existing != entt::null) {
        // Clean up stale mapping
        VoxelCoord key{x, y, z};
        removeFromTrackingMaps(key, existing);
        registry_.destroy(existing);
    }

    DirectionEnum direction = storage_.getTerrainDirection(x, y, z);

    entt::entity e = registry_.create();
    registry_.emplace<Position>(e, Position{x, y, z, direction});
    registry_.emplace<Velocity>(e, Velocity{0.f, 0.f, 0.f});
    registry_.emplace<MovingComponent>(e, MovingComponent{0});

    VoxelCoord key{x, y, z};

    // CRITICAL: Set terrain ID in grid BEFORE adding to tracking maps
    // This ensures the terrain ID matches when other threads query the voxel
    if (storage_.terrainGrid) {
        storage_.terrainGrid->tree().setValue(C(x, y, z), static_cast<int>(e));
    }

    // Now add to tracking maps after terrain ID is set
    addToTrackingMaps(key, e);

    return e;
}

// ---------------- EntityTypeComponent aggregation ----------------
EntityTypeComponent TerrainGridRepository::getTerrainEntityType(int x, int y, int z,
                                                                bool takeLock) const {
    return withSharedLock(
        [&]() {
            EntityTypeComponent etc{};
            etc.mainType = storage_.getTerrainMainType(x, y, z);
            etc.subType0 = storage_.getTerrainSubType0(x, y, z);
            etc.subType1 = storage_.getTerrainSubType1(x, y, z);
            return etc;
        },
        takeLock);
}

void TerrainGridRepository::setTerrainEntityType(int x, int y, int z, EntityTypeComponent etc,
                                                 bool takeLock) {
    withUniqueLock(
        [&]() {
            storage_.setTerrainMainType(x, y, z, etc.mainType);
            storage_.setTerrainSubType0(x, y, z, etc.subType0);
            storage_.setTerrainSubType1(x, y, z, etc.subType1);
        },
        takeLock);
}

TerrainInfo TerrainGridRepository::readTerrainInfo(int x, int y, int z) const {
    TerrainInfo info;
    info.x = x;
    info.y = y;
    info.z = z;

    // Use single shared lock for all storage reads
    withSharedLock([&]() {
        info.active = storage_.isActive(x, y, z);

        // Populate static data from VDB grids
        StaticData stat;
        stat.mainType = storage_.getTerrainMainType(x, y, z);
        stat.subType0 = storage_.getTerrainSubType0(x, y, z);
        stat.subType1 = storage_.getTerrainSubType1(x, y, z);
        stat.matter.TerrainMatter = storage_.getTerrainMatter(x, y, z);
        stat.matter.WaterMatter = storage_.getTerrainWaterMatter(x, y, z);
        stat.matter.WaterVapor = storage_.getTerrainVaporMatter(x, y, z);
        stat.matter.BioMassMatter = storage_.getTerrainBiomassMatter(x, y, z);
        stat.mass = storage_.getTerrainMass(x, y, z);
        stat.maxSpeed = storage_.getTerrainMaxSpeed(x, y, z);
        stat.minSpeed = storage_.getTerrainMinSpeed(x, y, z);
        stat.direction = storage_.getTerrainDirection(x, y, z);
        stat.canStackEntities = storage_.getTerrainCanStackEntities(x, y, z);
        stat.matterState = storage_.getTerrainMatterState(x, y, z);
        stat.gradient = storage_.getTerrainGradientVector(x, y, z);
        stat.maxLoadCapacity = storage_.getTerrainMaxLoadCapacity(x, y, z);
        info.stat = stat;
    });

    if (info.active) {
        entt::entity e = getEntityAt(x, y, z);
        if (e != entt::null) {
            TransientData tr;
            if (auto v = registry_.try_get<Velocity>(e)) tr.velocity = *v;
            if (auto m = registry_.try_get<MovingComponent>(e)) tr.moving = *m;
            info.transient = tr;
        }
    }
    return info;
}

// ---------------- Static arbitration passthrough ----------------

int TerrainGridRepository::getTerrainMainType(int x, int y, int z) const {
    return storage_.getTerrainMainType(x, y, z);
}
void TerrainGridRepository::setTerrainMainType(int x, int y, int z, int v) {
    storage_.setTerrainMainType(x, y, z, v);
}
int TerrainGridRepository::getTerrainSubType0(int x, int y, int z) const {
    return storage_.getTerrainSubType0(x, y, z);
}
void TerrainGridRepository::setTerrainSubType0(int x, int y, int z, int v) {
    storage_.setTerrainSubType0(x, y, z, v);
}
int TerrainGridRepository::getTerrainSubType1(int x, int y, int z) const {
    return storage_.getTerrainSubType1(x, y, z);
}
void TerrainGridRepository::setTerrainSubType1(int x, int y, int z, int v) {
    storage_.setTerrainSubType1(x, y, z, v);
}

MatterContainer TerrainGridRepository::getTerrainMatterContainer(int x, int y, int z) const {
    return withSharedLock([&]() {
        MatterContainer mc{};
        mc.TerrainMatter = storage_.getTerrainMatter(x, y, z);
        mc.WaterVapor = storage_.getTerrainVaporMatter(x, y, z);
        mc.WaterMatter = storage_.getTerrainWaterMatter(x, y, z);
        mc.BioMassMatter = storage_.getTerrainBiomassMatter(x, y, z);
        return mc;
    });
}

void TerrainGridRepository::setTerrainMatterContainer(int x, int y, int z,
                                                      const MatterContainer& mc) {
    withUniqueLock([&]() {
        storage_.setTerrainMatter(x, y, z, mc.TerrainMatter);
        storage_.setTerrainVaporMatter(x, y, z, mc.WaterVapor);
        storage_.setTerrainWaterMatter(x, y, z, mc.WaterMatter);
        storage_.setTerrainBiomassMatter(x, y, z, mc.BioMassMatter);
    });
}

int TerrainGridRepository::getTerrainMatter(int x, int y, int z) const {
    return storage_.getTerrainMatter(x, y, z);
}
void TerrainGridRepository::setTerrainMatter(int x, int y, int z, int v) {
    storage_.setTerrainMatter(x, y, z, v);
}
int TerrainGridRepository::getWaterMatter(int x, int y, int z) const {
    return storage_.getTerrainWaterMatter(x, y, z);
}
void TerrainGridRepository::setWaterMatter(int x, int y, int z, int v) {
    storage_.setTerrainWaterMatter(x, y, z, v);
}
int TerrainGridRepository::getVaporMatter(int x, int y, int z) const {
    return storage_.getTerrainVaporMatter(x, y, z);
}
void TerrainGridRepository::setVaporMatter(int x, int y, int z, int v) {
    storage_.setTerrainVaporMatter(x, y, z, v);
}
int TerrainGridRepository::getBiomassMatter(int x, int y, int z) const {
    return storage_.getTerrainBiomassMatter(x, y, z);
}
void TerrainGridRepository::setBiomassMatter(int x, int y, int z, int v) {
    storage_.setTerrainBiomassMatter(x, y, z, v);
}

int TerrainGridRepository::getMass(int x, int y, int z) const {
    return storage_.getTerrainMass(x, y, z);
}
void TerrainGridRepository::setMass(int x, int y, int z, int v) {
    storage_.setTerrainMass(x, y, z, v);
}
int TerrainGridRepository::getMaxSpeed(int x, int y, int z) const {
    return storage_.getTerrainMaxSpeed(x, y, z);
}
void TerrainGridRepository::setMaxSpeed(int x, int y, int z, int v) {
    storage_.setTerrainMaxSpeed(x, y, z, v);
}
int TerrainGridRepository::getMinSpeed(int x, int y, int z) const {
    return storage_.getTerrainMinSpeed(x, y, z);
}
void TerrainGridRepository::setMinSpeed(int x, int y, int z, int v) {
    storage_.setTerrainMinSpeed(x, y, z, v);
}

PhysicsStats TerrainGridRepository::getPhysicsStats(int x, int y, int z, bool takeLock) const {
    return withSharedLock(
        [&]() {
            PhysicsStats ps{};
            ps.mass = static_cast<float>(storage_.getTerrainMass(x, y, z));
            ps.maxSpeed = static_cast<float>(storage_.getTerrainMaxSpeed(x, y, z));
            ps.minSpeed = static_cast<float>(storage_.getTerrainMinSpeed(x, y, z));
            ps.heat = static_cast<float>(storage_.getTerrainHeat(x, y, z));
            ps.forceX = 0.0f;
            ps.forceY = 0.0f;
            ps.forceZ = 0.0f;
            return ps;
        },
        takeLock);
}

TerrainPhysicsSnapshot TerrainGridRepository::getPhysicsSnapshot(int x, int y, int z) const {
    // CRITICAL: All reads happen under a SINGLE shared lock to prevent TOCTOU bugs
    // This prevents terrain from moving/changing between position lookup and velocity read
    std::shared_lock<std::shared_mutex> lock(terrainGridMutex);

    // TerrainPhysicsSnapshot snapshot;

    // // Check terrain existence first
    // snapshot.terrainExists = storage_.checkIfTerrainExists(x, y, z);
    // if (!snapshot.terrainExists) {
    //     // Return empty snapshot if terrain doesn't exist
    //     snapshot.position = Position{x, y, z, DirectionEnum::UP};
    //     snapshot.velocity = Velocity{0.f, 0.f, 0.f};
    //     snapshot.stats = PhysicsStats{};
    //     return snapshot;
    // }

    // // Read position (atomically with existence check)
    // snapshot.position.x = x;
    // snapshot.position.y = y;
    // snapshot.position.z = z;
    // snapshot.position.direction = storage_.getTerrainDirection(x, y, z);

    // // Read velocity from ECS if entity is active
    // entt::entity e = entt::null;
    // auto it = byCoord_.find(Key{x, y, z});
    // if (it != byCoord_.end()) {
    //     e = it->second;
    // }

    // if (e != entt::null && registry_.valid(e)) {
    //     if (auto v = registry_.try_get<Velocity>(e)) {
    //         snapshot.velocity = *v;
    //     } else {
    //         snapshot.velocity = Velocity{0.f, 0.f, 0.f};
    //     }
    // } else {
    //     snapshot.velocity = Velocity{0.f, 0.f, 0.f};
    // }

    // // Read physics stats from OpenVDB storage
    // snapshot.stats.mass = static_cast<float>(storage_.getTerrainMass(x, y, z));
    // snapshot.stats.maxSpeed = static_cast<float>(storage_.getTerrainMaxSpeed(x, y, z));
    // snapshot.stats.minSpeed = static_cast<float>(storage_.getTerrainMinSpeed(x, y, z));
    // snapshot.stats.heat = static_cast<float>(storage_.getTerrainHeat(x, y, z));
    // snapshot.stats.forceX = 0.0f;
    // snapshot.stats.forceY = 0.0f;
    // snapshot.stats.forceZ = 0.0f;

    // return snapshot;
    return TerrainPhysicsSnapshot{};  // TODO: Implement proper snapshot retrieval
}

void TerrainGridRepository::setPhysicsStats(int x, int y, int z, const PhysicsStats& ps,
                                            bool takeLock) {
    withUniqueLock(
        [&]() {
            storage_.setTerrainHeat(x, y, z, static_cast<int>(ps.heat));
            storage_.setTerrainMass(x, y, z, static_cast<int>(ps.mass));
            storage_.setTerrainMaxSpeed(x, y, z, static_cast<int>(ps.maxSpeed));
            storage_.setTerrainMinSpeed(x, y, z, static_cast<int>(ps.minSpeed));
            // forceX/forceY/forceZ/heat are transient or derived; not persisted to VDB here.
        },
        takeLock);
}

DirectionEnum TerrainGridRepository::getDirection(int x, int y, int z) const {
    return storage_.getTerrainDirection(x, y, z);
}
void TerrainGridRepository::setDirection(int x, int y, int z, DirectionEnum dir) {
    storage_.setTerrainDirection(x, y, z, dir);
}

Position TerrainGridRepository::getPosition(int x, int y, int z) const {
    return withSharedLock([&]() {
        Position pos{};
        pos.x = x;
        pos.y = y;
        pos.z = z;
        pos.direction = storage_.getTerrainDirection(x, y, z);
        return pos;
    });
}

void TerrainGridRepository::setPosition(int x, int y, int z, const Position& pos) {
    // Coordinates are implied by (x,y,z); only persist direction.
    withUniqueLock([&]() { storage_.setTerrainDirection(x, y, z, pos.direction); });
}
bool TerrainGridRepository::getCanStackEntities(int x, int y, int z) const {
    return storage_.getTerrainCanStackEntities(x, y, z);
}
void TerrainGridRepository::setCanStackEntities(int x, int y, int z, bool v) {
    storage_.setTerrainCanStackEntities(x, y, z, v);
}
MatterState TerrainGridRepository::getMatterState(int x, int y, int z) const {
    return storage_.getTerrainMatterState(x, y, z);
}
void TerrainGridRepository::setMatterState(int x, int y, int z, MatterState s) {
    storage_.setTerrainMatterState(x, y, z, s);
}
GradientVector TerrainGridRepository::getGradient(int x, int y, int z) const {
    return storage_.getTerrainGradientVector(x, y, z);
}
void TerrainGridRepository::setGradient(int x, int y, int z, const GradientVector& g) {
    storage_.setTerrainGradientVector(x, y, z, g);
}
int TerrainGridRepository::getMaxLoadCapacity(int x, int y, int z) const {
    return storage_.getTerrainMaxLoadCapacity(x, y, z);
}
void TerrainGridRepository::setMaxLoadCapacity(int x, int y, int z, int v) {
    storage_.setTerrainMaxLoadCapacity(x, y, z, v);
}

// ---------------- Transient arbitration (ECS-backed) ----------------
Velocity TerrainGridRepository::getVelocity(int x, int y, int z) const {
    entt::entity e = getEntityAt(x, y, z);
    if (e == entt::null) return Velocity{0.f, 0.f, 0.f};
    if (auto v = registry_.try_get<Velocity>(e)) return *v;
    return Velocity{0.f, 0.f, 0.f};
}

void TerrainGridRepository::setVelocity(int x, int y, int z, const Velocity& vel) {
    entt::entity e = ensureActive(x, y, z);
    if (auto v = registry_.try_get<Velocity>(e)) {
        *v = vel;
    } else {
        registry_.emplace<Velocity>(e, vel);
    }
}

// void TerrainGridRepository::setTerrain(int x, int y, int z, int terrainID) {
//     if (terrainID == -2) {
//         deleteTerrain(x, y, z);
//     } else if (terrainID == -1) {
//         throw std::runtime_error(
//             "TerrainGridRepository::setTerrain: debugging placeholder -1 not supported here");
//     } else {
//         throw std::runtime_error(
//             "TerrainGridRepository::setTerrain: debugging placeholder -1 not supported here");
//     }
// }

// ---------------- Migration Methods ----------------
void TerrainGridRepository::setTerrainFromEntt(entt::entity entity) {
    if (!registry_.valid(entity)) {
        return;  // Invalid entity, nothing to migrate
    }

    // Get the position to know where to store the data
    Position* position = registry_.try_get<Position>(entity);
    if (!position) {
        return;  // No position, cannot determine where to store terrain data
    }

    int x = position->x;
    int y = position->y;
    int z = position->z;

    bool hasStaticComponents = false;

    // Migrate EntityTypeComponent
    if (EntityTypeComponent* etc = registry_.try_get<EntityTypeComponent>(entity)) {
        setTerrainEntityType(x, y, z, *etc);
        registry_.remove<EntityTypeComponent>(entity);
        hasStaticComponents = true;
    }

    // Migrate MatterContainer
    if (MatterContainer* mc = registry_.try_get<MatterContainer>(entity)) {
        setTerrainMatterContainer(x, y, z, *mc);
        registry_.remove<MatterContainer>(entity);
        hasStaticComponents = true;
    }

    // Migrate PhysicsStats
    if (PhysicsStats* ps = registry_.try_get<PhysicsStats>(entity)) {
        setPhysicsStats(x, y, z, *ps);
        registry_.remove<PhysicsStats>(entity);
        hasStaticComponents = true;
    }

    // Migrate StructuralIntegrityComponent
    if (StructuralIntegrityComponent* sic =
            registry_.try_get<StructuralIntegrityComponent>(entity)) {
        setCanStackEntities(x, y, z, sic->canStackEntities);
        setMaxLoadCapacity(x, y, z, sic->maxLoadCapacity);
        setMatterState(x, y, z, sic->matterState);
        setGradient(x, y, z, sic->gradientVector);
        registry_.remove<StructuralIntegrityComponent>(entity);
        hasStaticComponents = true;
    }

    // Store direction from Position component (but don't remove Position if entity has transients)
    setDirection(x, y, z, position->direction);

    // Check if only transient components remain
    bool hasTransientComponents = false;

    // Check for known transient components
    if (registry_.try_get<Velocity>(entity) || registry_.try_get<MovingComponent>(entity)) {
        hasTransientComponents = true;
    }

    // If no transient components remain, we can remove the entity
    if (!hasTransientComponents) {
        // Clear the activation state and remove entity from mapping
        clearActive(x, y, z);
        {
            std::unique_lock<std::shared_mutex> lock(trackingMapsMutex_);
            auto it = byCoord_.find(VoxelCoord{x, y, z});
            if (it != byCoord_.end()) {
                byCoord_.erase(it);
            }
            auto entityIt = byEntity_.find(entity);
            if (entityIt != byEntity_.end()) {
                byEntity_.erase(entityIt);
            }
        }
        registry_.destroy(entity);
        // Set terrain ID to -1 to indicate no active entity, but that terrain exists
        storage_.setTerrainId(x, y, z, -1);
    } else {
        // Otherwise, just update the terrain ID to point to this entity
        storage_.setTerrainId(x, y, z, static_cast<int>(entity));
    }
}

bool TerrainGridRepository::checkIfTerrainExists(int x, int y, int z) const {
    // Always use shared lock for safety - the lock is reentrant-safe for read operations
    // The isTerrainGridLocked() check is kept for compatibility with explicit lock holders
    if (!isTerrainGridLocked()) {
        std::shared_lock<std::shared_mutex> lock(terrainGridMutex);
        return storage_.checkIfTerrainExists(x, y, z);
    }
    // If externally locked, caller guarantees consistency
    return storage_.checkIfTerrainExists(x, y, z);
}

// Delete terrain at a specific voxel
void TerrainGridRepository::deleteTerrain(entt::dispatcher& dispatcher, int x, int y, int z,
                                          bool takeLock) {
    if (takeLock) {
        TerrainGridLock lock(this);
    }

    int terrainId = storage_.deleteTerrain(x, y, z);

    VoxelCoord key{x, y, z};
    if (terrainId != -2 && terrainId != -1 &&
        registry_.valid(static_cast<entt::entity>(terrainId))) {
        // TODO: Handle dropping components and remove from EnTT
        // std::cout << "Deleting terrain entity: " << terrainId << std::endl;
        entt::entity entity = static_cast<entt::entity>(terrainId);
        removeFromTrackingMaps(key, entity);
        dispatcher.enqueue<KillEntityEvent>(entity);
    } else if (terrainId != -2 && terrainId != -1) {
        // TODO
        std::cout << "No active terrain entity to delete at (" << x << ", " << y << ", " << z
                  << ") EntityID: " << terrainId << "But we might clean up something else here."
                  << "\n";
    } else {
        std::cout << "No active terrain entity to delete at (" << x << ", " << y << ", " << z
                  << ") EntityID: " << terrainId << "\n";
    }
}

StructuralIntegrityComponent TerrainGridRepository::getTerrainStructuralIntegrity(
    int x, int y, int z, bool takeLock) const {
    return withSharedLock([&]() { return storage_.getTerrainStructuralIntegrity(x, y, z); },
                          takeLock);
}

void TerrainGridRepository::setTerrainStructuralIntegrity(int x, int y, int z,
                                                          const StructuralIntegrityComponent& sic,
                                                          bool takeLock) {
    withUniqueLock([&]() { storage_.setTerrainStructuralIntegrity(x, y, z, sic); }, takeLock);
}

bool TerrainGridRepository::hasMovingComponent(int x, int y, int z) const {
    // Get the terrain ID at the specified coordinates
    std::optional<int> terrainIdOpt = getTerrainIdIfExists(x, y, z);

    if (!terrainIdOpt.has_value()) {
        // No terrain at this location
        return false;
    }

    int terrainId = terrainIdOpt.value();

    // Check if terrain is in grid storage (not an EnTT entity)
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        return false;
    }

    // Check if terrain is marked as NONE
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return false;
    }

    // Terrain is an EnTT entity - check if it has MovingComponent
    entt::entity entity = static_cast<entt::entity>(terrainId);
    return registry_.all_of<MovingComponent>(entity);
}

// Lock terrain grid for external synchronization
void TerrainGridRepository::lockTerrainGrid() {
    terrainGridMutex.lock();
    terrainGridLocked_.store(true);
}

// Unlock terrain grid after external synchronization
void TerrainGridRepository::unlockTerrainGrid() {
    terrainGridLocked_.store(false);
    terrainGridMutex.unlock();
}

void TerrainGridRepository::softDeactivateEntity(entt::dispatcher& dispatcher, entt::entity e,
                                                 bool takeLock) {
    if (takeLock) {
        if (!isTerrainGridLocked()) {
            spdlog::debug("softDeactivateEntity: acquiring TerrainGridLock for entity {}",
                          static_cast<int>(e));
            TerrainGridLock lock(this);
        } else {
            spdlog::debug(
                "softDeactivateEntity: repository already locked; skipping lock for entity {}",
                static_cast<int>(e));
        }
    } else {
        // Caller requested no lock; warn if repository is not externally locked to help catch
        // misuse
        if (!isTerrainGridLocked()) {
            spdlog::debug(
                "softDeactivateEntity: called with takeLock=false but repository is not locked for "
                "entity {}",
                static_cast<int>(e));
        }
    }

    if (!registry_.valid(e)) {
        // spdlog::warn("softDeactivateEntity: entity {} is not valid", static_cast<int>(e));
        return;
    }

    // Try to find the voxel coord for this entity
    VoxelCoord key{0, 0, 0};
    bool found = false;
    {
        std::shared_lock<std::shared_mutex> tlock(trackingMapsMutex_);
        auto it = byEntity_.find(e);
        if (it != byEntity_.end()) {
            key = it->second;
            found = true;
        }
    }

    // If entity has transient components, remove them and rely on hooks to clean mapping
    bool hadTransient = false;
    if (registry_.all_of<Velocity>(e)) {
        hadTransient = true;
        // registry_.remove<Velocity>(e);
        dispatcher.enqueue<TerrainRemoveVelocityEvent>(e);
    }
    if (registry_.all_of<MovingComponent>(e)) {
        hadTransient = true;
        // registry_.remove<MovingComponent>(e);
        dispatcher.enqueue<TerrainRemoveMovingComponentEvent>(e);
    }

    if (!hadTransient) {
        // No transient components removed — perform cleanup directly
        if (!found) {
            spdlog::debug("softDeactivateEntity: entity {} has no mapping and no transients",
                          static_cast<int>(e));
            return;
        }

        // Remove mapping and mark voxel as storage-only
        withTrackingMapsLock(
            [&]() {
                byCoord_.erase(key);
                byEntity_.erase(e);
                // Mark as ON_GRID_STORAGE
                setTerrainId(key.x, key.y, key.z,
                             static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE), false);
                clearActive(key.x, key.y, key.z, false);
            },
            false, false);
    }

    spdlog::debug("softDeactivateEntity: entity {} soft-deactivated", static_cast<int>(e));
}

int64_t TerrainGridRepository::sumTotalWater() const { return storage_.sumTotalWater(); }
