#include "HelpCommand.hpp"

void HelpCommand::execute(GuiContext& context, std::deque<TerminalLine>& terminalBuffer,
                          bool& scrollToBottom) {
    addOutput(terminalBuffer, "=== Terminal Help ===", false, false);
    addOutput(terminalBuffer, "Available commands:", false, false);

    // Display all registered commands
    for (const auto& cmd : commands_) {
        std::string cmdLine = "  " + cmd->getName();
        // Pad to align descriptions
        while (cmdLine.length() < 20) {
            cmdLine += " ";
        }
        cmdLine += "- " + cmd->getDescription();
        addOutput(terminalBuffer, cmdLine, false, false);
    }

    addOutput(terminalBuffer, "  <command> [params] - Execute custom command", false, false);
    addOutput(terminalBuffer, "", false, false);
    addOutput(terminalBuffer, "Command format: <type> param1=value1 param2=value2 ...", false,
              false);
    addOutput(terminalBuffer, "Navigation: Use Up/Down arrows to navigate history", false, false);
    scrollToBottom = true;
}
