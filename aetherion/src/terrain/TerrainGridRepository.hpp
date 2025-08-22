#ifndef TERRAIN_GRID_REPOSITORY_HPP
#define TERRAIN_GRID_REPOSITORY_HPP

#include <entt/entt.hpp>
#include <optional>
#include <unordered_map>

#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"
#include "components/EntityTypeComponent.hpp"
#include "terrain/TerrainStorage.hpp"

// TerrainGridRepository provides an ECS overlay for transient behavior while
// delegating all static storage to TerrainStorage (OpenVDB-backed).
//
// Rules:
// - Static data lives in TerrainStorage only.
// - Transient data (e.g., Velocity, runtime timers) lives in the ECS only.
// - Activation creates an ECS entity mapped to a voxel and marks activity via
//   TerrainStorage's active mask or entity-id grid strategy.
// - Deactivation clears the activity indicator and destroys the ECS entity.
class TerrainGridRepository {
   public:
    // Lightweight transient-only component example (timers, markers, etc.)
    struct MovingComponent {
        int ticksRemaining{0};
    };

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

    TerrainGridRepository(entt::registry& registry, TerrainStorage& storage);

    // Query whether a voxel is currently active (ECS-managed); kept for inspection
    bool isActive(int x, int y, int z) const;

    // Arbitration helper: inactive ⇒ VDB only; active ⇒ VDB static + ECS transient
    TerrainInfo readTerrainInfo(int x, int y, int z) const;

    // Tick transient systems; auto-deactivate when no transients remain
    void tick(int dtTicks = 1);

    // ---------------- Static getters/setters (VDB-backed) ----------------

    // EntityTypeComponent
    EntityTypeComponent getTerrainEntityType(int x, int y, int z) const;
    void setTerrainEntityType(int x, int y, int z, EntityTypeComponent etc);

    int getMainType(int x, int y, int z) const;
    void setMainType(int x, int y, int z, int v);

    int getSubType0(int x, int y, int z) const;
    void setSubType0(int x, int y, int z, int v);

    int getSubType1(int x, int y, int z) const;
    void setSubType1(int x, int y, int z, int v);

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

    int getMovingTicksRemaining(int x, int y, int z) const;
    void setMovingTicksRemaining(int x, int y, int z, int ticks);

   private:
    struct Key {
        int x, y, z;
        bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct KeyHash {
        std::size_t operator()(const Key& k) const noexcept {
            // Simple integer hash combine
            std::size_t h = static_cast<std::size_t>(k.x);
            h ^= static_cast<std::size_t>(k.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= static_cast<std::size_t>(k.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    entt::registry& registry_;
    TerrainStorage& storage_;
    std::unordered_map<Key, entt::entity, KeyHash> byCoord_;

    entt::entity getEntityAt(int x, int y, int z) const;
    entt::entity ensureActive(int x, int y, int z);
    void markActive(int x, int y, int z, entt::entity e);
    void clearActive(int x, int y, int z);
};

#endif  // TERRAIN_GRID_REPOSITORY_HPP
