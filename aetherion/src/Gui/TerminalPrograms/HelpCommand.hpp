#pragma once

#include "TerminalCommand.hpp"
#include <vector>
#include <memory>

/**
 * @brief Help command - Shows available terminal commands and usage information
 */
class HelpCommand : public TerminalCommand {
public:
    /**
     * @brief Set the list of available commands for help display
     */
    void setCommands(const std::vector<std::shared_ptr<TerminalCommand>>& commands) {
        commands_ = commands;
    }
    
    void execute(GuiContext& context,
                std::deque<TerminalLine>& terminalBuffer,
                bool& scrollToBottom) override;
    
    std::string getName() const override { return "help"; }
    
    std::string getDescription() const override {
        return "Show this help message";
    }

private:
    std::vector<std::shared_ptr<TerminalCommand>> commands_;
};
