#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include "Gui/GuiStateManager.hpp"
#include "EntityInterface.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>
#include <memory>

namespace nb = nanobind;

// Forward declare helper function
void RenderEntityInterfaceWindow(std::shared_ptr<EntityInterface> entityInterface);

/**
 * @brief Gadgets and debug tools program
 * 
 * Provides toggles for various debug visualizations (water camera stats,
 * terrain stats) and entity inspection tools (hovered/selected entity stats).
 */
class GadgetsProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Gadgets", &isActive_)) {
            bool waterCameraStats = GuiStateManager::Instance()->getWaterCameraStats();
            bool terrainCameraStats = GuiStateManager::Instance()->getTerrainCameraStats();
            bool showHoveredEntitiesStats = GuiStateManager::Instance()->getHoveredEntityInterfaceStats();
            bool showSelectedEntitiesStats = GuiStateManager::Instance()->getSelectedEntityInterfaceStats();
            
            // Water Camera Stats checkbox
            if (ImGui::Checkbox("Water Camera Stats", &waterCameraStats)) {
                GuiStateManager::Instance()->setWaterCameraStats(waterCameraStats);
            }
            
            // Terrain Gradient Camera Stats checkbox
            if (ImGui::Checkbox("Terrain Gradient Camera Stats", &terrainCameraStats)) {
                GuiStateManager::Instance()->setTerrainCameraStats(terrainCameraStats);
            }
            
            // Toggle buttons for entity interface windows
            if (ImGui::Button("Hovered Entity Interface Stats")) {
                showHoveredEntitiesStats = !showHoveredEntitiesStats;
                GuiStateManager::Instance()->setHoveredEntityInterfaceStats(showHoveredEntitiesStats);
            }
            
            if (ImGui::Button("Selected Entity Interface Stats")) {
                showSelectedEntitiesStats = !showSelectedEntitiesStats;
                GuiStateManager::Instance()->setSelectedEntityInterfaceStats(showSelectedEntitiesStats);
            }
            
            // Render entity interface windows if enabled
            if (showHoveredEntitiesStats) {
                if (ImGui::Begin("Hovered Entity Interface", &showHoveredEntitiesStats,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
                    RenderEntityInterfaceWindow(context.hoveredEntityInterfacePtr);
                }
                ImGui::End();
            }
            
            if (showSelectedEntitiesStats) {
                if (ImGui::Begin("Selected Entity Interface", &showSelectedEntitiesStats,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
                    RenderEntityInterfaceWindow(context.selectedEntityInterfacePtr);
                }
                ImGui::End();
            }
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "gadgets"; }
    std::string getDisplayName() const override { return "Gadgets"; }
};
