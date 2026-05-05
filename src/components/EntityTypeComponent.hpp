#ifndef ENTITY_TYPE_COMPONENT_HPP
#define ENTITY_TYPE_COMPONENT_HPP

#include <cmath>
#include <iostream>

enum class EntityEnum { TERRAIN = 0, PLANT = 1, BEAST = 2, TILE_EFFECT = 3 };

constexpr const char *entityMainTypeToString(EntityEnum e) noexcept {
  switch (e) {
  case EntityEnum::TERRAIN:
    return "TERRAIN";
  case EntityEnum::PLANT:
    return "PLANT";
  case EntityEnum::BEAST:
    return "BEAST";
  case EntityEnum::TILE_EFFECT:
    return "TILE_EFFECT";
  }
  return "UNKNOWN";
}

// Engine-wide storage classification, orthogonal to `EntityEnum` (which
// categorises domain semantics). Answers: *where does this entity live in
// the engine's storage layers?* Used by the `createEntityFromPython`
// dispatcher and (planned) the F1/F2/F3 hook + move guards.
enum class EntityStorageKind {
  // VDB grids only; no EnTT entity. Terrain id grid stores
  // `TerrainIdTypeEnum::ON_GRID_STORAGE` (-1). Plain terrain — water, vapor,
  // grass, stone — anything without entity-only components.
  OnGridStorage,

  // VDB grids AND an EnTT entity. Terrain id grid stores the entity int.
  // Terrain that needs entity-only components: `Inventory` (chests) or
  // `TileEffectsList` (torches). Terrain-state is dual-written.
  EntityBackedTerrain,

  // EnTT entity in the entity grid only; terrain grid at this voxel is
  // `TerrainIdTypeEnum::NONE`. Beasts, plants, items.
  EntityOnly,
};

constexpr const char *entityStorageKindToString(EntityStorageKind k) noexcept {
  switch (k) {
  case EntityStorageKind::OnGridStorage:
    return "OnGridStorage";
  case EntityStorageKind::EntityBackedTerrain:
    return "EntityBackedTerrain";
  case EntityStorageKind::EntityOnly:
    return "EntityOnly";
  }
  return "UNKNOWN";
}

// Position component to store an entity's position in 3D space
struct EntityTypeComponent {
  int mainType;
  int subType0;
  int subType1;

  // Print function for debugging
  void print() const {
    std::cout << "EntityTypeComponent(type: " << mainType
              << ", subType0: " << subType0 << ")" << std::endl;
  }
};

#endif // ENTITY_TYPE_COMPONENT_HPP