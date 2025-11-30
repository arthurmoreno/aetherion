#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>
#include <algorithm>

namespace nb = nanobind;

// Forward declare helper function
void RenderCameraSettingsWindow(int& CAMERA_SCREEN_WIDTH_ADJUST_OFFSET,
                                int& CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET);

/**
 * @brief Camera settings configuration program
 * 
 * Provides controls for adjusting camera position offsets with both
 * numerical sliders and a visual 2D position plane.
 */
class CameraSettingsProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Camera Settings", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            int CAMERA_SCREEN_WIDTH_ADJUST_OFFSET =
                nb::cast<int>(context.physicsChanges["CAMERA_SCREEN_WIDTH_ADJUST_OFFSET"]);
            int CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET =
                nb::cast<int>(context.physicsChanges["CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET"]);
            
            RenderCameraSettingsWindow(CAMERA_SCREEN_WIDTH_ADJUST_OFFSET,
                                      CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET);
            
            context.physicsChanges["CAMERA_SCREEN_WIDTH_ADJUST_OFFSET"] = CAMERA_SCREEN_WIDTH_ADJUST_OFFSET;
            context.physicsChanges["CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET"] = CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET;
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "camera_settings"; }
    std::string getDisplayName() const override { return "Camera Settings"; }
};
