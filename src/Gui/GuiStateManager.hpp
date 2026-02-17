#ifndef GUI_STATE_MANAGER_H
#define GUI_STATE_MANAGER_H

#include <map>
#include <string>

#include "ItemWindow.hpp"

class GuiStateManager {
   public:
    InventoryWindow inventoryWindow;
    HotbarWindow hotbarWindow;
    EquipmentWindow equipmentWindow;

    bool isDraggingFromUI;
    int draggedItemIndex;
    std::string src_window_id;

    // Gadgets
    bool waterCameraStats;
    bool terrainCameraStats;
    bool hoveredEntityInterfaceStats;
    bool selectedEntityInterfaceStats;

    // Retrieves the singleton instance
    static GuiStateManager* Instance();

    bool getWaterCameraStats() const;

    bool getTerrainCameraStats() const;

    bool getHoveredEntityInterfaceStats() const;

    bool getSelectedEntityInterfaceStats() const;

    void setWaterCameraStats(const bool waterCameraStats);

    void setTerrainCameraStats(const bool terrainCameraStats);

    void setHoveredEntityInterfaceStats(const bool hoveredEntityInterfaceStats);

    void setSelectedEntityInterfaceStats(const bool selectedEntityInterfaceStats);

    // Optional: Load physics settings from a file
    bool loadSettings(const std::string& fileName);

    // Optional: Save physics settings to a file
    bool saveSettings(const std::string& fileName) const;

   private:
    // Private constructor to prevent instantiation
    GuiStateManager();

    // Private destructor
    ~GuiStateManager();

    // Delete copy constructor and assignment operator to prevent copying
    GuiStateManager(const GuiStateManager&) = delete;
    GuiStateManager& operator=(const GuiStateManager&) = delete;

    // Static instance pointer
    static GuiStateManager* s_pInstance;

    // Optional: Additional physics parameters can be added here
};

typedef GuiStateManager TheGuiStateManager;

const bool getWaterCameraStats();

const bool getTerrainCameraStats();

#endif  // GUI_STATE_MANAGER_H