#ifndef ENTITY_TYPE_COMPONENT_HPP
#define ENTITY_TYPE_COMPONENT_HPP

#include <cmath>
#include <iostream>

enum class EntityEnum { TERRAIN = 0, PLANT = 1, BEAST = 2, TILE_EFFECT = 3 };

constexpr const char* entityMainTypeToString(EntityEnum e) noexcept {
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

// Position component to store an entity's position in 3D space
struct EntityTypeComponent {
    int mainType;
    int subType0;
    int subType1;

    // Print function for debugging
    void print() const {
        std::cout << "EntityTypeComponent(type: " << mainType << ", subType0: " << subType0 << ")"
                  << std::endl;
    }
};

#endif  // ENTITY_TYPE_COMPONENT_HPP