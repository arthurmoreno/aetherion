#pragma once

#include <string>
#include <memory>
#include "GuiContext.hpp"

/**
 * @brief Base class for GUI programs in the GUI OS
 * 
 * Each program represents an independent window/feature that can be shown/hidden
 * and rendered independently. Programs are analogous to OS processes - they have
 * lifecycle management, state, and can communicate via the command queue.
 * 
 * Design Philosophy:
 * - Each program is self-contained and owns its rendering logic
 * - Programs communicate via commands (loose coupling)
 * - Programs can be activated/deactivated independently
 * - The GuiProgramManager acts as the "process scheduler"
 */
class GuiProgram {
public:
    virtual ~GuiProgram() = default;
    
    /**
     * @brief Render the program's GUI
     * 
     * Called once per frame if the program is active. The program should render
     * its ImGui window(s) and append any generated commands to context.commands.
     * 
     * @param context Shared context containing all GUI data
     */
    virtual void render(GuiContext& context) = 0;
    
    /**
     * @brief Get the program's unique identifier
     * 
     * Used for program activation/deactivation and inter-program communication.
     * Should be lowercase with underscores (e.g., "physics_settings").
     * 
     * @return Program ID string
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief Get program's display name for UI
     * 
     * Human-readable name shown in menus and window titles.
     * 
     * @return Display name string
     */
    virtual std::string getDisplayName() const = 0;
    
    /**
     * @brief Check if program should be rendered this frame
     * 
     * @return true if program is active and should render
     */
    virtual bool isActive() const { return isActive_; }
    
    /**
     * @brief Set program active state
     * 
     * @param active true to activate program, false to deactivate
     */
    virtual void setActive(bool active) { isActive_ = active; }
    
    /**
     * @brief Called when program is activated
     * 
     * Override to implement custom initialization logic (e.g., loading data,
     * resetting state). Default implementation does nothing.
     */
    virtual void onActivate() {}
    
    /**
     * @brief Called when program is deactivated
     * 
     * Override to implement cleanup logic (e.g., saving state, releasing resources).
     * Default implementation does nothing.
     */
    virtual void onDeactivate() {}
    
protected:
    bool isActive_ = false;  ///< Program activation state
};
