#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../GuiCore/GuiProgram.hpp"
#include "Gui/GuiStateManager.hpp"

namespace nb = nanobind;

// Forward declare helper function
void RenderLifeMetricsWindow(const nb::dict &statistics);

/**
 * @brief Life metrics visualization program
 *
 * Displays real-time plots of life event time series (entity kills, component
 * removals) using ImPlot.
 */
class LifeMetricsProgram : public GuiProgram {
public:
  void render(GuiContext &context) override {
    if (!isActive_)
      return;

    auto *gsm = GuiStateManager::Instance();
    if (ImGui::Begin("Life Metrics", &isActive_,
                     ImGuiWindowFlags_NoScrollbar)) {
      bool queryEnabled = gsm->getQueryLifeMetrics();
      if (ImGui::Checkbox("Query enabled", &queryEnabled)) {
        gsm->setQueryLifeMetrics(queryEnabled);
      }
      RenderLifeMetricsWindow(context.statistics);
    }
    ImGui::End();
  }

private:
  std::string getId() const override { return "life_metrics"; }
  std::string getDisplayName() const override { return "Life Metrics"; }
};
