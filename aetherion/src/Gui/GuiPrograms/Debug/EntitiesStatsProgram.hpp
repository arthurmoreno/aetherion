#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

// Forward declare helper function
void RenderEntitiesWindow(nb::list& commands, nb::list& entitiesData);

/**
 * @brief Entity statistics and query program
 * 
 * Allows querying entities by type ID and displays results in a table format.
 * Useful for debugging and inspecting game entities.
 */
class EntitiesStatsProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Entities Stats", &isActive_,
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            RenderEntitiesWindow(context.commands, context.entitiesData);
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "entities_stats"; }
    std::string getDisplayName() const override { return "Entities Stats"; }
};
