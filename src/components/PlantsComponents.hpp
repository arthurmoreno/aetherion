#ifndef PLANTS_COMPONENTS_HPP
#define PLANTS_COMPONENTS_HPP

enum class PlantEnum { RASPBERRY = 1 };

struct FruitGrowth {
    int energyNeeded;
    int currentEnergy;
};

struct PlantResources {
    int maxEnergy;
    int currentEnergy;
    float water;
    float bioMacronutrients;
};

#endif  // PLANTS_COMPONENTS_HPP