#include "terrain/TerrainStorage.hpp"

#include <openvdb/openvdb.h>

#include <algorithm>
#include <cmath>

namespace {
// Bit layout for flagsGrid (int32):
//  bits 0-7:   gradient.x as int8 (quantized [-1,1] -> [-127,127])
//  bits 8-15:  gradient.y as int8
//  bits 16-23: gradient.z as int8
//  bits 24-26: direction (3 bits)
//  bit  27:    canStack (1 bit)
//  bits 28-29: matterState (2 bits, stores enum-1)

// Bit field constants for flagsGrid manipulation
namespace FlagBits {
// Gradient vector field (bits 0-23)
constexpr uint32_t GRADIENT_MASK = 0xFFFFFF;  // 24 bits: 0x00FFFFFF
constexpr int GRADIENT_SHIFT = 0;

// Direction field (bits 24-26)
constexpr uint32_t DIRECTION_MASK = 0x7;  // 3 bits: 0x00000007
constexpr int DIRECTION_SHIFT = 24;

// CanStack field (bit 27)
constexpr uint32_t CANSTACK_MASK = 0x1;  // 1 bit: 0x00000001
constexpr int CANSTACK_SHIFT = 27;

// MatterState field (bits 28-29)
constexpr uint32_t MATTERSTATE_MASK = 0x3;  // 2 bits: 0x00000003
constexpr int MATTERSTATE_SHIFT = 28;
}  // namespace FlagBits

inline int8_t quantizeGrad(float v) {
    float clamped = std::clamp(v, -1.0f, 1.0f);
    int q = static_cast<int>(std::round(clamped * 127.0f));
    if (q < -127) q = -127;
    if (q > 127) q = 127;
    return static_cast<int8_t>(q);
}

inline float dequantizeGrad(int8_t q) { return static_cast<float>(q) / 127.0f; }

inline uint32_t packGradToBits(GradientVector g) {
    uint8_t gx = static_cast<uint8_t>(quantizeGrad(g.gx));
    uint8_t gy = static_cast<uint8_t>(quantizeGrad(g.gy));
    uint8_t gz = static_cast<uint8_t>(quantizeGrad(g.gz));
    return static_cast<uint32_t>(gx) | (static_cast<uint32_t>(gy) << 8) |
           (static_cast<uint32_t>(gz) << 16);
}

inline GradientVector unpackGradFromBits(uint32_t bits) {
    uint8_t gx = static_cast<uint8_t>(bits & 0xFF);
    uint8_t gy = static_cast<uint8_t>((bits >> 8) & 0xFF);
    uint8_t gz = static_cast<uint8_t>((bits >> 16) & 0xFF);
    // reinterpret as signed int8
    int8_t sgx = *reinterpret_cast<int8_t*>(&gx);
    int8_t sgy = *reinterpret_cast<int8_t*>(&gy);
    int8_t sgz = *reinterpret_cast<int8_t*>(&gz);
    return GradientVector{dequantizeGrad(sgx), dequantizeGrad(sgy), dequantizeGrad(sgz)};
}

inline uint32_t setBits(uint32_t flags, int shift, uint32_t mask, uint32_t value) {
    flags &= ~(mask << shift);
    flags |= ((value & mask) << shift);
    return flags;
}

inline uint32_t getBits(uint32_t flags, int shift, uint32_t mask) {
    return (flags >> shift) & mask;
}

// Helper functions to decode from flags directly without additional tree lookups
inline bool decodeCanStackEntities(uint32_t flags) {
    return getBits(flags, FlagBits::CANSTACK_SHIFT, FlagBits::CANSTACK_MASK) != 0u;
}

inline GradientVector decodeGradientVector(uint32_t flags) {
    uint32_t gradBits = getBits(flags, FlagBits::GRADIENT_SHIFT, FlagBits::GRADIENT_MASK);
    return unpackGradFromBits(gradBits);
}

inline DirectionEnum decodeDirection(uint32_t flags) {
    uint32_t dirVal = getBits(flags, FlagBits::DIRECTION_SHIFT, FlagBits::DIRECTION_MASK);
    if (dirVal == 0) return DirectionEnum::UP;  // default
    return static_cast<DirectionEnum>(static_cast<int>(dirVal));
}

inline MatterState decodeMatterState(uint32_t flags) {
    uint32_t val = getBits(flags, FlagBits::MATTERSTATE_SHIFT, FlagBits::MATTERSTATE_MASK);
    return static_cast<MatterState>(static_cast<int>(val) + 1);
}

// Helper function to encode StructuralIntegrityComponent into flags
inline uint32_t encodeStructuralIntegrity(const StructuralIntegrityComponent& sic,
                                          uint32_t existingFlags) {
    uint32_t flags = existingFlags;

    // Set canStackEntities (bit 27)
    flags = setBits(flags, FlagBits::CANSTACK_SHIFT, FlagBits::CANSTACK_MASK,
                    sic.canStackEntities ? 1u : 0u);

    // Set gradient vector (bits 0-23)
    flags = setBits(flags, FlagBits::GRADIENT_SHIFT, FlagBits::GRADIENT_MASK,
                    packGradToBits(sic.gradientVector));

    return flags;
}

// Helper functions to encode individual flag components
inline uint32_t encodeDirection(uint32_t flags, DirectionEnum direction) {
    uint32_t dirVal = static_cast<uint32_t>(direction) & FlagBits::DIRECTION_MASK;
    return setBits(flags, FlagBits::DIRECTION_SHIFT, FlagBits::DIRECTION_MASK, dirVal);
}

inline uint32_t encodeCanStackEntities(uint32_t flags, bool canStack) {
    return setBits(flags, FlagBits::CANSTACK_SHIFT, FlagBits::CANSTACK_MASK, canStack ? 1u : 0u);
}

inline uint32_t encodeMatterState(uint32_t flags, MatterState state) {
    int val = static_cast<int>(state) - 1;
    if (val < 0) val = 0;
    if (val > 3) val = 3;
    return setBits(flags, FlagBits::MATTERSTATE_SHIFT, FlagBits::MATTERSTATE_MASK,
                   static_cast<uint32_t>(val));
}

inline uint32_t encodeGradientVector(uint32_t flags, const GradientVector& gradient) {
    // Clear existing gradient bits (0..23) and set
    return setBits(flags, FlagBits::GRADIENT_SHIFT, FlagBits::GRADIENT_MASK,
                   packGradToBits(gradient));
}
}  // namespace

// ------------------ TerrainStorage implementation ------------------

thread_local TerrainStorage::ThreadCache TerrainStorage::s_threadCache;

TerrainStorage::TerrainStorage() {
    // Ensure OpenVDB is initialized when TerrainStorage is used standalone
    // (e.g., in tests that don't create a VoxelGrid, which normally initializes OpenVDB).
    openvdb::initialize();
    // Allocate terrain-related grids (terrainGrid attached externally)
    terrainGrid =
        openvdb::Int32Grid::create(-2);  // -2 = off nodes, -1 = terrain exists but no enTT entity
    // Entity type component grids
    mainTypeGrid = openvdb::Int32Grid::create(0);
    subType0Grid = openvdb::Int32Grid::create(0);
    subType1Grid = openvdb::Int32Grid::create(-1);

    // Matter container grids
    terrainMatterGrid = openvdb::Int32Grid::create(0);
    waterMatterGrid = openvdb::Int32Grid::create(0);
    vaporMatterGrid = openvdb::Int32Grid::create(0);
    biomassMatterGrid = openvdb::Int32Grid::create(0);

    // Physics stats grids
    massGrid = openvdb::Int32Grid::create(0);
    maxSpeedGrid = openvdb::Int32Grid::create(0);
    minSpeedGrid = openvdb::Int32Grid::create(0);

    // Flags and aux grids
    flagsGrid = openvdb::Int32Grid::create(0);
    maxLoadCapacityGrid = openvdb::Int32Grid::create(0);
}

void TerrainStorage::initialize() { applyTransform(voxelSize); }

void TerrainStorage::applyTransform(double voxelSize_) {
    voxelSize = voxelSize_;
    auto xform = openvdb::math::Transform::createLinearTransform(voxelSize);
    if (terrainGrid) terrainGrid->setTransform(xform);
    if (mainTypeGrid) mainTypeGrid->setTransform(xform);
    subType0Grid->setTransform(xform);
    subType1Grid->setTransform(xform);
    terrainMatterGrid->setTransform(xform);
    waterMatterGrid->setTransform(xform);
    vaporMatterGrid->setTransform(xform);
    biomassMatterGrid->setTransform(xform);
    massGrid->setTransform(xform);
    maxSpeedGrid->setTransform(xform);
    minSpeedGrid->setTransform(xform);
    flagsGrid->setTransform(xform);
    maxLoadCapacityGrid->setTransform(xform);
}

size_t TerrainStorage::memUsage() const {
    size_t total = 0;
    total += terrainGrid ? terrainGrid->memUsage() : 0;
    total += mainTypeGrid ? mainTypeGrid->memUsage() : 0;
    total += subType0Grid ? subType0Grid->memUsage() : 0;
    total += subType1Grid ? subType1Grid->memUsage() : 0;
    total += terrainMatterGrid ? terrainMatterGrid->memUsage() : 0;
    total += waterMatterGrid ? waterMatterGrid->memUsage() : 0;
    total += vaporMatterGrid ? vaporMatterGrid->memUsage() : 0;
    total += biomassMatterGrid ? biomassMatterGrid->memUsage() : 0;
    total += massGrid ? massGrid->memUsage() : 0;
    total += maxSpeedGrid ? maxSpeedGrid->memUsage() : 0;
    total += minSpeedGrid ? minSpeedGrid->memUsage() : 0;
    total += flagsGrid ? flagsGrid->memUsage() : 0;
    total += maxLoadCapacityGrid ? maxLoadCapacityGrid->memUsage() : 0;
    return total;
}

void TerrainStorage::configureThreadCache() {
    auto& tc = s_threadCache;

    // If a different TerrainStorage instance is now in use, clear the cache fully
    const bool newOwner = (tc.owner != this);
    if (newOwner) {
        tc.owner = this;
        tc.configured = false;

        // Reset all tree pointers
        tc.terrainPtr = nullptr;
        tc.mainTypePtr = nullptr;
        tc.subType0Ptr = nullptr;
        tc.subType1Ptr = nullptr;
        tc.terrainMatterPtr = nullptr;
        tc.waterMatterPtr = nullptr;
        tc.vaporMatterPtr = nullptr;
        tc.biomassMatterPtr = nullptr;
        tc.massPtr = nullptr;
        tc.maxSpeedPtr = nullptr;
        tc.minSpeedPtr = nullptr;
        tc.flagsPtr = nullptr;
        tc.maxLoadCapacityPtr = nullptr;

        // Reset all accessors to drop stale references into previous instance trees
        tc.terrainAcc.reset();
        tc.mainTypeAcc.reset();
        tc.subType0Acc.reset();
        tc.subType1Acc.reset();
        tc.terrainMatterAcc.reset();
        tc.waterMatterAcc.reset();
        tc.vaporMatterAcc.reset();
        tc.biomassMatterAcc.reset();
        tc.massAcc.reset();
        tc.maxSpeedAcc.reset();
        tc.minSpeedAcc.reset();
        tc.flagsAcc.reset();
        tc.maxLoadCapacityAcc.reset();
    }

    // Collect current tree identity pointers
    const void* tptr = terrainGrid ? static_cast<const void*>(&terrainGrid->tree()) : nullptr;
    const void* mtPtr = static_cast<const void*>(&mainTypeGrid->tree());
    const void* st0Ptr = static_cast<const void*>(&subType0Grid->tree());
    const void* st1Ptr = static_cast<const void*>(&subType1Grid->tree());
    const void* tmPtr = static_cast<const void*>(&terrainMatterGrid->tree());
    const void* wmPtr = static_cast<const void*>(&waterMatterGrid->tree());
    const void* vapPtr = static_cast<const void*>(&vaporMatterGrid->tree());
    const void* bmPtr = static_cast<const void*>(&biomassMatterGrid->tree());
    const void* mPtr = static_cast<const void*>(&massGrid->tree());
    const void* mxPtr = static_cast<const void*>(&maxSpeedGrid->tree());
    const void* mnPtr = static_cast<const void*>(&minSpeedGrid->tree());
    const void* fptr = static_cast<const void*>(&flagsGrid->tree());
    const void* mlcPtr = static_cast<const void*>(&maxLoadCapacityGrid->tree());

    const bool changed = newOwner || tc.terrainPtr != tptr || tc.mainTypePtr != mtPtr ||
                         tc.subType0Ptr != st0Ptr || tc.subType1Ptr != st1Ptr ||
                         tc.terrainMatterPtr != tmPtr || tc.waterMatterPtr != wmPtr ||
                         tc.vaporMatterPtr != vapPtr || tc.biomassMatterPtr != bmPtr ||
                         tc.massPtr != mPtr || tc.maxSpeedPtr != mxPtr || tc.minSpeedPtr != mnPtr ||
                         tc.flagsPtr != fptr || tc.maxLoadCapacityPtr != mlcPtr;

    if (!tc.configured || changed) {
        // Rebuild all accessors from current grids
        if (terrainGrid)
            tc.terrainAcc =
                std::make_unique<openvdb::Int32Grid::Accessor>(terrainGrid->getAccessor());
        tc.mainTypeAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(mainTypeGrid->getAccessor());
        tc.subType0Acc =
            std::make_unique<openvdb::Int32Grid::Accessor>(subType0Grid->getAccessor());
        tc.subType1Acc =
            std::make_unique<openvdb::Int32Grid::Accessor>(subType1Grid->getAccessor());
        tc.terrainMatterAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(terrainMatterGrid->getAccessor());
        tc.waterMatterAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(waterMatterGrid->getAccessor());
        tc.vaporMatterAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(vaporMatterGrid->getAccessor());
        tc.biomassMatterAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(biomassMatterGrid->getAccessor());
        tc.massAcc = std::make_unique<openvdb::Int32Grid::Accessor>(massGrid->getAccessor());
        tc.maxSpeedAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(maxSpeedGrid->getAccessor());
        tc.minSpeedAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(minSpeedGrid->getAccessor());
        tc.flagsAcc = std::make_unique<openvdb::Int32Grid::Accessor>(flagsGrid->getAccessor());
        tc.maxLoadCapacityAcc =
            std::make_unique<openvdb::Int32Grid::Accessor>(maxLoadCapacityGrid->getAccessor());

        // Update identity pointers
        tc.terrainPtr = tptr;
        tc.mainTypePtr = mtPtr;
        tc.subType0Ptr = st0Ptr;
        tc.subType1Ptr = st1Ptr;
        tc.terrainMatterPtr = tmPtr;
        tc.waterMatterPtr = wmPtr;
        tc.vaporMatterPtr = vapPtr;
        tc.biomassMatterPtr = bmPtr;
        tc.massPtr = mPtr;
        tc.maxSpeedPtr = mxPtr;
        tc.minSpeedPtr = mnPtr;
        tc.flagsPtr = fptr;
        tc.maxLoadCapacityPtr = mlcPtr;

        tc.configured = true;
    }
}

// Accessors

void TerrainStorage::setFlagBits(int x, int y, int z, int bits) {
    configureThreadCache();
    if (s_threadCache.flagsAcc) {
        s_threadCache.flagsAcc->setValue(openvdb::Coord(x, y, z), bits);
    } else {
        // Fallback, should not generally happen
        flagsGrid->tree().setValue(openvdb::Coord(x, y, z), bits);
    }
}

int TerrainStorage::getFlagBits(int x, int y, int z) const {
    return flagsGrid->tree().getValue(openvdb::Coord(x, y, z));
}


int TerrainStorage::getTerrainIdIfExists(int x, int y, int z) const {
    if (s_threadCache.terrainAcc) {
        int entityId = s_threadCache.terrainAcc->getValue(openvdb::Coord(x, y, z));
        if (entityId != -1 && entityId != -2) {
            return entityId;
        }
        return -2;
    }
    int entityId = terrainGrid->tree().getValue(openvdb::Coord(x, y, z));
    if (entityId != -1 && entityId != -2) {
        return entityId;
    }
    return -2;
}


// ------------------ New Accessors: Entity type components ------------------
void TerrainStorage::setTerrainMainType(int x, int y, int z, int terrainType) {
    if (!mainTypeGrid) return;
    mainTypeGrid->tree().setValue(openvdb::Coord(x, y, z), terrainType);
}

int TerrainStorage::getTerrainMainType(int x, int y, int z) const {
    if (!mainTypeGrid) return 0;
    if (s_threadCache.mainTypeAcc) {
        return s_threadCache.mainTypeAcc->getValue(openvdb::Coord(x, y, z));
    }
    return mainTypeGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainSubType0(int x, int y, int z, int subType) {
    if (!subType0Grid) return;
    subType0Grid->tree().setValue(openvdb::Coord(x, y, z), subType);
}

int TerrainStorage::getTerrainSubType0(int x, int y, int z) const {
    if (!subType0Grid) return 0;
    if (s_threadCache.subType0Acc) {
        return s_threadCache.subType0Acc->getValue(openvdb::Coord(x, y, z));
    }
    return subType0Grid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainSubType1(int x, int y, int z, int subType) {
    if (!subType1Grid) return;
    subType1Grid->tree().setValue(openvdb::Coord(x, y, z), subType);
}

int TerrainStorage::getTerrainSubType1(int x, int y, int z) const {
    if (!subType1Grid) return -1;
    if (s_threadCache.subType1Acc) {
        return s_threadCache.subType1Acc->getValue(openvdb::Coord(x, y, z));
    }
    return subType1Grid->tree().getValue(openvdb::Coord(x, y, z));
}

// ------------------ New Accessors: MatterContainer ------------------
void TerrainStorage::setTerrainMatter(int x, int y, int z, int amount) {
    terrainMatterGrid->tree().setValue(openvdb::Coord(x, y, z), amount);
}

int TerrainStorage::getTerrainMatter(int x, int y, int z) const {
    return terrainMatterGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainWaterMatter(int x, int y, int z, int amount) {
    waterMatterGrid->tree().setValue(openvdb::Coord(x, y, z), amount);
}

int TerrainStorage::getTerrainWaterMatter(int x, int y, int z) const {
    return waterMatterGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainVaporMatter(int x, int y, int z, int amount) {
    vaporMatterGrid->tree().setValue(openvdb::Coord(x, y, z), amount);
}

int TerrainStorage::getTerrainVaporMatter(int x, int y, int z) const {
    return vaporMatterGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainBiomassMatter(int x, int y, int z, int amount) {
    biomassMatterGrid->tree().setValue(openvdb::Coord(x, y, z), amount);
}

int TerrainStorage::getTerrainBiomassMatter(int x, int y, int z) const {
    return biomassMatterGrid->tree().getValue(openvdb::Coord(x, y, z));
}

// ------------------ New Accessors: PhysicsStats ------------------
void TerrainStorage::setTerrainMass(int x, int y, int z, int mass) {
    massGrid->tree().setValue(openvdb::Coord(x, y, z), mass);
}

int TerrainStorage::getTerrainMass(int x, int y, int z) const {
    return massGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainMaxSpeed(int x, int y, int z, int maxSpeed) {
    maxSpeedGrid->tree().setValue(openvdb::Coord(x, y, z), maxSpeed);
}

int TerrainStorage::getTerrainMaxSpeed(int x, int y, int z) const {
    return maxSpeedGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void TerrainStorage::setTerrainMinSpeed(int x, int y, int z, int minSpeed) {
    minSpeedGrid->tree().setValue(openvdb::Coord(x, y, z), minSpeed);
}

int TerrainStorage::getTerrainMinSpeed(int x, int y, int z) const {
    return minSpeedGrid->tree().getValue(openvdb::Coord(x, y, z));
}

// ------------------ New Accessors: Flags ------------------
void TerrainStorage::setTerrainDirection(int x, int y, int z, DirectionEnum direction) {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    flags = encodeDirection(flags, direction);
    flagsGrid->tree().setValue(c, static_cast<int>(flags));
}

DirectionEnum TerrainStorage::getTerrainDirection(int x, int y, int z) const {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    return decodeDirection(flags);
}

void TerrainStorage::setTerrainCanStackEntities(int x, int y, int z, bool canStack) {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    flags = encodeCanStackEntities(flags, canStack);
    flagsGrid->tree().setValue(c, static_cast<int>(flags));
}

bool TerrainStorage::getTerrainCanStackEntities(int x, int y, int z) const {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    return decodeCanStackEntities(flags);
}

void TerrainStorage::setTerrainMatterState(int x, int y, int z, MatterState state) {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    flags = encodeMatterState(flags, state);
    flagsGrid->tree().setValue(c, static_cast<int>(flags));
}

MatterState TerrainStorage::getTerrainMatterState(int x, int y, int z) const {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    return decodeMatterState(flags);
}

void TerrainStorage::setTerrainGradientVector(int x, int y, int z, const GradientVector& gradient) {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    flags = encodeGradientVector(flags, gradient);
    flagsGrid->tree().setValue(c, static_cast<int>(flags));
}

GradientVector TerrainStorage::getTerrainGradientVector(int x, int y, int z) const {
    auto c = openvdb::Coord(x, y, z);
    uint32_t flags = static_cast<uint32_t>(flagsGrid->tree().getValue(c));
    return decodeGradientVector(flags);
}

void TerrainStorage::setTerrainMaxLoadCapacity(int x, int y, int z, int capacity) {
    maxLoadCapacityGrid->tree().setValue(openvdb::Coord(x, y, z), capacity);
}

int TerrainStorage::getTerrainMaxLoadCapacity(int x, int y, int z) const {
    return maxLoadCapacityGrid->tree().getValue(openvdb::Coord(x, y, z));
}

bool TerrainStorage::isActive(int x, int y, int z) const {
    if (terrainGrid) {
        return terrainGrid->tree().getValue(openvdb::Coord(x, y, z)) != bgEntityId;
    }
    return false;
}

size_t TerrainStorage::prune(int currentTick) {
    if (!useActiveMask) {
        size_t count = 0;
        if (terrainGrid) {
            for (auto it = terrainGrid->cbeginValueOn(); it; ++it) {
                if (static_cast<int>(it.getValue()) != bgEntityId) ++count;
            }
        }
        return count;
    }
    if (currentTick - lastPruneTick < pruneInterval) {
        size_t count = 0;
        for (auto it = terrainGrid->cbeginValueOn(); it; ++it) {
            if (it.getValue()) ++count;
        }
        return count;
    }

    // Recompute active mask as union of non-background values
    terrainGrid->clear();
    auto markActive = [&](auto& grid) {
        for (auto it = grid->cbeginValueOn(); it; ++it) {
            terrainGrid->tree().setValue(it.getCoord(), true);
        }
    };
    if (terrainGrid) markActive(terrainGrid);
    markActive(flagsGrid);
    markActive(terrainMatterGrid);
    markActive(waterMatterGrid);
    markActive(vaporMatterGrid);
    markActive(biomassMatterGrid);

    size_t activeCount = 0;
    for (auto it = terrainGrid->cbeginValueOn(); it; ++it) {
        if (it.getValue()) ++activeCount;
    }
    lastPruneTick = currentTick;
    return activeCount;
}

void TerrainStorage::setTerrainId(int x, int y, int z, int id) {
    if (terrainGrid) {
        terrainGrid->tree().setValue(openvdb::Coord(x, y, z), id);
    }
}

bool TerrainStorage::checkIfTerrainExists(int x, int y, int z) const {
    if (!terrainGrid) return false;
    int terrainType = terrainGrid->tree().getValue(openvdb::Coord(x, y, z));
    return terrainType != -2;  // -2 = off nodes (no terrain), -1/-0/+ = terrain exists
}

int TerrainStorage::deleteTerrain(int x, int y, int z) {
    openvdb::Coord coord(x, y, z);

    // Deactivate the voxel in all grids to keep the tree clean
    int terrainId = -2;
    if (terrainGrid) {
        terrainId = terrainGrid->tree().getValue(coord);
        terrainGrid->tree().setValueOff(coord);
    }

    // Also deactivate in all other grids for this coordinate
    mainTypeGrid->tree().setValueOff(coord);
    subType0Grid->tree().setValueOff(coord);
    subType1Grid->tree().setValueOff(coord);
    terrainMatterGrid->tree().setValueOff(coord);
    waterMatterGrid->tree().setValueOff(coord);
    vaporMatterGrid->tree().setValueOff(coord);
    biomassMatterGrid->tree().setValueOff(coord);
    massGrid->tree().setValueOff(coord);
    maxSpeedGrid->tree().setValueOff(coord);
    minSpeedGrid->tree().setValueOff(coord);
    flagsGrid->tree().setValueOff(coord);
    maxLoadCapacityGrid->tree().setValueOff(coord);

    return terrainId;
}

// StructuralIntegrityComponent accessors:
void TerrainStorage::setTerrainStructuralIntegrity(int x, int y, int z,
                                                   const StructuralIntegrityComponent& sic) {
    int encodedFlags = flagsGrid->tree().getValue(openvdb::Coord(x, y, z));
    // Combine the structural integrity data with the existing flags
    flagsGrid->tree().setValue(openvdb::Coord(x, y, z),
                               encodeStructuralIntegrity(sic, encodedFlags));
}

StructuralIntegrityComponent TerrainStorage::getTerrainStructuralIntegrity(int x, int y,
                                                                           int z) const {
    openvdb::Coord coord(x, y, z);
    uint32_t encodedFlags = static_cast<uint32_t>(flagsGrid->tree().getValue(coord));

    StructuralIntegrityComponent sic;
    sic.canStackEntities = decodeCanStackEntities(encodedFlags);
    sic.gradientVector = decodeGradientVector(encodedFlags);
    sic.maxLoadCapacity = maxLoadCapacityGrid->tree().getValue(coord);
    return sic;
}