#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../GuiCore/GuiProgram.hpp"

namespace nb = nanobind;

// Forward declare helper function
void RenderPhysicsMetricsWindow(const nb::dict& statistics);

/**
 * @brief Physics metrics visualization program
 *
 * Displays real-time plots of physics event time series and performance statistics using ImPlot.
 */
class PhysicsMetricsProgram : public GuiProgram {
   public:
    void render(GuiContext& context) override {
        if (!isActive_) return;

        if (ImGui::Begin("Physics Metrics", &isActive_, ImGuiWindowFlags_NoScrollbar)) {
            RenderPhysicsMetricsWindow(context.statistics);
        }
        ImGui::End();
    }

    std::string getId() const override { return "physics_metrics"; }
    std::string getDisplayName() const override { return "Physics Metrics"; }
};
