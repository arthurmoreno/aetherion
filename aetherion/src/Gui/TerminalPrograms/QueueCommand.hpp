#pragma once

#include "TerminalCommand.hpp"

/**
 * @brief Queue command - Shows command queue status
 */
class QueueCommand : public TerminalCommand {
public:
    void execute(GuiContext& context,
                std::deque<TerminalLine>& terminalBuffer,
                bool& scrollToBottom) override;
    
    std::string getName() const override { return "queue"; }
    
    std::string getDescription() const override {
        return "Show command queue status";
    }
};
