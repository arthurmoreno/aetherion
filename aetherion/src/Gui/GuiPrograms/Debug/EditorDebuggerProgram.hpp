#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <ctime>
#include <string>
#include <vector>

#include "../../../components/core/Command.hpp"
#include "../../GuiCore/GuiProgram.hpp"
#include "../../GuiCore/GuiProgramManager.hpp"

namespace nb = nanobind;

/**
 * @brief Editor Debugger Program - Control panel for simulation debugging
 *
 * Provides controls for:
 * - Simulation control (Play, Stop, Step, Exit to Editor)
 * - Settings access
 * - Snapshot management for world state capture and analysis
 *
 * Snapshot data is stored in context.sharedData["snapshots"] as a list of snapshot names.
 * Commands are issued to take snapshots, analyze them, or delete them.
 */
class EditorDebuggerProgram : public GuiProgram {
   public:
    EditorDebuggerProgram() = default;
    ~EditorDebuggerProgram() override = default;

    std::string getId() const override { return "editor_debugger"; }
    std::string getDisplayName() const override { return "Editor Debugger Menu"; }

    void render(GuiContext& context) override {
        if (!isActive_) return;

        // Set initial position only on first use, then allow user to move it
        ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Editor Debugger Menu", &isActive_, ImGuiWindowFlags_None)) {
            renderSimulationControl(context);
            ImGui::Separator();
            renderSnapshotDebugger(context);
        }
        ImGui::End();
    }

   private:
    /**
     * @brief Render simulation control buttons
     */
    void renderSimulationControl(GuiContext& context) {
        if (ImGui::CollapsingHeader("Simulation Control", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Play Button
            if (ImGui::Button("Play")) {
                EditorCommand command(EditorCommand::Action::Play);
                context.commands.append(command);
            }

            ImGui::SameLine();

            // Stop Button
            if (ImGui::Button("Stop")) {
                EditorCommand command(EditorCommand::Action::Stop);
                context.commands.append(command);
            }

            ImGui::SameLine();

            // Step Button
            if (ImGui::Button("Step")) {
                EditorCommand command(EditorCommand::Action::Step);
                context.commands.append(command);
            }

            ImGui::SameLine();

            // Exit to Editor Button
            if (ImGui::Button("Exit to Editor")) {
                EditorCommand command(EditorCommand::Action::ExitToEditor);
                context.commands.append(command);
            }

            ImGui::SameLine();

            // Settings Button
            if (ImGui::Button("Settings")) {
                GuiProgramManager::Instance()->toggleProgram("settings");
            }

            // FPS Input (slider + text)
            ImGui::Separator();
            // Ensure FPS value is stored in sharedData so UI stays in sync with the simulation
            int fps = 60;
            if (context.sharedData.contains("desired_fps")) {
                try {
                    fps = nb::cast<int>(context.sharedData["desired_fps"]);
                } catch (...) {
                    context.sharedData["desired_fps"] = 60;
                    fps = 60;
                }
            } else {
                context.sharedData["desired_fps"] = fps;
            }

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Simulation FPS:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            // InputInt for direct editing
            if (ImGui::InputInt("##editor_fps_input", &fps, 1, 10)) {
                if (fps < 1) fps = 1;
                if (fps > 1000) fps = 1000;
                context.sharedData["desired_fps"] = fps;
                Command cmd("set_fps");
                cmd.setParam("fps", fps);
                context.commands.append(cmd);
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(220);
            // Slider keeps the value and input in sync
            if (ImGui::SliderInt("##editor_fps_slider", &fps, 1, 1000)) {
                context.sharedData["desired_fps"] = fps;
                Command cmd("set_fps");
                cmd.setParam("fps", fps);
                context.commands.append(cmd);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("(1-1000)");
        }
    }

    /**
     * @brief Render snapshot debugger section
     */
    void renderSnapshotDebugger(GuiContext& context) {
        if (ImGui::CollapsingHeader("Snapshot Debugger", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Take Snapshot Button
            if (ImGui::Button("Take Snapshot")) {
                // Generate snapshot name with timestamp
                char timestamp[64];
                time_t now = time(nullptr);
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
                std::string snapshotName = std::string("Snapshot ") + timestamp;

                // Issue command to take snapshot
                Command cmd("take_snapshot");
                cmd.setParam("name", snapshotName);
                context.commands.append(cmd);
            }

            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Captures the current world state for analysis");
            }

            ImGui::Spacing();
            ImGui::Text("Captured Snapshots:");
            ImGui::Separator();

            // Get snapshots from shared data
            std::vector<std::string> snapshots;
            if (context.sharedData.contains("snapshots")) {
                try {
                    snapshots = nb::cast<std::vector<std::string>>(context.sharedData["snapshots"]);
                } catch (...) {
                    // If casting fails, initialize empty list
                    context.sharedData["snapshots"] = nb::list();
                }
            }

            // Display list of snapshots in a scrollable region
            ImGui::BeginChild("SnapshotList", ImVec2(0, 200), true);

            if (snapshots.empty()) {
                ImGui::TextDisabled("No snapshots taken yet");
            } else {
                for (size_t i = 0; i < snapshots.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));

                    // Snapshot item - just display it
                    ImGui::Selectable(snapshots[i].c_str());

                    // Context menu for each snapshot
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Delete")) {
                            // Issue command to delete snapshot
                            Command cmd("delete_snapshot");
                            cmd.setParam("name", snapshots[i]);
                            cmd.setParam("index", static_cast<int>(i));
                            context.commands.append(cmd);
                            ImGui::EndPopup();
                            ImGui::PopID();
                            break;
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndChild();
        }
    }
};
