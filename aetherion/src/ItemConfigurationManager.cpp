#include "ItemConfigurationManager.hpp"

uintptr_t registerItemConfigurationOnManager(const std::shared_ptr<ItemConfiguration>& config) {
    auto& manager = ItemConfigurationManager::getInstance();

    if (manager.configurations.find(config->getItemId()) != manager.configurations.end()) {
        throw std::runtime_error("ItemConfiguration with ID '" + config->getItemId() +
                                 "' already exists.");
    }

    manager.registerItemConfiguration(config);
    return reinterpret_cast<uintptr_t>(config.get());
}

// Get an item configuration by itemId
std::shared_ptr<ItemConfiguration> getItemConfigurationOnManager(const std::string& itemId) {
    return ItemConfigurationManager::getInstance().getItemConfiguration(itemId);
}

// Deregister an item configuration by itemId
void deregisterItemConfigurationOnManager(const std::string& itemId) {
    ItemConfigurationManager::getInstance().deregisterItemConfiguration(itemId);
}

ItemConfigurationManager& ItemConfigurationManager::getInstance() {
    static ItemConfigurationManager instance;
    return instance;
}

void ItemConfigurationManager::registerItemConfiguration(
    const std::shared_ptr<ItemConfiguration>& config) {
    std::lock_guard<std::mutex> lock(managerMutex);
    configurations[config->getItemId()] = config;
}

std::shared_ptr<ItemConfiguration> ItemConfigurationManager::getItemConfiguration(
    const std::string& itemId) const {
    std::lock_guard<std::mutex> lock(managerMutex);
    auto it = configurations.find(itemId);
    if (it != configurations.end()) {
        return it->second;
    }
    return nullptr;
}

void ItemConfigurationManager::deregisterItemConfiguration(const std::string& itemId) {
    std::lock_guard<std::mutex> lock(managerMutex);
    configurations.erase(itemId);
}

ItemConfigurationManager::~ItemConfigurationManager() { configurations.clear(); }
