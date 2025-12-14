#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../GuiCore/GuiProgram.hpp"
#include "../../GuiCore/GuiProgramManager.hpp"

namespace nb = nanobind;

/**
 * @brief Main settings program that opens other settings sub-programs
 *
 * Acts as a "launcher" for other settings-related programs. When buttons
 * are clicked, it activates the corresponding sub-program.
 */
class SettingsProgram : public GuiProgram {
   public:
    void render(GuiContext& context) override {
        if (!isActive_) return;

        if (ImGui::Begin("Settings", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Settings buttons to activate sub-programs
            if (ImGui::Button("Camera Settings")) {
                GuiProgramManager::Instance()->toggleProgram("camera_settings");
            }

            if (ImGui::Button("Physics Settings")) {
                GuiProgramManager::Instance()->toggleProgram("physics_settings");
            }

            if (ImGui::Button("General Metrics")) {
                GuiProgramManager::Instance()->toggleProgram("general_metrics");
            }

            if (ImGui::Button("Player Stats")) {
                nb::dict cmd;
                cmd["type"] = "activate_program";
                cmd["program_id"] = "player_stats";
                context.commands.append(cmd);
            }

            ImGui::Spacing();

            if (ImGui::Button("Entity Interface")) {
                nb::dict cmd;
                cmd["type"] = "activate_program";
                cmd["program_id"] = "entity_interface";
                context.commands.append(cmd);
            }

            if (ImGui::Button("Title Screen")) {
                context.physicsChanges["GOTO_TITLE_SCREEN"] = true;
            }
        }
        ImGui::End();
    }

    std::string getId() const override { return "settings"; }
    std::string getDisplayName() const override { return "Settings"; }
};
