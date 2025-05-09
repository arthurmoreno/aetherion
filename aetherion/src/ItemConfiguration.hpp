#pragma once
#include <entt/entt.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "components/ItemsComponents.hpp"

class ItemConfiguration {
   public:
    using DefaultValue = std::variant<int, float, double, std::string, bool>;

    ItemConfiguration(const std::string& id);

    // Setters
    void setInGameTextures(const std::vector<std::string>& textures);
    void setInventoryTextures(const std::vector<std::string>& textures);
    void setDefaultValue(const std::string& key, const DefaultValue& value);

    // Getters
    const std::string& getItemId() const;
    const std::vector<std::string>& getInGameTextures() const;
    const std::vector<std::string>& getInventoryTextures() const;
    const DefaultValue* getDefaultValue(const std::string& key) const;
    const float getDefaultValueAsFloat(const std::string& key);

    // Item Creators
    entt::entity createFoodItem(entt::registry& registry);

   private:
    std::string itemId;
    std::vector<std::string> inGameTextures;
    std::vector<std::string> inventoryTextures;
    std::unordered_map<std::string, DefaultValue> defaultValues;
};