#include "terrain/TerrainGridRepository.hpp"

#include <openvdb/openvdb.h>

#include <cassert>
#include <functional>

namespace {
inline openvdb::Coord C(int x, int y, int z) { return openvdb::Coord(x, y, z); }
}  // namespace

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

std::optional<int> TerrainGridRepository::getTerrainIdIfExists(int x, int y, int z) const {
    return storage_.getTerrainIdIfExists(x, y, z);
}

void TerrainGridRepository::markActive(int x, int y, int z, entt::entity e) {
    // Update TerrainStorage activation indicator based on strategy
    if (storage_.useActiveMask) {
        if (storage_.terrainGrid) {
            storage_.terrainGrid->tree().setValue(C(x, y, z), 1);  // mark as active
        }
    } else {
        if (storage_.terrainGrid) {
            storage_.terrainGrid->tree().setValue(C(x, y, z), static_cast<int>(e));
        }
    }
}

void TerrainGridRepository::clearActive(int x, int y, int z) {
    if (!storage_.terrainGrid) return;
    if (storage_.useActiveMask) {
        storage_.terrainGrid->tree().setValue(C(x, y, z), 0);
    } else {
        storage_.terrainGrid->tree().setValue(C(x, y, z), storage_.bgEntityId);
    }
}

void TerrainGridRepository::onConstructVelocity(entt::registry& reg, entt::entity e) {
    // If entity has a Position, mark active in the TerrainStorage grid
    if (auto pos = reg.try_get<Position>(e)) {
        Key key{pos->x, pos->y, pos->z};
        byCoord_[key] = e;
        markActive(pos->x, pos->y, pos->z, e);
    }
}

void TerrainGridRepository::onConstructMoving(entt::registry& reg, entt::entity e) {
    if (auto pos = reg.try_get<Position>(e)) {
        Key key{pos->x, pos->y, pos->z};
        byCoord_[key] = e;
        markActive(pos->x, pos->y, pos->z, e);
    }
}

entt::entity TerrainGridRepository::ensureActive(int x, int y, int z) {
    Key key{x, y, z};
    auto it = byCoord_.find(key);
    if (it != byCoord_.end()) return it->second;

    entt::entity e = registry_.create();
    registry_.emplace<Position>(e, Position{x, y, z, storage_.getTerrainDirection(x, y, z)});
    registry_.emplace<Velocity>(e, Velocity{0.f, 0.f, 0.f});
    registry_.emplace<MovingComponent>(e, MovingComponent{0});
    byCoord_.emplace(key, e);
    markActive(x, y, z, e);
    return e;
}

bool TerrainGridRepository::isActive(int x, int y, int z) const {
    // Prefer authoritative storage indicator; overlay map is a cache
    return storage_.isActive(x, y, z);
}

// ---------------- EntityTypeComponent aggregation ----------------
EntityTypeComponent TerrainGridRepository::getTerrainEntityType(int x, int y, int z) const {
    EntityTypeComponent etc{};
    etc.mainType = getTerrainMainType(x, y, z);
    etc.subType0 = getTerrainSubType0(x, y, z);
    etc.subType1 = getTerrainSubType1(x, y, z);
    return etc;
}

void TerrainGridRepository::setTerrainEntityType(int x, int y, int z, EntityTypeComponent etc) {
    setTerrainMainType(x, y, z, etc.mainType);
    setTerrainSubType0(x, y, z, etc.subType0);
    setTerrainSubType1(x, y, z, etc.subType1);
}

TerrainInfo TerrainGridRepository::readTerrainInfo(int x, int y, int z) const {
    TerrainInfo info;
    info.x = x;
    info.y = y;
    info.z = z;
    info.active = isActive(x, y, z);

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
    MatterContainer mc{};
    mc.TerrainMatter = getTerrainMatter(x, y, z);
    mc.WaterVapor = getVaporMatter(x, y, z);
    mc.WaterMatter = getWaterMatter(x, y, z);
    mc.BioMassMatter = getBiomassMatter(x, y, z);
    return mc;
}

void TerrainGridRepository::setTerrainMatterContainer(int x, int y, int z,
                                                      const MatterContainer& mc) {
    setTerrainMatter(x, y, z, mc.TerrainMatter);
    setVaporMatter(x, y, z, mc.WaterVapor);
    setWaterMatter(x, y, z, mc.WaterMatter);
    setBiomassMatter(x, y, z, mc.BioMassMatter);
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
    PhysicsStats ps{};
    ps.mass = static_cast<float>(getMass(x, y, z));
    ps.maxSpeed = static_cast<float>(getMaxSpeed(x, y, z));
    ps.minSpeed = static_cast<float>(getMinSpeed(x, y, z));
    ps.forceX = 0.0f;
    ps.forceY = 0.0f;
    ps.forceZ = 0.0f;
    ps.heat = 0.0f;
    return ps;
}

void TerrainGridRepository::setPhysicsStats(int x, int y, int z, const PhysicsStats& ps) {
    setMass(x, y, z, static_cast<int>(ps.mass));
    setMaxSpeed(x, y, z, static_cast<int>(ps.maxSpeed));
    setMinSpeed(x, y, z, static_cast<int>(ps.minSpeed));
    // forceX/forceY/forceZ/heat are transient or derived; not persisted to VDB here.
}

DirectionEnum TerrainGridRepository::getDirection(int x, int y, int z) const {
    return storage_.getTerrainDirection(x, y, z);
}
void TerrainGridRepository::setDirection(int x, int y, int z, DirectionEnum dir) {
    storage_.setTerrainDirection(x, y, z, dir);
}

Position TerrainGridRepository::getPosition(int x, int y, int z) const {
    Position pos{};
    pos.x = x;
    pos.y = y;
    pos.z = z;
    pos.direction = getDirection(x, y, z);
    return pos;
}

void TerrainGridRepository::setPosition(int x, int y, int z, const Position& pos) {
    // Coordinates are implied by (x,y,z); only persist direction.
    setDirection(x, y, z, pos.direction);
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
        registry_.destroy(entity);
        storage_.setTerrainId(x, y, z, static_cast<int>(entity));
    } else {
        storage_.setTerrainId(x, y, z, -1);
    }
}

bool TerrainGridRepository::checkIfTerrainExists(int x, int y, int z) const {
    return storage_.checkIfTerrainExists(x, y, z);
}

// Delete terrain at a specific voxel
void TerrainGridRepository::deleteTerrain(int x, int y, int z) { storage_.deleteTerrain(x, y, z); }

StructuralIntegrityComponent TerrainGridRepository::getTerrainStructuralIntegrity(int x, int y,
                                                                                  int z) const {
    return storage_.getTerrainStructuralIntegrity(x, y, z);
}

void TerrainGridRepository::setTerrainStructuralIntegrity(int x, int y, int z,
                                                          const StructuralIntegrityComponent& sic) {
    storage_.setTerrainStructuralIntegrity(x, y, z, sic);
}
