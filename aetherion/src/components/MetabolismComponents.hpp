#ifndef DIGESTION_COMPONENT_HPP
#define DIGESTION_COMPONENT_HPP

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

struct DigestingFoodItem {
    int foodItemID;      // ID in the entt registry for the food entity
    int processingTime;  // Time the food has been processed
    float energyLeft;    // Remaining energy to be extracted from the food

    float energyDensity;
    float mass;
    float volume;
    float energyHealthRatio;
    float convertionEfficiency;
};

struct MetabolismComponent {
    float energyReserve;
    float maxEnergyReserve;
};

struct DigestionComponent {
    std::vector<DigestingFoodItem> digestingItems;
    float sizeOfStomach;

    int addItem(int itemID, int processingTime, float energyLeft, float energyDensity, float mass,
                float volume, float convertionEfficiency, float energyHealthRatio);
    void removeItem(int itemID);

    bool isFull() const;
    bool isEmpty() const;
};

#endif  // DIGESTION_COMPONENT_HPP