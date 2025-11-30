#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include "Gui/GuiStateManager.hpp"
#include <imgui.h>
#include <nanobind/nanobind.h>
#include <sstream>
#include <iostream>

namespace nb = nanobind;

/**
 * @brief Console Program - Command-line interface for debugging and system control
 * 
 * Provides a terminal-like interface for entering commands, viewing logs, and
 * monitoring the command queue. Similar to a system console or terminal emulator.
 */
class ConsoleProgram : public GuiProgram {
public:
    ConsoleProgram() = default;
    ~ConsoleProgram() override = default;

    std::string getId() const override { return "console"; }
    std::string getDisplayName() const override { return "Console"; }

    void render(GuiContext& context) override {
        // Normalize colors for semi-transparent background
        auto NormalizeColor = [](int r, int g, int b, float a = 1.0f) -> ImVec4 {
            return ImVec4(static_cast<float>(r) / 255.0f, 
                         static_cast<float>(g) / 255.0f, 
                         static_cast<float>(b) / 255.0f, a);
        };

        ImVec4 color_DarkBlue = NormalizeColor(30, 49, 75);
        ImVec4 semiTransparentBg = NormalizeColor(30, 49, 75, 0.7f);

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4 originalBg = style.Colors[ImGuiCol_WindowBg];
        style.Colors[ImGuiCol_WindowBg] = semiTransparentBg;

        // Begin the console window
        ImGui::Begin("Console", &isActive_,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

        // Create a child region for the console logs with scroll functionality
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), 
                         false, ImGuiWindowFlags_HorizontalScrollbar);

        // Iterate over the consoleLogs Python list and display each log entry
        for (const auto& item : context.consoleLogs) {
            try {
                std::string logEntry = nb::cast<std::string>(item);
                ImGui::TextUnformatted(logEntry.c_str());
            } catch (const nb::cast_error& e) {
                std::string errorMsg = "[Console Error] Failed to display log entry: ";
                errorMsg += e.what();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorMsg.c_str());
            } catch (const std::exception& e) {
                std::string errorMsg = "[Console Error] Unexpected error: ";
                errorMsg += e.what();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorMsg.c_str());
            }
        }

        // Render command buffer status (system-level command queue display)
        renderCommandBufferStatus(context.commands);

        // Auto-scroll to the bottom if the user hasn't scrolled up manually
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        // Add some spacing before the input field
        ImGui::Spacing();

        // Command input field
        if (ImGui::InputText("Command Input", inputBuf_, sizeof(inputBuf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            processCommand(context);
        }

        // Clear button
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            context.consoleLogs.clear();
        }

        ImGui::Text("Enter commands and press Enter to execute.");

        ImGui::End();

        // Restore the previous window background color
        style.Colors[ImGuiCol_WindowBg] = originalBg;
    }

private:
    char inputBuf_[256] = "";  // Static buffer for command input

    /**
     * @brief Render the command buffer status display
     * 
     * Shows pending commands in the command queue, similar to a kernel's
     * command buffer or bash's command history.
     */
    void renderCommandBufferStatus(const nb::list& command_queue) {
        if (command_queue.size() == 0) {
            return;
        }

        // Set text color for system messages
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
        
        ImGui::Text("[System] Processing %zu commands:", command_queue.size());
        
        // Limit display to prevent spam
        constexpr size_t MAX_COMMANDS_DISPLAY = 5;
        const size_t display_count = std::min(command_queue.size(), MAX_COMMANDS_DISPLAY);
        
        for (size_t i = 0; i < display_count; ++i) {
            try {
                if (!nb::isinstance<nb::dict>(command_queue[i])) {
                    continue;
                }
                
                nb::dict cmd = nb::cast<nb::dict>(command_queue[i]);
                
                if (!cmd.contains("type")) {
                    continue;
                }
                
                std::string cmd_type = nb::cast<std::string>(cmd["type"]);
                std::string cmd_repr = "  - Command: " + cmd_type;
                
                if (cmd.contains("params")) {
                    cmd_repr += " (with params)";
                }
                
                ImGui::Text("%s", cmd_repr.c_str());
                
            } catch (const std::exception& e) {
                ImGui::Text("  - [Unable to parse command]");
            }
        }
        
        if (command_queue.size() > display_count) {
            ImGui::Text("  ... and %zu more commands", command_queue.size() - display_count);
        }
        
        ImGui::PopStyleColor();
    }

    /**
     * @brief Process a command entered in the input field
     */
    void processCommand(GuiContext& context) {
        std::string commandStr(inputBuf_);

        // Trim whitespace
        size_t first = commandStr.find_first_not_of(" \t\n\r");
        size_t last = commandStr.find_last_not_of(" \t\n\r");
        if (first != std::string::npos && last != std::string::npos) {
            commandStr = commandStr.substr(first, last - first + 1);
        } else {
            commandStr = "";
        }

        // Process non-empty commands
        if (!commandStr.empty()) {
            try {
                // Parse command: type param1=value1 param2=value2 ...
                std::istringstream iss(commandStr);
                std::string type;
                iss >> type;

                nb::dict params;
                std::string paramPair;
                bool hasParams = false;

                while (iss >> paramPair) {
                    size_t eqPos = paramPair.find('=');
                    if (eqPos != std::string::npos) {
                        hasParams = true;
                        std::string keyStr = paramPair.substr(0, eqPos);
                        nb::str key = nb::str(keyStr.c_str());

                        std::string valueStr = paramPair.substr(eqPos + 1);
                        nb::object value = nb::str(valueStr.c_str());

                        // Try to convert value to appropriate type
                        try {
                            int intValue = std::stoi(valueStr);
                            value = nb::int_(intValue);
                        } catch (...) {}

                        if (nb::isinstance<nb::str>(value)) {
                            try {
                                double floatValue = std::stod(valueStr);
                                value = nb::float_(floatValue);
                            } catch (...) {}
                        }

                        if (nb::isinstance<nb::str>(value)) {
                            if (valueStr == "true" || valueStr == "True") {
                                value = nb::bool_(true);
                            } else if (valueStr == "false" || valueStr == "False") {
                                value = nb::bool_(false);
                            }
                        }

                        params[key] = value;
                    }
                }

                // Create the command dict
                nb::dict command;
                command["type"] = nb::str(type.c_str());
                
                if (hasParams) {
                    command["params"] = params;
                }

                context.commands.append(command);
                context.consoleLogs.append("> " + commandStr);

                // Clear the input buffer
                inputBuf_[0] = '\0';

            } catch (const nb::cast_error& e) {
                std::string errorMsg = "[Console Error] Failed to process command: ";
                errorMsg += e.what();
                context.consoleLogs.append(errorMsg);
                std::cerr << errorMsg << std::endl;
                inputBuf_[0] = '\0';
            } catch (const std::exception& e) {
                std::string errorMsg = "[Console Error] Unexpected error processing command: ";
                errorMsg += e.what();
                context.consoleLogs.append(errorMsg);
                std::cerr << errorMsg << std::endl;
                inputBuf_[0] = '\0';
            }
        }
    }
};
