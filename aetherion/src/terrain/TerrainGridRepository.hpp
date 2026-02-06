#ifndef TERRAIN_GRID_REPOSITORY_HPP
#define TERRAIN_GRID_REPOSITORY_HPP

#include <entt/entt.hpp>
#include <optional>
#include <shared_mutex>
#include <atomic>
#include <unordered_map>

#include "components/EntityTypeComponent.hpp"
#include "components/LifecycleComponents.hpp"
#include "components/MovingComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "ecosystem/EcosystemEvents.hpp"
#include "terrain/TerrainStorage.hpp"
#include "terrain/VoxelCoord.hpp"

// TerrainGridRepository provides an ECS overlay for transient behavior while
// delegating all static storage to TerrainStorage (OpenVDB-backed).
//
// Rules:
// - Static data lives in TerrainStorage only.
// - Transient data (e.g., Velocity, runtime timers) lives in the ECS only.
// - Activation creates an ECS entity mapped to a voxel and marks activity via
//   TerrainStorage's active mask or entity-id grid strategy.
// - Deactivation clears the activity indicator and destroys the ECS entity.

// Snapshot of static attributes for a voxel
struct StaticData {
    int mainType{0};
    int subType0{0};
    int subType1{-1};
    MatterContainer matter{0, 0, 0, 0};
    int mass{0};
    int maxSpeed{0};
    int minSpeed{0};
    DirectionEnum direction{DirectionEnum::UP};
    bool canStackEntities{false};
    MatterState matterState{MatterState::SOLID};
    GradientVector gradient{0.f, 0.f, 0.f};
    int maxLoadCapacity{0};
};

// Optional transient overlay data (only present when active)
struct TransientData {
    Velocity velocity{0.f, 0.f, 0.f};
    MovingComponent moving{};
};

struct TerrainInfo {
    int x{0}, y{0}, z{0};
    bool active{false};
    StaticData stat{};
    std::optional<TransientData> transient{};
};

// Atomic multi-read for physics calculations (prevents TOCTOU bugs)
struct TerrainPhysicsSnapshot {
    Position position;
    Velocity velocity;
    PhysicsStats stats;
    bool terrainExists;
};

class TerrainGridRepository {
   public:
    TerrainGridRepository(entt::registry& registry, TerrainStorage& storage);

    // Arbitration helper: inactive ⇒ VDB only; active ⇒ VDB static + ECS transient
    TerrainInfo readTerrainInfo(int x, int y, int z) const;

    // Tick transient systems; auto-deactivate when no transients remain
    void tick(int dtTicks = 1);

    void setTerrainId(int x, int y, int z, int terrainID, bool takeLock = true);

    bool isTerrainIdOnEnttRegistry(int terrainID) const;

    // ---------------- Static getters/setters (VDB-backed) ----------------

    std::optional<int> getTerrainIdIfExists(int x, int y, int z, bool takeLock = true) const;

    // EntityTypeComponent
    EntityTypeComponent getTerrainEntityType(int x, int y, int z) const;
    void setTerrainEntityType(int x, int y, int z, EntityTypeComponent etc);

    int getTerrainMainType(int x, int y, int z) const;
    void setTerrainMainType(int x, int y, int z, int v);

    int getTerrainSubType0(int x, int y, int z) const;
    void setTerrainSubType0(int x, int y, int z, int v);

    int getTerrainSubType1(int x, int y, int z) const;
    void setTerrainSubType1(int x, int y, int z, int v);

    // StructuralIntegrityComponent
    StructuralIntegrityComponent getTerrainStructuralIntegrity(int x, int y, int z) const;
    void setTerrainStructuralIntegrity(int x, int y, int z,
                                       const StructuralIntegrityComponent& sic);

    // Matter
    MatterContainer getTerrainMatterContainer(int x, int y, int z) const;
    void setTerrainMatterContainer(int x, int y, int z, const MatterContainer& mc);

    int getTerrainMatter(int x, int y, int z) const;
    void setTerrainMatter(int x, int y, int z, int v);
    int getWaterMatter(int x, int y, int z) const;
    void setWaterMatter(int x, int y, int z, int v);
    int getVaporMatter(int x, int y, int z) const;
    void setVaporMatter(int x, int y, int z, int v);
    int getBiomassMatter(int x, int y, int z) const;
    void setBiomassMatter(int x, int y, int z, int v);

    // Physics stats
    PhysicsStats getPhysicsStats(int x, int y, int z) const;
    void setPhysicsStats(int x, int y, int z, const PhysicsStats& ps);

    // Atomic multi-read for physics calculations (prevents TOCTOU bugs)
    TerrainPhysicsSnapshot getPhysicsSnapshot(int x, int y, int z) const;

    int getMass(int x, int y, int z) const;
    void setMass(int x, int y, int z, int v);
    int getMaxSpeed(int x, int y, int z) const;
    void setMaxSpeed(int x, int y, int z, int v);
    int getMinSpeed(int x, int y, int z) const;
    void setMinSpeed(int x, int y, int z, int v);

    // Flags/aux
    Position getPosition(int x, int y, int z) const;
    void setPosition(int x, int y, int z, const Position& pos);

    DirectionEnum getDirection(int x, int y, int z) const;
    void setDirection(int x, int y, int z, DirectionEnum dir);
    bool getCanStackEntities(int x, int y, int z) const;
    void setCanStackEntities(int x, int y, int z, bool v);
    MatterState getMatterState(int x, int y, int z) const;
    void setMatterState(int x, int y, int z, MatterState s);
    GradientVector getGradient(int x, int y, int z) const;
    void setGradient(int x, int y, int z, const GradientVector& g);
    int getMaxLoadCapacity(int x, int y, int z) const;
    void setMaxLoadCapacity(int x, int y, int z, int v);

    // ---------------- Transient getters/setters (ECS-backed) -------------
    // Get velocity without activating; returns zero when inactive
    Velocity getVelocity(int x, int y, int z) const;
    // Setting a transient auto-activates voxel and writes to ECS only
    void setVelocity(int x, int y, int z, const Velocity& vel);

    // ---------------- Migration Methods ----------------
    // Extract terrain components from EnTT entity and save to OpenVDB storage
    // If no transient components remain after migration, destroys the entity
    // void setTerrain(int x, int y, int z, int terrainID);
    void setTerrainFromEntt(entt::entity entity);
    bool checkIfTerrainExists(int x, int y, int z) const;
    bool checkIfTerrainHasEntity(int x, int y, int z, bool takeLock = true) const;
    // If `takeLock` is true (default) the function will acquire the TerrainGridLock;
    // callers that already hold the lock may pass `false` to avoid double-locking.
    void deleteTerrain(entt::dispatcher& dispatcher, int x, int y, int z,
                       bool takeLock = true);

    // Soft-deactivate an active terrain EnTT entity without immediately destroying it.
    // This removes transient components (Velocity/MovingComponent) and ensures the
    // repository mapping and storage are updated to mark the voxel as back in grid storage.
    // Callers may then decide to schedule a final destruction later.
    void softDeactivateEntity(entt::dispatcher& dispatcher, entt::entity e, bool takeLock = true);

    // Check if a terrain voxel has a MovingComponent
    bool hasMovingComponent(int x, int y, int z) const;

    // ================ High-Level Iterator Methods ================
    // Efficient full-grid iteration with access to both static and transient data
    template <typename Callback>
    void iterateWaterMatter(Callback callback) const;

    template <typename Callback>
    void iterateVaporMatter(Callback callback) const;

    template <typename Callback>
    void iterateBiomassMatter(Callback callback) const;

    // Generic iterator that provides TerrainInfo for each active voxel
    template <typename Callback>
    void iterateActiveVoxels(Callback callback) const;

    Position getPositionOfEntt(entt::entity terrain_entity) const;
    void moveTerrain(MovingComponent& movingComponent);

    // Locking methods for external synchronization during terrain movement
    void lockTerrainGrid();
    void unlockTerrainGrid();

    entt::entity ensureActive(int x, int y, int z);

    // Tracking map helpers
    // Tracking map helpers. Two-level configurability:
    // - `takeTrackingLock`: whether to acquire `trackingMapsMutex_`.
    // - `respectTerrainGridLock`: when true (default) the helper will skip acquiring
    //    `trackingMapsMutex_` if the terrain grid is externally locked via
    //    `lockTerrainGrid()`; this mirrors the previous behavior.
    void addToTrackingMaps(const VoxelCoord& key, entt::entity e,
                           bool takeTrackingLock = true, bool respectTerrainGridLock = true);
    void removeFromTrackingMaps(const VoxelCoord& key, entt::entity e,
                                bool takeTrackingLock = true, bool respectTerrainGridLock = true);

    template <typename Func>
    auto withTrackingMapsLock(Func&& func, bool takeTrackingLock = true,
                              bool respectTerrainGridLock = true) -> decltype(func()) {
        if (!takeTrackingLock) return func();
        if (respectTerrainGridLock) {
            if (!isTerrainGridLocked()) {
                std::unique_lock<std::shared_mutex> lock(trackingMapsMutex_);
                return func();
            }
            return func();
        }
        std::unique_lock<std::shared_mutex> lock(trackingMapsMutex_);
        return func();
    }

    bool isTerrainGridLocked() const;
   private:
    // Check if terrain grid is currently locked

    // Utility methods for conditional locking
    template <typename Func>
    auto withSharedLock(Func&& func, bool takeLock = true) const -> decltype(func()) {
        if (!takeLock) {
            return func();
        }
        if (!isTerrainGridLocked()) {
            std::shared_lock<std::shared_mutex> lock(terrainGridMutex);
            return func();
        }
        return func();
    }

    template <typename Func>
    auto withUniqueLock(Func&& func, bool takeLock = true) -> decltype(func()) {
        if (!takeLock) {
            return func();
        }
        if (!isTerrainGridLocked()) {
            std::unique_lock<std::shared_mutex> lock(terrainGridMutex);
            return func();
        }
        return func();
    }

    // Mutex specifically for terrainGrid thread safety
    mutable std::shared_mutex terrainGridMutex;
    // Track if terrain grid is currently locked (atomic to avoid races)
    mutable std::atomic<bool> terrainGridLocked_{false};

    // Mutex specifically for tracking maps (byCoord_ and byEntity_) thread safety
    // Separate from terrainGridMutex for better performance (less lock contention)
    mutable std::shared_mutex trackingMapsMutex_;

    entt::registry& registry_;
    TerrainStorage& storage_;
    std::unordered_map<VoxelCoord, entt::entity, VoxelCoordHash> byCoord_;
    std::unordered_map<entt::entity, VoxelCoord> byEntity_;

    entt::entity getEntityAt(int x, int y, int z) const;
    void markActive(int x, int y, int z, entt::entity e, bool takeLock = true);
    void clearActive(int x, int y, int z, bool takeLock = true);

    // EnTT hooks to auto-activate on transient component emplacement
    void onConstructVelocity(entt::registry& reg, entt::entity e);
    void onConstructMoving(entt::registry& reg, entt::entity e);
    void onDestroyVelocity(entt::registry& reg, entt::entity e);
    void onDestroyMoving(entt::registry& reg, entt::entity e);
};

// ================ Template Method Implementations ================
// These must be in the header for proper instantiation

template <typename Callback>
void TerrainGridRepository::iterateWaterMatter(Callback callback) const {
    storage_.iterateWaterMatter([this, callback](int x, int y, int z, int amount) {
        // Provide both static water amount and full terrain info (including transients)
        TerrainInfo info = this->readTerrainInfo(x, y, z);
        callback(x, y, z, amount, info);
    });
}

template <typename Callback>
void TerrainGridRepository::iterateVaporMatter(Callback callback) const {
    storage_.iterateVaporMatter([this, callback](int x, int y, int z, int amount) {
        // Provide both static vapor amount and full terrain info (including transients)
        TerrainInfo info = this->readTerrainInfo(x, y, z);
        callback(x, y, z, amount, info);
    });
}

template <typename Callback>
void TerrainGridRepository::iterateBiomassMatter(Callback callback) const {
    storage_.iterateBiomassMatter([this, callback](int x, int y, int z, int amount) {
        // Provide both static biomass amount and full terrain info (including transients)
        TerrainInfo info = this->readTerrainInfo(x, y, z);
        callback(x, y, z, amount, info);
    });
}

template <typename Callback>
void TerrainGridRepository::iterateActiveVoxels(Callback callback) const {
    // Iterate through all active coordinates and provide full terrain info
    for (const auto& [key, entity] : byCoord_) {
        TerrainInfo info = readTerrainInfo(key.x, key.y, key.z);
        callback(key.x, key.y, key.z, info);
    }
}

#endif  // TERRAIN_GRID_REPOSITORY_HPP
