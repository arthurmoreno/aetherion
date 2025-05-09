#include "GuiStateManager.hpp"

#include <iostream>

// Initialize the static instance pointer to nullptr
GuiStateManager* GuiStateManager::s_pInstance = nullptr;

// Retrieves the singleton instance
GuiStateManager* GuiStateManager::Instance() {
    if (s_pInstance == nullptr) {
        s_pInstance = new GuiStateManager();
    }
    return s_pInstance;
}

// Private constructor
GuiStateManager::GuiStateManager() {
    // Create window instances
    InventoryWindow inventoryWindow;
    HotbarWindow hotbarWindow;
    EquipmentWindow equipmentWindow;

    isDraggingFromUI = false;
    draggedItemIndex = -1;

    waterCameraStats = false;
    terrainCameraStats = false;
    // Initialization code (if any) goes here
    std::cout << "GuiStateManager initialized.";
}

// Private destructor
GuiStateManager::~GuiStateManager() {
    // Cleanup code (if any) goes here
}

bool GuiStateManager::getWaterCameraStats() const { return waterCameraStats; }

bool GuiStateManager::getTerrainCameraStats() const { return terrainCameraStats; }

void GuiStateManager::setWaterCameraStats(const bool waterCameraStats) {
    this->waterCameraStats = waterCameraStats;
}

void GuiStateManager::setTerrainCameraStats(const bool terrainCameraStats) {
    this->terrainCameraStats = terrainCameraStats;
}

// Optional: Load physics settings from a file
bool GuiStateManager::loadSettings(const std::string& fileName) {
    // Implement file loading logic here
    // For example, parse a config file and set gravity and friction
    // Return true if successful, false otherwise
    return false;
}

// Optional: Save physics settings to a file
bool GuiStateManager::saveSettings(const std::string& fileName) const {
    // Implement file saving logic here
    // For example, write gravity and friction to a config file
    // Return true if successful, false otherwise
    return false;
}

const bool getWaterCameraStats() { return GuiStateManager::Instance()->getWaterCameraStats(); }

const bool getTerrainCameraStats() { return GuiStateManager::Instance()->getTerrainCameraStats(); }
