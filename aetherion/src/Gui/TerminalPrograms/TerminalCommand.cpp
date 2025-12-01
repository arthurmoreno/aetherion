#include "TerminalCommand.hpp"

void TerminalCommand::addOutput(std::deque<TerminalLine>& terminalBuffer,
                               const std::string& text, 
                               bool isCommand, 
                               bool isError) {
    terminalBuffer.emplace_back(text, isCommand, isError);
    
    // Limit buffer size
    constexpr size_t MAX_TERMINAL_LINES = 1000;
    while (terminalBuffer.size() > MAX_TERMINAL_LINES) {
        terminalBuffer.pop_front();
    }
}
