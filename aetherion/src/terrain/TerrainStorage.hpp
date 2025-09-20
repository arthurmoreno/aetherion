#ifndef TERRAIN_STORAGE_HPP
#define TERRAIN_STORAGE_HPP

#include <openvdb/openvdb.h>

#include <functional>
#include <memory>
#include <vector>

#include "components/PhysicsComponents.hpp"

// --------------------- TerrainStorage Repo ---------------------
class TerrainStorage {
   public:
    // Configuration metadata
    int bgTerrainType = -2;  // -2 = off nodes, -1 = terrain exists but no enTT entity
    int bgVariant = 0;
    int bgMatter = 0;
    int bgFlags = 0;
    float bgHeat = 0.0f;
    int bgEntityId = -2;        // -2 = off nodes, -1 = terrain exists but no enTT entity
    bool useActiveMask = true;  // If false, entityGrid is the authoritative activity

    // Voxel transform metadata
    double voxelSize = 1.0;

    // ---------------- Grids ----------------
    // Main terrain grid reference (source of truth)
    openvdb::Int32Grid::Ptr terrainGrid;  // non-owning semantic: constructed in VoxelGrid
    // Entity type component grids [This could be a flag system]:
    openvdb::Int32Grid::Ptr mainTypeGrid;  // mainType (0=TERRAIN, 1=PLANT, etc.)
    openvdb::Int32Grid::Ptr subType0Grid;
    openvdb::Int32Grid::Ptr subType1Grid;  // EMPTY=-1, GRASS=0, WATER=1
    // MatterContainer component grids:
    openvdb::Int32Grid::Ptr terrainMatterGrid;  // 0 default
    openvdb::Int32Grid::Ptr waterMatterGrid;    // 0 default
    openvdb::Int32Grid::Ptr vaporMatterGrid;    // 0 default
    openvdb::Int32Grid::Ptr biomassMatterGrid;  // 0 default

    // PhysicsStats component grids:
    // [TODO: Use a map to store default values for terrains! -- this is mostly redundant data]
    openvdb::Int32Grid::Ptr massGrid;      // 0 default
    openvdb::Int32Grid::Ptr maxSpeedGrid;  // 0 default
    openvdb::Int32Grid::Ptr minSpeedGrid;  // 0 default
    openvdb::FloatGrid::Ptr heatGrid;      // 0.0f default

    // Flags -- List of bit flags:
    //      DirectionEnum direction;
    //      bool canStackEntities;
    //      MatterState matterState;
    //      GradientVector gradientVector;
    openvdb::Int32Grid::Ptr flagsGrid;            // 0 default
    openvdb::Int32Grid::Ptr maxLoadCapacityGrid;  // 0 default

    // Cadence
    int pruneInterval = 60;  // ticks
    int lastPruneTick = 0;

    TerrainStorage();
    void initialize();
    void applyTransform(double voxelSize_);

    // Memory usage of all terrain-related grids
    size_t memUsage() const;

    int getTerrainIdIfExists(int x, int y, int z);

    void setTerrainId(int x, int y, int z, int id);
    bool checkIfTerrainExists(int x, int y, int z) const;

    // Entity type component accessors:
    void setTerrainMainType(int x, int y, int z, int terrainType);
    int getTerrainMainType(int x, int y, int z) const;

    void setTerrainSubType0(int x, int y, int z, int subType);
    int getTerrainSubType0(int x, int y, int z) const;

    void setTerrainSubType1(int x, int y, int z, int subType);
    int getTerrainSubType1(int x, int y, int z) const;

    // StructuralIntegrityComponent accessors:
    void setTerrainStructuralIntegrity(int x, int y, int z,
                                       const StructuralIntegrityComponent& sic);
    StructuralIntegrityComponent getTerrainStructuralIntegrity(int x, int y, int z) const;

    // MatterContainer accessors:
    void setTerrainMatter(int x, int y, int z, int amount);
    int getTerrainMatter(int x, int y, int z) const;

    void setTerrainWaterMatter(int x, int y, int z, int amount);
    int getTerrainWaterMatter(int x, int y, int z) const;

    void setTerrainVaporMatter(int x, int y, int z, int amount);
    int getTerrainVaporMatter(int x, int y, int z) const;

    void setTerrainBiomassMatter(int x, int y, int z, int amount);
    int getTerrainBiomassMatter(int x, int y, int z) const;

    // PhysicsStats component accessors:
    void setTerrainMass(int x, int y, int z, int mass);
    int getTerrainMass(int x, int y, int z) const;

    void setTerrainMaxSpeed(int x, int y, int z, int maxSpeed);
    int getTerrainMaxSpeed(int x, int y, int z) const;

    void setTerrainMinSpeed(int x, int y, int z, int minSpeed);
    int getTerrainMinSpeed(int x, int y, int z) const;

    void setTerrainHeat(int x, int y, int z, float heat);
    float getTerrainHeat(int x, int y, int z) const;

    // Flags accessors:
    //      DirectionEnum direction;
    void setTerrainDirection(int x, int y, int z, DirectionEnum direction);
    DirectionEnum getTerrainDirection(int x, int y, int z) const;

    //      bool canStackEntities;
    void setTerrainCanStackEntities(int x, int y, int z, bool canStack);
    bool getTerrainCanStackEntities(int x, int y, int z) const;

    //      MatterState matterState;
    void setTerrainMatterState(int x, int y, int z, MatterState state);
    MatterState getTerrainMatterState(int x, int y, int z) const;

    //      GradientVector gradientVector;
    void setTerrainGradientVector(int x, int y, int z, const GradientVector& gradient);
    GradientVector getTerrainGradientVector(int x, int y, int z) const;

    void setTerrainMaxLoadCapacity(int x, int y, int z, int capacity);
    int getTerrainMaxLoadCapacity(int x, int y, int z) const;

    // TODO: Maybe this should be only internal methods!!
    void setFlagBits(int x, int y, int z, int bits);
    int getFlagBits(int x, int y, int z) const;

    bool isActive(int x, int y, int z) const;
    size_t prune(int currentTick);

    // Delete terrain at a specific voxel
    int deleteTerrain(int x, int y, int z);

    // ================ Iterator Methods ================
    // Efficient full-grid iteration for specific grids
    template <typename Callback>
    void iterateWaterMatter(Callback callback) const;

    template <typename Callback>
    void iterateVaporMatter(Callback callback) const;

    template <typename Callback>
    void iterateBiomassMatter(Callback callback) const;

    // Generic grid iterator - can iterate over any Int32Grid with a predicate
    template <typename Callback>
    void iterateGrid(const openvdb::Int32Grid::Ptr& grid, Callback callback,
                     int minValue = 1) const;

   private:
    // Thread-local accessor cache for fast O(1) get/set
    struct ThreadCache {
        // Which TerrainStorage instance this cache is bound to
        const TerrainStorage* owner = nullptr;

        // Tree identity pointers, to detect when grids change and refresh accessors
        const void* terrainPtr = nullptr;
        const void* mainTypePtr = nullptr;
        const void* subType0Ptr = nullptr;
        const void* subType1Ptr = nullptr;
        const void* terrainMatterPtr = nullptr;
        const void* waterMatterPtr = nullptr;
        const void* vaporMatterPtr = nullptr;
        const void* biomassMatterPtr = nullptr;
        const void* massPtr = nullptr;
        const void* maxSpeedPtr = nullptr;
        const void* minSpeedPtr = nullptr;
        const void* flagsPtr = nullptr;
        const void* maxLoadCapacityPtr = nullptr;
        const void* heatPtr = nullptr;

        // Accessors are not default-constructible; store as pointers and create on demand
        // Main terrain grid reference (source of truth) Accessor:
        std::unique_ptr<openvdb::Int32Grid::Accessor> terrainAcc;

        // Entity type component grids [This could be a flag system] Accessors:
        std::unique_ptr<openvdb::Int32Grid::Accessor> mainTypeAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> subType0Acc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> subType1Acc;

        // MatterContainer component grids Accessors:
        std::unique_ptr<openvdb::Int32Grid::Accessor> terrainMatterAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> waterMatterAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> vaporMatterAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> biomassMatterAcc;

        // PhysicsStats component grids Accessors:
        std::unique_ptr<openvdb::Int32Grid::Accessor> massAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> maxSpeedAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> minSpeedAcc;
        std::unique_ptr<openvdb::FloatGrid::Accessor> heatAcc;

        // Flags and other Accessors:
        std::unique_ptr<openvdb::Int32Grid::Accessor> flagsAcc;
        std::unique_ptr<openvdb::Int32Grid::Accessor> maxLoadCapacityAcc;

        bool configured = false;
    };

    static thread_local ThreadCache s_threadCache;
    void configureThreadCache();
};

// ================ Template Method Implementations ================

template <typename Callback>
void TerrainStorage::iterateGrid(const openvdb::Int32Grid::Ptr& grid, Callback callback,
                                 int minValue) const {
    if (!grid) return;

    for (auto it = grid->cbeginValueOn(); it; ++it) {
        const auto coord = it.getCoord();
        const int amount = it.getValue();
        if (amount >= minValue) {
            callback(coord.x(), coord.y(), coord.z(), amount);
        }
    }
}

template <typename Callback>
void TerrainStorage::iterateWaterMatter(Callback callback) const {
    iterateGrid(waterMatterGrid, callback, 1);
}

template <typename Callback>
void TerrainStorage::iterateVaporMatter(Callback callback) const {
    iterateGrid(vaporMatterGrid, callback, 1);
}

template <typename Callback>
void TerrainStorage::iterateBiomassMatter(Callback callback) const {
    iterateGrid(biomassMatterGrid, callback, 1);
}

#endif  // TERRAIN_STORAGE_HPP
