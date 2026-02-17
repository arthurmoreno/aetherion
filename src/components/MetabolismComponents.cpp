#include "MetabolismComponents.hpp"

// Adds an item by appending it to the vector
int DigestionComponent::addItem(int itemID, int processingTime, float energyLeft,
                                float energyDensity, float mass, float volume,
                                float convertionEfficiency, float energyHealthRatio) {
    DigestingFoodItem digestingFood;
    digestingFood.foodItemID = itemID;  // Example entity ID
    digestingFood.processingTime = processingTime;
    digestingFood.energyLeft = energyLeft;

    digestingFood.energyDensity = energyDensity;
    digestingFood.mass = mass;
    digestingFood.volume = volume;
    digestingFood.convertionEfficiency = convertionEfficiency;
    digestingFood.energyHealthRatio = energyHealthRatio;

    // Append the itemID to the vector
    digestingItems.push_back(digestingFood);

    // Return the index at which it was added
    return digestingItems.size() - 1;
}

void DigestionComponent::removeItem(int itemID) {
    auto it =
        std::find_if(digestingItems.begin(), digestingItems.end(),
                     [itemID](const DigestingFoodItem& item) { return item.foodItemID == itemID; });

    if (it != digestingItems.end()) {
        digestingItems.erase(it);
        // Optionally, handle any additional cleanup here
    } else {
        // Handle the case where the item is not found
        // For example, throw an exception or log a warning
    }
}