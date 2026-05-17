#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../GuiCore/GuiProgram.hpp"
#include "Gui/GuiStateManager.hpp"

namespace nb = nanobind;

// Forward declare helper function
void RenderPhysicsMetricsWindow(const nb::dict &statistics);

/**
 * @brief Physics metrics visualization program
 *
 * Displays real-time plots of physics event time series and performance
 * statistics using ImPlot.
 */
class PhysicsMetricsProgram : public GuiProgram {
public:
  void render(GuiContext &context) override {
    if (!isActive_)
      return;

    auto *gsm = GuiStateManager::Instance();
    if (ImGui::Begin("Physics Metrics", &isActive_,
                     ImGuiWindowFlags_NoScrollbar)) {
      bool queryEnabled = gsm->getQueryPhysicsMetrics();
      if (ImGui::Checkbox("Query enabled", &queryEnabled)) {
        gsm->setQueryPhysicsMetrics(queryEnabled);
      }
      RenderPhysicsMetricsWindow(context.statistics);
    }
    ImGui::End();
  }

private:
  std::string getId() const override { return "physics_metrics"; }
  std::string getDisplayName() const override { return "Physics Metrics"; }
};
