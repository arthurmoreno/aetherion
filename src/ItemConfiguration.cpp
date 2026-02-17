#include "ItemConfiguration.hpp"

ItemConfiguration::ItemConfiguration(const std::string& id) : itemId(id) {}

void ItemConfiguration::setInGameTextures(const std::vector<std::string>& textures) {
    inGameTextures = textures;
}

void ItemConfiguration::setInventoryTextures(const std::vector<std::string>& textures) {
    inventoryTextures = textures;
}

void ItemConfiguration::setDefaultValue(const std::string& key, const DefaultValue& value) {
    defaultValues[key] = value;
}

const std::string& ItemConfiguration::getItemId() const { return itemId; }

const std::vector<std::string>& ItemConfiguration::getInGameTextures() const {
    return inGameTextures;
}

const std::vector<std::string>& ItemConfiguration::getInventoryTextures() const {
    return inventoryTextures;
}

const ItemConfiguration::DefaultValue* ItemConfiguration::getDefaultValue(
    const std::string& key) const {
    auto it = defaultValues.find(key);
    if (it != defaultValues.end()) {
        return &it->second;
    }
    return nullptr;
}

const float ItemConfiguration::getDefaultValueAsFloat(const std::string& key) {
    auto defaultValuePtr = getDefaultValue(key);
    float value{};
    try {
        // Correct type check
        if (std::holds_alternative<float>(*defaultValuePtr)) {
            value = std::get<float>(*defaultValuePtr);
        } else if (std::holds_alternative<int>(*defaultValuePtr)) {
            value = static_cast<float>(std::get<int>(*defaultValuePtr));
        } else if (std::holds_alternative<double>(*defaultValuePtr)) {
            value = static_cast<float>(std::get<double>(*defaultValuePtr));
        } else {
            throw std::runtime_error("Unhandled variant type for default value!");
        }
    } catch (const std::bad_variant_access& e) {
        throw e.what();
    }

    return value;
}

entt::entity ItemConfiguration::createFoodItem(entt::registry& registry) {
    auto [itemMainType, itemSubType0] = splitStringToInts(itemId);

    if (itemMainType == static_cast<int>(ItemEnum::FOOD)) {
        float energyDensity = this->getDefaultValueAsFloat("energy_density");
        float energyHealthRatio = this->getDefaultValueAsFloat("energy_health_ratio");
        float conversionEfficiency = this->getDefaultValueAsFloat("conversion_efficiency");
        float defaultMass = this->getDefaultValueAsFloat("default_mass");
        float defaultVolume = this->getDefaultValueAsFloat("default_volume");

        auto newFoodItem = registry.create();

        registry.emplace<ItemTypeComponent>(newFoodItem,
                                            ItemTypeComponent{itemMainType, itemSubType0});
        registry.emplace<FoodItem>(newFoodItem,
                                   FoodItem{.energyDensity = energyDensity,
                                            .mass = defaultMass,
                                            .volume = defaultVolume,
                                            .energyHealthRatio = energyHealthRatio,
                                            .convertionEfficiency = conversionEfficiency});
        return newFoodItem;
    } else {
        throw std::runtime_error("This ItemConfiguration instance is not a Food!");
    }
}