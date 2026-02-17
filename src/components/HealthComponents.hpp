#ifndef HEALTH_COMPONENT_HPP
#define HEALTH_COMPONENT_HPP

#include <cmath>
#include <iostream>

// Position component to store an entity's position in 3D space
struct HealthComponent {
    float healthLevel;
    float maxHealth;

    // Print function for debugging
    void print() const {
        std::cout << "HealthComponent(healthLevel: " << healthLevel << ", maxHealth: " << maxHealth
                  << ")" << std::endl;
    }
};

#endif  // HEALTH_COMPONENT_HPP