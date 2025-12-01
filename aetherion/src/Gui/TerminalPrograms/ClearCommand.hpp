#pragma once

#include "TerminalCommand.hpp"

/**
 * @brief Clear command - Clears the terminal buffer
 */
class ClearCommand : public TerminalCommand {
public:
    void execute(GuiContext& context,
                std::deque<TerminalLine>& terminalBuffer,
                bool& scrollToBottom) override;
    
    std::string getName() const override { return "clear"; }
    
    std::string getDescription() const override {
        return "Clear the terminal";
    }
};
