#pragma once

#include <vector>

#include "TerminalCommand.hpp"

/**
 * @brief History command - Shows command history
 */
class HistoryCommand : public TerminalCommand {
   public:
    /**
     * @brief Set the command history reference
     */
    void setHistory(const std::vector<std::string>* history) { history_ = history; }

    void execute(GuiContext& context, std::deque<TerminalLine>& terminalBuffer,
                 bool& scrollToBottom) override;

    std::string getName() const override { return "history"; }

    std::string getDescription() const override { return "Show command history"; }

   private:
    const std::vector<std::string>* history_ = nullptr;
};
