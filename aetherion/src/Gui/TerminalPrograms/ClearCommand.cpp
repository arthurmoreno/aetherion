#include "ClearCommand.hpp"

void ClearCommand::execute(GuiContext& context,
                          std::deque<TerminalLine>& terminalBuffer,
                          bool& scrollToBottom) {
    terminalBuffer.clear();
    addOutput(terminalBuffer, "Terminal cleared. Type 'help' for available commands.", false, false);
    scrollToBottom = true;
}
