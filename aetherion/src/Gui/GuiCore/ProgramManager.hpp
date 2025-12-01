#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include "BasicProgram.hpp"

/**
 * @brief Generic program manager base class
 * 
 * Provides common program management functionality for any type of program
 * that derives from BasicProgram. This templated base class handles:
 * - Program registration and lifecycle
 * - Activation/deactivation with callbacks
 * - Program lookup and enumeration
 * - State queries
 * 
 * Derived managers can add specific functionality (e.g., rendering for GUI,
 * execution for terminal programs) while reusing the core management logic.
 * 
 * @tparam T Program type (must derive from BasicProgram)
 */
template<typename T>
class ProgramManager {
    static_assert(std::is_base_of<BasicProgram, T>::value, 
                  "T must derive from BasicProgram");

public:
    virtual ~ProgramManager() = default;
    
    /**
     * @brief Register a program
     * 
     * Programs must be registered before they can be activated. Typically called
     * during initialization.
     * 
     * @param program Shared pointer to program instance
     */
    void registerProgram(std::shared_ptr<T> program) {
        if (programs_.count(program->getId())) {
            std::cerr << "[ProgramManager] Warning: Program '" << program->getId() 
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
            std::cerr << "[ProgramManager] Warning: Program '" << programId 
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
     * @brief Get list of all active program IDs
     * 
     * @return Vector of active program identifiers
     */
    std::vector<std::string> getActiveProgramIds() const {
        std::vector<std::string> ids;
        for (const auto& [id, program] : programs_) {
            if (program->isActive()) {
                ids.push_back(id);
            }
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
    std::shared_ptr<T> getProgram(const std::string& programId) {
        if (!programs_.count(programId)) {
            return nullptr;
        }
        return programs_[programId];
    }
    
    /**
     * @brief Get a program by ID (const version)
     * 
     * @param programId Program identifier
     * @return Shared pointer to program, or nullptr if not found
     */
    std::shared_ptr<const T> getProgram(const std::string& programId) const {
        if (!programs_.count(programId)) {
            return nullptr;
        }
        return programs_.at(programId);
    }
    
    /**
     * @brief Get count of registered programs
     * 
     * @return Number of registered programs
     */
    size_t getProgramCount() const {
        return programs_.size();
    }
    
    /**
     * @brief Get count of active programs
     * 
     * @return Number of active programs
     */
    size_t getActiveProgramCount() const {
        size_t count = 0;
        for (const auto& [_, program] : programs_) {
            if (program->isActive()) {
                ++count;
            }
        }
        return count;
    }
    
    /**
     * @brief Clear all registered programs
     * 
     * Deactivates and removes all programs. Use with caution.
     */
    void clearAllPrograms() {
        // Deactivate all programs first
        for (auto& [id, program] : programs_) {
            if (program->isActive()) {
                program->setActive(false);
                program->onDeactivate();
            }
        }
        programs_.clear();
    }

protected:
    std::unordered_map<std::string, std::shared_ptr<T>> programs_;  ///< Program registry
};
