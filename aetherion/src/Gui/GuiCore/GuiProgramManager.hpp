#pragma once

#include "ProgramManager.hpp"
#include "GuiProgram.hpp"
#include "../../components/core/GuiContext.hpp"

/**
 * @brief Manages the lifecycle of all GUI programs
 * 
 * Acts as the "process manager" for the GUI OS, maintaining a registry of
 * available programs and handling activation/deactivation. This is a singleton
 * to provide global access throughout the GUI system.
 * 
 * Extends the generic ProgramManager with GUI-specific functionality:
 * - Rendering all active programs with GuiContext
 * - Singleton access pattern for global GUI management
 * 
 * Inherits from ProgramManager<GuiProgram>:
 * - Program registration and lifecycle management
 * - Activation/deactivation with callbacks
 * - Program lookup and enumeration
 */
class GuiProgramManager : public ProgramManager<GuiProgram> {
public:
    /**
     * @brief Get singleton instance
     */
    static GuiProgramManager* Instance() {
        static GuiProgramManager instance;
        return &instance;
    }
    
    /**
     * @brief Render all active GUI programs
     * 
     * Iterates over all registered programs and renders those that are active.
     * Should be called once per frame. This is the GUI-specific functionality
     * that extends the base ProgramManager.
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

private:
    GuiProgramManager() = default;
};
