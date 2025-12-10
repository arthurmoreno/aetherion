#pragma once

#include "../../components/core/GuiContext.hpp"
#include "ProgramManager.hpp"
#include "TerminalProgram.hpp"

/**
 * @brief Manages the lifecycle of all terminal programs
 *
 * Acts as the program manager specifically for terminal-based programs,
 * maintaining a registry of available terminal programs and handling their
 * activation/deactivation. This is a singleton to provide global access
 * throughout the system.
 *
 * Extends the generic ProgramManager with terminal-specific functionality:
 * - Rendering all active terminal programs with GuiContext
 * - Singleton access pattern for global terminal program management
 * - Specialized handling for terminal-based interfaces
 *
 * Inherits from ProgramManager<TerminalProgram>:
 * - Program registration and lifecycle management
 * - Activation/deactivation with callbacks
 * - Program lookup and enumeration
 *
 * Design Philosophy:
 * - Terminal programs are managed separately from GUI programs
 * - Allows for specialized terminal-specific features (e.g., command routing)
 * - Maintains separation of concerns between GUI and terminal interfaces
 */
class TerminalProgramManager : public ProgramManager<TerminalProgram> {
   public:
    /**
     * @brief Get singleton instance
     */
    static TerminalProgramManager* Instance() {
        static TerminalProgramManager instance;
        return &instance;
    }

    /**
     * @brief Render all active terminal programs
     *
     * Iterates over all registered terminal programs and renders those that
     * are active. Should be called once per frame. This is the terminal-specific
     * functionality that extends the base ProgramManager.
     *
     * @param context Shared GUI context for all programs
     */
    void renderAllPrograms(GuiContext& context) {
        for (auto& [id, program] : programs_) {
            if (program->isActive()) {
                program->render(context);
            }
        }
    }

    /**
     * @brief Execute a command in a specific terminal program
     *
     * Routes a command string to a specific terminal program for execution.
     * This allows external systems to send commands to terminal programs.
     *
     * @param programId The terminal program to execute the command in
     * @param command The command string to execute
     * @param context Shared GUI context
     * @return true if command was routed successfully, false if program not found
     */
    bool executeCommandInTerminal(const std::string& programId, const std::string& command,
                                  GuiContext& context) {
        auto program = getProgram(programId);
        if (!program) {
            std::cerr << "[TerminalProgramManager] Warning: Terminal program '" << programId
                      << "' not found for command execution." << std::endl;
            return false;
        }

        // Activate the terminal if not already active
        if (!program->isActive()) {
            activateProgram(programId);
        }

        // Note: Command execution would need to be implemented in TerminalProgram
        // This is a hook for future command routing functionality
        std::cerr << "[TerminalProgramManager] Command routing not yet implemented." << std::endl;
        return true;
    }

    /**
     * @brief Get the currently focused terminal program
     *
     * Returns the terminal program that currently has focus, or nullptr if
     * no terminal is focused. This can be used for global command routing.
     *
     * @return Shared pointer to focused terminal, or nullptr
     */
    std::shared_ptr<TerminalProgram> getFocusedTerminal() {
        // TODO: Implement focus tracking
        // For now, return the first active terminal
        for (auto& [id, program] : programs_) {
            if (program->isActive()) {
                return program;
            }
        }
        return nullptr;
    }

    /**
     * @brief Get all active terminal program IDs
     *
     * Convenience method specifically for terminal programs.
     *
     * @return Vector of active terminal program identifiers
     */
    std::vector<std::string> getActiveTerminalIds() const { return getActiveProgramIds(); }

   private:
    TerminalProgramManager() = default;
};
