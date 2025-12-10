#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "../../GuiCore/GuiProgram.hpp"
#include "../../TerminalPrograms/ClearCommand.hpp"
#include "../../TerminalPrograms/HelpCommand.hpp"
#include "../../TerminalPrograms/HistoryCommand.hpp"
#include "../../TerminalPrograms/QueueCommand.hpp"
#include "../../TerminalPrograms/TerminalCommand.hpp"
#include "Gui/GuiStateManager.hpp"

namespace nb = nanobind;

/**
 * @brief Console Program - Shell-like terminal interface for debugging and system control
 *
 * Provides a bash/sh-like terminal interface with command history, output display,
 * and command execution. Features include:
 * - Command history navigation (up/down arrows)
 * - Command output display
 * - Scrollable terminal view
 * - Command echoing with prompts
 */
class ConsoleProgram : public GuiProgram {
   public:
    ConsoleProgram() {
        history_.clear();
        historyPos_ = -1;
        initializeCommands();
        clearTerminal();
    }
    ~ConsoleProgram() override = default;

    std::string getId() const override { return "console"; }
    std::string getDisplayName() const override { return "Console"; }

    void render(GuiContext& context) override {
        // Normalize colors for semi-transparent background
        auto NormalizeColor = [](int r, int g, int b, float a = 1.0f) -> ImVec4 {
            return ImVec4(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                          static_cast<float>(b) / 255.0f, a);
        };

        // Terminal-like dark background
        ImVec4 terminalBg = NormalizeColor(20, 20, 20, 0.95f);

        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4 originalBg = style.Colors[ImGuiCol_WindowBg];
        style.Colors[ImGuiCol_WindowBg] = terminalBg;

        // Begin the console window
        ImGui::Begin("Terminal", &isActive_,
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Create a child region for the terminal output with scroll functionality
        ImGui::BeginChild("TerminalScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
                          false, ImGuiWindowFlags_HorizontalScrollbar);

        // Render terminal buffer (command history and output)
        for (const auto& line : terminalBuffer_) {
            if (line.isCommand) {
                // Display command prompt with green prompt color
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
                ImGui::Text("$ ");
                ImGui::PopStyleColor();
                ImGui::SameLine(0, 0);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            } else if (line.isError) {
                // Display error output in red
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            } else {
                // Display normal output
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            }
        }

        // Auto-scroll to the bottom if scrollReclaim is true
        if (scrollToBottom_) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom_ = false;
        }

        ImGui::EndChild();

        // Command input field with shell-like prompt
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        ImGui::Text("$ ");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Set focus to input when console is first opened
        if (reclaimFocus_) {
            ImGui::SetKeyboardFocusHere();
            reclaimFocus_ = false;
        }

        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                         ImGuiInputTextFlags_CallbackHistory |
                                         ImGuiInputTextFlags_CallbackCompletion;

        if (ImGui::InputText("##Input", inputBuf_, sizeof(inputBuf_), inputFlags,
                             &TextEditCallbackStub, (void*)this)) {
            executeCommand(context);
            reclaimFocus_ = true;
        }

        // Buttons for terminal actions
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            auto it = commands_.find("clear");
            if (it != commands_.end()) {
                it->second->execute(context, terminalBuffer_, scrollToBottom_);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Help")) {
            auto it = commands_.find("help");
            if (it != commands_.end()) {
                it->second->execute(context, terminalBuffer_, scrollToBottom_);
            }
        }

        ImGui::End();

        // Restore the previous window background color
        style.Colors[ImGuiCol_WindowBg] = originalBg;
    }

   private:
    char inputBuf_[256] = "";                           // Static buffer for command input
    std::vector<std::string> history_;                  // Command history
    int historyPos_ = -1;                               // Current position in history
    std::deque<TerminalLine> terminalBuffer_;           // Terminal output buffer
    bool scrollToBottom_ = false;                       // Flag to trigger auto-scroll
    bool reclaimFocus_ = false;                         // Flag to reclaim input focus
    static constexpr size_t MAX_TERMINAL_LINES = 1000;  // Maximum lines in terminal buffer

    // Terminal command handlers
    std::map<std::string, std::shared_ptr<TerminalCommand>> commands_;
    std::vector<std::shared_ptr<TerminalCommand>> commandList_;  // For ordered display

    /**
     * @brief Initialize built-in terminal commands
     */
    void initializeCommands() {
        // Create command instances
        auto clearCmd = std::make_shared<ClearCommand>();
        auto helpCmd = std::make_shared<HelpCommand>();
        auto historyCmd = std::make_shared<HistoryCommand>();
        auto queueCmd = std::make_shared<QueueCommand>();

        // Set up command references
        historyCmd->setHistory(&history_);

        // Register commands
        commands_["clear"] = clearCmd;
        commands_["help"] = helpCmd;
        commands_["history"] = historyCmd;
        commands_["queue"] = queueCmd;

        // Build command list for help display
        commandList_ = {clearCmd, helpCmd, historyCmd, queueCmd};
        helpCmd->setCommands(commandList_);
    }

    /**
     * @brief Clear the terminal buffer (used during initialization)
     */
    void clearTerminal() {
        terminalBuffer_.clear();
        addOutput("Terminal cleared. Type 'help' for available commands.", false, false);
        scrollToBottom_ = true;
    }

    /**
     * @brief Add a line to the terminal buffer
     */
    void addOutput(const std::string& text, bool isCommand, bool isError) {
        terminalBuffer_.emplace_back(text, isCommand, isError);

        // Limit buffer size
        while (terminalBuffer_.size() > MAX_TERMINAL_LINES) {
            terminalBuffer_.pop_front();
        }
    }

    /**
     * @brief ImGui callback for input text (handles history navigation)
     */
    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) {
        ConsoleProgram* console = (ConsoleProgram*)data->UserData;
        return console->textEditCallback(data);
    }

    int textEditCallback(ImGuiInputTextCallbackData* data) {
        switch (data->EventFlag) {
            case ImGuiInputTextFlags_CallbackHistory:
                // Navigate command history with up/down arrows
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (historyPos_ == -1) {
                        historyPos_ = static_cast<int>(history_.size()) - 1;
                    } else if (historyPos_ > 0) {
                        historyPos_--;
                    }

                    if (historyPos_ >= 0 && historyPos_ < static_cast<int>(history_.size())) {
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, history_[historyPos_].c_str());
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (historyPos_ != -1) {
                        historyPos_++;
                        if (historyPos_ >= static_cast<int>(history_.size())) {
                            historyPos_ = -1;
                            data->DeleteChars(0, data->BufTextLen);
                        } else {
                            data->DeleteChars(0, data->BufTextLen);
                            data->InsertChars(0, history_[historyPos_].c_str());
                        }
                    }
                }
                break;

            case ImGuiInputTextFlags_CallbackCompletion:
                // Tab completion (future enhancement)
                break;
        }
        return 0;
    }

    /**
     * @brief Execute a command entered in the terminal
     */
    void executeCommand(GuiContext& context) {
        std::string commandStr(inputBuf_);

        // Trim whitespace
        size_t first = commandStr.find_first_not_of(" \t\n\r");
        size_t last = commandStr.find_last_not_of(" \t\n\r");
        if (first != std::string::npos && last != std::string::npos) {
            commandStr = commandStr.substr(first, last - first + 1);
        } else {
            commandStr = "";
        }

        // Clear input buffer
        inputBuf_[0] = '\0';

        // Process non-empty commands
        if (commandStr.empty()) {
            return;
        }

        // Add command to terminal and history
        addOutput(commandStr, true, false);
        history_.push_back(commandStr);
        historyPos_ = -1;

        // Handle built-in commands using command handlers
        auto it = commands_.find(commandStr);
        if (it != commands_.end()) {
            it->second->execute(context, terminalBuffer_, scrollToBottom_);
            return;
        }

        // Parse and execute custom command
        try {
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
                    } catch (...) {
                    }

                    if (nb::isinstance<nb::str>(value)) {
                        try {
                            double floatValue = std::stod(valueStr);
                            value = nb::float_(floatValue);
                        } catch (...) {
                        }
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

            // Display confirmation
            std::string output = "Command queued: " + type;
            if (hasParams) {
                output += " (with parameters)";
            }
            addOutput(output, false, false);

        } catch (const nb::cast_error& e) {
            std::string errorMsg = "Error: Failed to process command - ";
            errorMsg += e.what();
            addOutput(errorMsg, false, true);
            std::cerr << errorMsg << std::endl;
        } catch (const std::exception& e) {
            std::string errorMsg = "Error: Unexpected error - ";
            errorMsg += e.what();
            addOutput(errorMsg, false, true);
            std::cerr << errorMsg << std::endl;
        }

        scrollToBottom_ = true;
    }
};
