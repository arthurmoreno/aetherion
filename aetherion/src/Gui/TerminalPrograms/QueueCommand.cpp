#include "QueueCommand.hpp"
#include "../../components/core/GuiContext.hpp"

void QueueCommand::execute(GuiContext& context,
                          std::deque<TerminalLine>& terminalBuffer,
                          bool& scrollToBottom) {
    const nb::list& command_queue = context.commands;
    
    if (command_queue.size() == 0) {
        addOutput(terminalBuffer, "Command queue is empty.", false, false);
        return;
    }

    addOutput(terminalBuffer, "=== Command Queue ===", false, false);
    addOutput(terminalBuffer, "Pending commands: " + std::to_string(command_queue.size()), false, false);
    
    // Limit display to prevent spam
    constexpr size_t MAX_COMMANDS_DISPLAY = 10;
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
            std::string cmd_repr = "  " + std::to_string(i + 1) + ". " + cmd_type;
            
            if (cmd.contains("params")) {
                cmd_repr += " (with params)";
            }
            
            addOutput(terminalBuffer, cmd_repr, false, false);
            
        } catch (const std::exception& e) {
            addOutput(terminalBuffer, "  [Unable to parse command]", false, false);
        }
    }
    
    if (command_queue.size() > display_count) {
        std::string more = "  ... and " + std::to_string(command_queue.size() - display_count) + " more commands";
        addOutput(terminalBuffer, more, false, false);
    }
    
    scrollToBottom = true;
}
