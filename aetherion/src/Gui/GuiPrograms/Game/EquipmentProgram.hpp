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
 * @brief Player equipment management program
 * 
 * Displays equipped items (armor, weapons, accessories) with
 * equip/unequip functionality.
 */
class EquipmentProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Equipment", &isActive_)) {
            std::vector<InventoryItem> items = LoadInventory(context.inventoryData);
            GuiStateManager::Instance()->equipmentWindow.setItems(items);
            GuiStateManager::Instance()->equipmentWindow.setCommands(context.commands);
            GuiStateManager::Instance()->equipmentWindow.Render();
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "equipment"; }
    std::string getDisplayName() const override { return "Equipment"; }
};
