#ifndef TERRAIN_COMPONENTS_HPP
#define TERRAIN_COMPONENTS_HPP

#include <unordered_set>
#include <vector>

enum struct TerrainEnum { EMPTY = -1, GRASS = 0, WATER = 1 };

enum struct TerrainVariantEnum {
    FULL = 0,
    RAMP_EAST = 1,
    RAMP_WEST = 2,
    CORNER_SOUTH_EAST = 3,
    CORNER_SOUTH_EAST_INV = 4,
    CORNER_NORTH_EAST = 5,
    CORNER_NORTH_EAST_INV = 6,
    RAMP_SOUTH = 7,
    RAMP_NORTH = 8,
    CORNER_SOUTH_WEST = 9,
    CORNER_NORTH_WEST = 10
};

struct MatterContainer {
    int TerrainMatter;
    int WaterVapor;
    int WaterMatter;
    int BioMassMatter;
};

enum struct TileEffectTypeEnum { EMPTY = -1, BLOOD_DAMAGE = 0, GREEN_DAMAGE = 1 };

struct TileEffectComponent {
    int tileEffectType;
    float damageValue;
    int effectTotalTime;
    int effectRemainingTime;
};

struct TileEffectsList {
    std::vector<int> tileEffectsIDs;
    std::unordered_set<int> unique_elements;

    void addEffect(int tileEffectID) {
        if (unique_elements.insert(tileEffectID).second) {
            tileEffectsIDs.push_back(tileEffectID);
        }
    }
};

#endif  // TERRAIN_COMPONENTS_HPP