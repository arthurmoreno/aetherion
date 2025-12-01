#include "HistoryCommand.hpp"

void HistoryCommand::execute(GuiContext& context,
                            std::deque<TerminalLine>& terminalBuffer,
                            bool& scrollToBottom) {
    if (!history_ || history_->empty()) {
        addOutput(terminalBuffer, "No commands in history.", false, false);
        return;
    }

    addOutput(terminalBuffer, "=== Command History ===", false, false);
    for (size_t i = 0; i < history_->size(); ++i) {
        std::string line = std::to_string(i + 1) + "  " + (*history_)[i];
        addOutput(terminalBuffer, line, false, false);
    }
    scrollToBottom = true;
}
