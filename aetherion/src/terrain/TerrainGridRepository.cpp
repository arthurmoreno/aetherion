#include "terrain/TerrainGridRepository.hpp"

#include <openvdb/openvdb.h>

#include <cassert>
#include <functional>
#include <shared_mutex>

namespace {
inline openvdb::Coord C(int x, int y, int z) { return openvdb::Coord(x, y, z); }
}  // namespace

bool TerrainGridRepository::isTerrainGridLocked() const {
    return terrainGridLocked_;
}

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
}

entt::entity TerrainGridRepository::getEntityAt(int x, int y, int z) const {
    auto it = byCoord_.find(Key{x, y, z});
    if (it == byCoord_.end()) return entt::null;
    return it->second;
}

Position TerrainGridRepository::getPositionOfEntt(entt::entity terrain_entity) const {
    auto it = byEntity_.find(terrain_entity);
    if (it == byEntity_.end()) {
        // Entity not found in mapping, return invalid position
        return Position{-1, -1, -1, DirectionEnum::UP};
    }

    const Key& key = it->second;
    return getPosition(key.x, key.y, key.z);
}

void TerrainGridRepository::moveTerrain(MovingComponent& movingComponent) {
    // Get the current position of the entity
    std::optional<int> currentPositionEntityId = getTerrainIdIfExists(
        movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
    std::optional<int> movingToPositionEntityId = getTerrainIdIfExists(
        movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ);
    if (currentPositionEntityId && !movingToPositionEntityId) {
        int terrainID = currentPositionEntityId.value();
        std::cout << "Moving terrain entity ID " << terrainID << " from ("
                  << movingComponent.movingFromX << ", " << movingComponent.movingFromY << ", "
                  << movingComponent.movingFromZ << ") to (" << movingComponent.movingToX << ", "
                  << movingComponent.movingToY << ", " << movingComponent.movingToZ << ")\n";

        // CRITICAL: Reserve destination FIRST to prevent race conditions
        // This must happen before copying any data to prevent other entities from trying to move here
        setTerrainId(movingComponent.movingToX, movingComponent.movingToY,
                     movingComponent.movingToZ, terrainID);
        setTerrainId(movingComponent.movingFromX, movingComponent.movingFromY,
                     movingComponent.movingFromZ, -2);  // Clear old position

        // Copy Position component
        setDirection(movingComponent.movingToX, movingComponent.movingToY,
                     movingComponent.movingToZ, movingComponent.direction);

        // Copy structural EntityTypeComponent component
        EntityTypeComponent currentEntityType = getTerrainEntityType(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        setTerrainEntityType(
            movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ,
            EntityTypeComponent{currentEntityType.mainType, currentEntityType.subType0,
                                currentEntityType.subType1});

        // Copy structural StructuralIntegrityComponent component
        StructuralIntegrityComponent currentSIC = getTerrainStructuralIntegrity(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        setCanStackEntities(movingComponent.movingToX, movingComponent.movingToY,
                            movingComponent.movingToZ, currentSIC.canStackEntities);
        setMaxLoadCapacity(movingComponent.movingToX, movingComponent.movingToY,
                           movingComponent.movingToZ, currentSIC.maxLoadCapacity);
        setGradient(movingComponent.movingToX, movingComponent.movingToY, movingComponent.movingToZ,
                    currentSIC.gradientVector);
        setMatterState(movingComponent.movingToX, movingComponent.movingToY,
                       movingComponent.movingToZ, currentSIC.matterState);

        // Copy structural MatterContainer component
        MatterContainer currentMC = getTerrainMatterContainer(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        setTerrainMatterContainer(movingComponent.movingToX, movingComponent.movingToY,
                                  movingComponent.movingToZ, currentMC);

        // Copy structural PhysicsStats component
        PhysicsStats currentPS = getPhysicsStats(
            movingComponent.movingFromX, movingComponent.movingFromY, movingComponent.movingFromZ);
        setPhysicsStats(movingComponent.movingToX, movingComponent.movingToY,
                        movingComponent.movingToZ,
                        PhysicsStats{currentPS.mass, currentPS.maxSpeed, currentPS.minSpeed, 0.0f,
                                     0.0f, 0.0f, 0.0f});
    } else {
        std::string errorMsg = "Invalid terrain move from (" + std::to_string(movingComponent.movingFromX) + 
            "," + std::to_string(movingComponent.movingFromY) + "," + std::to_string(movingComponent.movingFromZ) + 
            ") to (" + std::to_string(movingComponent.movingToX) + "," + std::to_string(movingComponent.movingToY) + 
            "," + std::to_string(movingComponent.movingToZ) + "). Source: " + 
            (currentPositionEntityId ? "ID=" + std::to_string(currentPositionEntityId.value()) : "none") + 
            ", Dest: " + (movingToPositionEntityId ? "ID=" + std::to_string(movingToPositionEntityId.value()) : "none");
        std::cout << errorMsg << "\n";
        throw std::runtime_error(errorMsg);
    }
}

std::optional<int> TerrainGridRepository::getTerrainIdIfExists(int x, int y, int z) const {
    return withSharedLock([&]() -> std::optional<int> {
        int terrainId = storage_.getTerrainIdIfExists(x, y, z);
        if (terrainId == -2) {
            return std::nullopt;
        }
        return terrainId;
    });
}

bool TerrainGridRepository::checkIfTerrainHasEntity(int x, int y, int z) const {
    return withSharedLock([&]() {
        std::optional<int> entityId = storage_.getTerrainIdIfExists(x, y, z);
        return entityId.has_value() && entityId.value() >= 0;
    });
}

void TerrainGridRepository::markActive(int x, int y, int z, entt::entity e) {
    // Update TerrainStorage activation indicator based on strategy
    withUniqueLock([&]() {
        if (storage_.terrainGrid) {
            storage_.terrainGrid->tree().setValue(C(x, y, z), static_cast<int>(e));
        }
    });
}

void TerrainGridRepository::clearActive(int x, int y, int z) {
    withUniqueLock([&]() {
        if (!storage_.terrainGrid) return;
        if (storage_.useActiveMask) {
            storage_.terrainGrid->tree().setValue(C(x, y, z), 0);
        } else {
            storage_.terrainGrid->tree().setValue(C(x, y, z), storage_.bgEntityId);
        }
    });
}

void TerrainGridRepository::setTerrainId(int x, int y, int z, int terrainID) {
    withUniqueLock([&]() {
        std::cout << "[setTerrainId] Setting terrain ID at (" << x << ", " << y << ", " << z << ") to "
                  << terrainID << std::endl;
        storage_.terrainGrid->tree().setValue(C(x, y, z), terrainID);
    });
}

void TerrainGridRepository::onConstructVelocity(entt::registry& reg, entt::entity e) {
    // If entity has a Position, mark active in the TerrainStorage grid
    if (!reg.valid(e)) return;
    if (auto pos = reg.try_get<Position>(e)) {
        auto entityType = reg.try_get<EntityTypeComponent>(e);
        if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
            return;
        }
        if (checkIfTerrainHasEntity(pos->x, pos->y, pos->z)) {
            // std::cout << "TerrainGridRepository: onConstructVelocity at (" << pos->x << ", "
            //           << pos->y << ", " << pos->z << ") entity=" << int(e) << "\n";
            Key key{pos->x, pos->y, pos->z};
            byCoord_[key] = e;
            markActive(pos->x, pos->y, pos->z, e);
        }
    }
}

void TerrainGridRepository::onConstructMoving(entt::registry& reg, entt::entity e) {
    if (!reg.valid(e)) return;
    if (auto pos = reg.try_get<Position>(e)) {
        auto entityType = reg.try_get<EntityTypeComponent>(e);
        if (entityType && entityType->mainType != static_cast<int>(EntityEnum::TERRAIN)) {
            return;
        }
        if (checkIfTerrainHasEntity(pos->x, pos->y, pos->z)) {
            // std::cout << "TerrainGridRepository: onConstructMoving at (" << pos->x << ", " << pos->y
            //           << ", " << pos->z << ") entity=" << int(e) << "\n";
            Key key{pos->x, pos->y, pos->z};
            byCoord_[key] = e;
            markActive(pos->x, pos->y, pos->z, e);
        }
    }
}

entt::entity TerrainGridRepository::ensureActive(int x, int y, int z) {
    Key key{x, y, z};
    auto it = byCoord_.find(key);
    if (it != byCoord_.end()) return it->second;

    DirectionEnum direction = withSharedLock([&]() {
        return storage_.getTerrainDirection(x, y, z);
    });

    entt::entity e = registry_.create();
    registry_.emplace<Position>(e, Position{x, y, z, direction});
    registry_.emplace<Velocity>(e, Velocity{0.f, 0.f, 0.f});
    registry_.emplace<MovingComponent>(e, MovingComponent{0});
    byCoord_.emplace(key, e);
    byEntity_.emplace(e, key);
    markActive(x, y, z, e);
    return e;
}


// ---------------- EntityTypeComponent aggregation ----------------
EntityTypeComponent TerrainGridRepository::getTerrainEntityType(int x, int y, int z) const {
    return withSharedLock([&]() {
        EntityTypeComponent etc{};
        etc.mainType = storage_.getTerrainMainType(x, y, z);
        etc.subType0 = storage_.getTerrainSubType0(x, y, z);
        etc.subType1 = storage_.getTerrainSubType1(x, y, z);
        return etc;
    });
}

void TerrainGridRepository::setTerrainEntityType(int x, int y, int z, EntityTypeComponent etc) {
    withUniqueLock([&]() {
        storage_.setTerrainMainType(x, y, z, etc.mainType);
        storage_.setTerrainSubType0(x, y, z, etc.subType0);
        storage_.setTerrainSubType1(x, y, z, etc.subType1);
    });
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

PhysicsStats TerrainGridRepository::getPhysicsStats(int x, int y, int z) const {
    return withSharedLock([&]() {
        PhysicsStats ps{};
        ps.mass = static_cast<float>(storage_.getTerrainMass(x, y, z));
        ps.maxSpeed = static_cast<float>(storage_.getTerrainMaxSpeed(x, y, z));
        ps.minSpeed = static_cast<float>(storage_.getTerrainMinSpeed(x, y, z));
        ps.heat = static_cast<float>(storage_.getTerrainHeat(x, y, z));
        ps.forceX = 0.0f;
        ps.forceY = 0.0f;
        ps.forceZ = 0.0f;
        return ps;
    });
}

void TerrainGridRepository::setPhysicsStats(int x, int y, int z, const PhysicsStats& ps) {
    withUniqueLock([&]() {
        storage_.setTerrainHeat(x, y, z, static_cast<int>(ps.heat));
        storage_.setTerrainMass(x, y, z, static_cast<int>(ps.mass));
        storage_.setTerrainMaxSpeed(x, y, z, static_cast<int>(ps.maxSpeed));
        storage_.setTerrainMinSpeed(x, y, z, static_cast<int>(ps.minSpeed));
        // forceX/forceY/forceZ/heat are transient or derived; not persisted to VDB here.
    });
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
    withUniqueLock([&]() {
        storage_.setTerrainDirection(x, y, z, pos.direction);
    });
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
        auto it = byCoord_.find(Key{x, y, z});
        if (it != byCoord_.end()) {
            byCoord_.erase(it);
        }
        auto entityIt = byEntity_.find(entity);
        if (entityIt != byEntity_.end()) {
            byEntity_.erase(entityIt);
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
    if (!isTerrainGridLocked()) {
        std::shared_lock<std::shared_mutex> lock(terrainGridMutex);
        return storage_.checkIfTerrainExists(x, y, z);
    }
    return storage_.checkIfTerrainExists(x, y, z);
}

// Delete terrain at a specific voxel
void TerrainGridRepository::deleteTerrain(entt::dispatcher& dispatcher, int x, int y, int z) {
    int terrainId = withUniqueLock([&]() {
        return storage_.deleteTerrain(x, y, z);
    });
    
    if (terrainId != -2 && terrainId != -1 &&
        registry_.valid(static_cast<entt::entity>(terrainId))) {
        // TODO: Handle dropping components and remove from EnTT
        std::cout << "Deleting terrain entity: " << terrainId << std::endl;
        entt::entity entity = static_cast<entt::entity>(terrainId);
        dispatcher.enqueue<KillEntityEvent>(entity);
    }
}

StructuralIntegrityComponent TerrainGridRepository::getTerrainStructuralIntegrity(int x, int y,
                                                                                  int z) const {
    return withSharedLock([&]() {
        return storage_.getTerrainStructuralIntegrity(x, y, z);
    });
}

void TerrainGridRepository::setTerrainStructuralIntegrity(int x, int y, int z,
                                                          const StructuralIntegrityComponent& sic) {
    withUniqueLock([&]() {
        storage_.setTerrainStructuralIntegrity(x, y, z, sic);
    });
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
    terrainGridLocked_ = true;
}

// Unlock terrain grid after external synchronization
void TerrainGridRepository::unlockTerrainGrid() {
    terrainGridLocked_ = false;
    terrainGridMutex.unlock();
}
