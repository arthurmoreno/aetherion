#ifndef PHYSICS_COMPONENT_HPP
#define PHYSICS_COMPONENT_HPP

#include <cmath>
#include <iostream>

struct PhysicsStats {
    float mass;
    float maxSpeed;
    float minSpeed;

    float forceX;
    float forceY;
    float forceZ;

    float heat;

    // Print function for debugging
    void print() const {
        std::cout << "PhysicsStats(mass: " << mass << ", maxSpeed: " << maxSpeed << ")"
                  << std::endl;
    }

    // Getter for mass
    float getPhysicsStats() const { return mass; }

    // Setter for mass
    void setPhysicsStats(float new_mass) { mass = new_mass; }
};

enum struct DirectionEnum { UP = 1, RIGHT = 2, DOWN = 3, LEFT = 4, UPWARD = 5, DOWNWARD = 6 };

enum struct MatterState { SOLID = 1, LIQUID = 2, GAS = 3, PLASMA = 4 };

// Position component to store an entity's position in 3D space
struct Position {
    int x;
    int y;
    int z;

    DirectionEnum direction;

    // Print function for debugging
    void print() const {
        std::cout << "Position(x: " << x << ", y: " << y << ", z: " << z << ")" << std::endl;
    }

    int getDirectionAsInt() { return static_cast<int>(direction); }

    // Calculate distance between two positions
    static float distance(const Position& a, const Position& b) {
        return std::sqrt(std::pow(b.x - a.x, 2) + std::pow(b.y - a.y, 2) + std::pow(b.z - a.z, 2));
    }
};

// Velocity component to store an entity's velocity in 3D space
struct Velocity {
    float vx;
    float vy;
    float vz;

    // Print function for debugging
    void print() const {
        std::cout << "Velocity(vx: " << vx << ", vy: " << vy << ", vz: " << vz << ")" << std::endl;
    }

    // Calculate speed from velocity vector
    float speed() const { return std::sqrt(vx * vx + vy * vy + vz * vz); }
};

struct GradientVector {
    float gx;
    float gy;
    float gz;

    // Print function for debugging
    void print() const {
        std::cout << "GradientVector(gx: " << gx << ", gy: " << gy << ", gz: " << gz << ")"
                  << std::endl;
    }
};

struct StructuralIntegrityComponent {
    bool canStackEntities;
    int maxLoadCapacity;
    // TODO: move this to PhysicisStats ?
    MatterState matterState;
    GradientVector gradientVector;
};

#endif  // PHYSICS_COMPONENT_HPP