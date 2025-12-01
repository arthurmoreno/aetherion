#pragma once

#include <string>
#include <deque>
#include <nanobind/nanobind.h>

namespace nb = nanobind;

// TerminalLine structure (needs to be complete for std::deque)
struct TerminalLine {
    std::string text;
    bool isCommand = false;
    bool isError = false;
    
    TerminalLine(const std::string& t, bool cmd = false, bool err = false)
        : text(t), isCommand(cmd), isError(err) {}
};

// Forward declaration for GuiContext
struct GuiContext;

/**
 * @brief Base class for terminal commands
 * 
 * All built-in terminal commands should inherit from this class
 * and implement the execute() method.
 */
class TerminalCommand {
public:
    virtual ~TerminalCommand() = default;
    
    /**
     * @brief Execute the terminal command
     * @param context GUI context for accessing command queue
     * @param terminalBuffer Terminal output buffer to write to
     * @param scrollToBottom Flag to trigger auto-scroll
     */
    virtual void execute(GuiContext& context,
                        std::deque<TerminalLine>& terminalBuffer,
                        bool& scrollToBottom) = 0;
    
    /**
     * @brief Get the command name
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get the command description
     */
    virtual std::string getDescription() const = 0;

protected:
    /**
     * @brief Add a line to the terminal buffer
     */
    void addOutput(std::deque<TerminalLine>& terminalBuffer,
                  const std::string& text, 
                  bool isCommand = false, 
                  bool isError = false);
};
