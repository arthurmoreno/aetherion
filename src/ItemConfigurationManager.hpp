#ifndef ITEMCONFIGURATIONMANAGER_HPP
#define ITEMCONFIGURATIONMANAGER_HPP

#pragma once
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "ItemConfiguration.hpp"

// Function to load an image and create a texture
uintptr_t registerItemConfigurationOnManager(const std::shared_ptr<ItemConfiguration>& config);

// Function to render the texture
std::shared_ptr<ItemConfiguration> getItemConfigurationOnManager(const std::string& itemId);

// Function to destroy the texture
void deregisterItemConfigurationOnManager(const std::string& itemId);

class ItemConfigurationManager {
   public:
    mutable std::mutex managerMutex;
    std::unordered_map<std::string, std::shared_ptr<ItemConfiguration>> configurations;

    static ItemConfigurationManager& getInstance();

    // Delete copy constructor and assignment operator
    ItemConfigurationManager(const ItemConfigurationManager&) = delete;
    ItemConfigurationManager& operator=(const ItemConfigurationManager&) = delete;

    // Registration methods
    void registerItemConfiguration(const std::shared_ptr<ItemConfiguration>& config);
    std::shared_ptr<ItemConfiguration> getItemConfiguration(const std::string& itemId) const;
    void deregisterItemConfiguration(const std::string& itemId);

   private:
    ItemConfigurationManager() = default;
    ~ItemConfigurationManager();
};

#endif  // ITEMCONFIGURATIONMANAGER_HPP