#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include "Gui/GuiStateManager.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>
#include <vector>

namespace nb = nanobind;

// Forward declarations
struct InventoryItem;
std::vector<InventoryItem> LoadInventory(nb::dict inventoryData);

/**
 * @brief Player inventory management program
 * 
 * Displays the player's inventory with item management capabilities.
 * When closed, automatically shows the hotbar instead.
 */
class InventoryProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Inventory", &isActive_)) {
            std::vector<InventoryItem> items = LoadInventory(context.inventoryData);
            GuiStateManager::Instance()->inventoryWindow.setItems(items);
            GuiStateManager::Instance()->inventoryWindow.setCommands(context.commands);
            GuiStateManager::Instance()->inventoryWindow.Render();
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "inventory"; }
    std::string getDisplayName() const override { return "Inventory"; }
};
