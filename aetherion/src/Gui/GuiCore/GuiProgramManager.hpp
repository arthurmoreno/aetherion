#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include "GuiProgram.hpp"

/**
 * @brief Manages the lifecycle of all GUI programs
 * 
 * Acts as the "process manager" for the GUI OS, maintaining a registry of
 * available programs and handling activation/deactivation. This is a singleton
 * to provide global access throughout the GUI system.
 * 
 * Responsibilities:
 * - Maintain registry of all available programs
 * - Handle program activation/deactivation with lifecycle callbacks
 * - Render all active programs each frame
 * - Provide program lookup and enumeration
 */
class GuiProgramManager {
public:
    /**
     * @brief Get singleton instance
     */
    static GuiProgramManager* Instance() {
        static GuiProgramManager instance;
        return &instance;
    }
    
    /**
     * @brief Register a GUI program
     * 
     * Programs must be registered before they can be activated. Typically called
     * during GUI initialization.
     * 
     * @param program Shared pointer to program instance
     */
    void registerProgram(std::shared_ptr<GuiProgram> program) {
        if (programs_.count(program->getId())) {
            std::cerr << "[GuiProgramManager] Warning: Program '" << program->getId() 
                      << "' already registered. Replacing." << std::endl;
        }
        programs_[program->getId()] = program;
    }
    
    /**
     * @brief Activate a program by ID
     * 
     * Sets the program's active state and calls its onActivate() callback.
     * If the program is already active, this is a no-op.
     * 
     * @param programId Program identifier
     * @return true if program was activated, false if not found
     */
    bool activateProgram(const std::string& programId) {
        if (!programs_.count(programId)) {
            std::cerr << "[GuiProgramManager] Warning: Program '" << programId 
                      << "' not found." << std::endl;
            return false;
        }
        
        auto& program = programs_[programId];
        if (!program->isActive()) {
            program->setActive(true);
            program->onActivate();
        }
        return true;
    }
    
    /**
     * @brief Deactivate a program by ID
     * 
     * Sets the program's active state to false and calls its onDeactivate() callback.
     * 
     * @param programId Program identifier
     * @return true if program was deactivated, false if not found
     */
    bool deactivateProgram(const std::string& programId) {
        if (!programs_.count(programId)) {
            return false;
        }
        
        auto& program = programs_[programId];
        if (program->isActive()) {
            program->setActive(false);
            program->onDeactivate();
        }
        return true;
    }
    
    /**
     * @brief Toggle a program's active state
     * 
     * @param programId Program identifier
     * @return true if program state was toggled, false if not found
     */
    bool toggleProgram(const std::string& programId) {
        if (!programs_.count(programId)) {
            return false;
        }
        
        if (programs_[programId]->isActive()) {
            return deactivateProgram(programId);
        } else {
            return activateProgram(programId);
        }
    }
    
    /**
     * @brief Render all active programs
     * 
     * Iterates over all registered programs and renders those that are active.
     * Should be called once per frame.
     * 
     * @param context Shared context for all programs
     */
    void renderAllPrograms(GuiContext& context) {
        for (auto& [id, program] : programs_) {
            if (program->isActive()) {
                program->render(context);
            }
        }
    }
    
    /**
     * @brief Get list of all program IDs
     * 
     * @return Vector of program identifiers
     */
    std::vector<std::string> getAllProgramIds() const {
        std::vector<std::string> ids;
        ids.reserve(programs_.size());
        for (const auto& [id, _] : programs_) {
            ids.push_back(id);
        }
        return ids;
    }
    
    /**
     * @brief Check if a program is active
     * 
     * @param programId Program identifier
     * @return true if program exists and is active
     */
    bool isProgramActive(const std::string& programId) const {
        if (!programs_.count(programId)) {
            return false;
        }
        return programs_.at(programId)->isActive();
    }
    
    /**
     * @brief Get a program by ID
     * 
     * @param programId Program identifier
     * @return Shared pointer to program, or nullptr if not found
     */
    std::shared_ptr<GuiProgram> getProgram(const std::string& programId) {
        if (!programs_.count(programId)) {
            return nullptr;
        }
        return programs_[programId];
    }
    
private:
    GuiProgramManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<GuiProgram>> programs_;
};
