#pragma once

#include <imgui.h>

#include "../../GuiCore/GuiProgram.hpp"

// Forward declare helper function
void RenderGeneralMetricsWindow(int worldTicks, float availableFps);

/**
 * @brief General game metrics display program
 *
 * Shows performance and world state metrics like FPS and tick count.
 */
class GeneralMetricsProgram : public GuiProgram {
   public:
    void render(GuiContext& context) override {
        if (!isActive_) return;

        if (ImGui::Begin("General Metrics", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderGeneralMetricsWindow(context.worldTicks, context.availableFps);
        }
        ImGui::End();
    }

    std::string getId() const override { return "general_metrics"; }
    std::string getDisplayName() const override { return "General Metrics"; }
};
