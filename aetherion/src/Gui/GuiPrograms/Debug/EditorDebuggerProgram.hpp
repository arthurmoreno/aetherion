#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include "../../GuiCore/GuiProgramManager.hpp"
#include "../../../components/core/Command.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <vector>
#include <string>
#include <ctime>

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
