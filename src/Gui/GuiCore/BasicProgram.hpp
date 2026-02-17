#pragma once

#include <string>

/**
 * @brief Base class for all programs in the GUI OS
 *
 * Provides the fundamental program interface including lifecycle management,
 * identification, and activation state. All program types (GUI programs,
 * terminal programs, etc.) inherit from this base.
 *
 * Design Philosophy:
 * - Programs are self-contained units with unique identifiers
 * - Programs have activation state and lifecycle hooks
 * - Programs can be activated/deactivated independently
 * - Derived classes implement specific rendering/execution logic
 */
class BasicProgram {
   public:
    virtual ~BasicProgram() = default;

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
