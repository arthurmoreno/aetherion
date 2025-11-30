#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

// Forward declare helper function
void RenderAIStatisticsWindow(const nb::dict& statistics);

/**
 * @brief AI statistics visualization program
 * 
 * Displays real-time plots of AI population metrics, inference queue sizes,
 * and performance statistics using ImPlot.
 */
class AIStatisticsProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("AI Statistics", &isActive_, ImGuiWindowFlags_NoScrollbar)) {
            RenderAIStatisticsWindow(context.statistics);
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "ai_statistics"; }
    std::string getDisplayName() const override { return "AI Statistics"; }
};
