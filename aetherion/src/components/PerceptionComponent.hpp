#ifndef PERCEPTION_COMPONENT_HPP
#define PERCEPTION_COMPONENT_HPP

#include <iostream>

struct PerceptionComponent {
    int perception_area;    // Horizontal perception area
    int z_perception_area;  // Vertical perception area

    // Getters
    int getPerceptionArea() const { return perception_area; }

    int getZPerceptionArea() const { return z_perception_area; }

    // Setters
    void setPerceptionArea(int area) { perception_area = area; }

    void setZPerceptionArea(int area) { z_perception_area = area; }

    // Print function for debugging
    void print() const {
        std::cout << "Perception Area: " << perception_area
                  << ", Z-Perception Area: " << z_perception_area << std::endl;
    }
};

#endif  // PERCEPTION_COMPONENT_HPP