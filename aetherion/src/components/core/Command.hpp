#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#include <string>
#include <string_view>

namespace nb = nanobind;

/**
 * @brief Base Command class with flexible dict-based parameters
 * 
 * This class uses nanobind::dict for maximum flexibility and seamless Python interop.
 * Parameters can be of any type that nanobind supports.
 * 
 * Usage Example (C++):
 * @code
 *   Command cmd("drop_to_world");
 *   cmd["item_index"] = 5;
 *   cmd["world_x"] = 100.0f;
 *   int index = nb::cast<int>(cmd["item_index"]);
 * @endcode
 * 
 * Usage Example (Python):
 * @code
 *   cmd = Command("drop_to_world")
 *   cmd["item_index"] = 5
 *   cmd["world_x"] = 100.0
 *   index = cmd["item_index"]
 * @endcode
 */
class Command {
public:
    /**
     * @brief Construct a new Command object
     * @param type The command type/name (required)
     */
    explicit Command(std::string type) : type_(std::move(type)), params_() {}
    
    /**
     * @brief Default constructor for deserialization
     */
    Command() : type_(), params_() {}

    // ============================================================================
    // Type Accessors
    // ============================================================================

    /**
     * @brief Get the command type
     */
    [[nodiscard]] const std::string& getType() const noexcept { return type_; }
    
    /**
     * @brief Set the command type
     */
    void setType(std::string type) { type_ = std::move(type); }

    // ============================================================================
    // Dict-like Parameter Access (Python-friendly)
    // ============================================================================

    /**
     * @brief Access parameter by key (dict-like access)
     * @param key Parameter key
     * @return Reference to the parameter value
     */
    nb::object operator[](const char* key) {
        return params_[key];
    }

    /**
     * @brief Access parameter by key (const version)
     */
    nb::object operator[](const char* key) const {
        return params_[key];
    }

    /**
     * @brief Check if a parameter exists
     */
    [[nodiscard]] bool hasParam(std::string_view key) const noexcept {
        return params_.contains(key.data());
    }

    /**
     * @brief Get all parameters as dict
     */
    [[nodiscard]] nb::dict& getParams() noexcept { return params_; }
    
    /**
     * @brief Get all parameters as dict (const)
     */
    [[nodiscard]] const nb::dict& getParams() const noexcept { return params_; }

    /**
     * @brief Clear all parameters
     */
    void clearParams() noexcept { params_.clear(); }

    // ============================================================================
    // Helper methods for convenience
    // ============================================================================

    /**
     * @brief Set a parameter value (convenience method)
     */
    template<typename T>
    void setParam(std::string_view key, T&& value) {
        params_[key.data()] = nb::cast(std::forward<T>(value));
    }

    /**
     * @brief Get a parameter value with type checking
     * @tparam T Expected type of the parameter
     * @return The parameter value
     * @throws std::runtime_error or nb::cast_error if parameter doesn't exist or type mismatch
     */
    template<typename T>
    [[nodiscard]] T getParam(std::string_view key) const {
        if (!params_.contains(key.data())) {
            throw std::runtime_error("Parameter '" + std::string(key) + "' not found in command '" + type_ + "'");
        }
        return nb::cast<T>(params_[key.data()]);
    }

    /**
     * @brief Get a parameter value with default fallback
     * @tparam T Expected type of the parameter
     * @param defaultValue Value to return if parameter doesn't exist
     * @return The parameter value or default
     */
    template<typename T>
    [[nodiscard]] T getParamOr(std::string_view key, T defaultValue) const {
        if (!params_.contains(key.data())) {
            return defaultValue;
        }
        try {
            return nb::cast<T>(params_[key.data()]);
        } catch (...) {
            return defaultValue;
        }
    }

    // ============================================================================
    // Validation
    // ============================================================================

    /**
     * @brief Validate that command has required parameters
     * @param requiredParams List of required parameter names
     * @throws std::runtime_error if any required parameter is missing
     */
    void validate(const std::vector<std::string>& requiredParams) const {
        for (const auto& param : requiredParams) {
            if (!hasParam(param)) {
                throw std::runtime_error(
                    "Missing required parameter '" + param + "' in command '" + type_ + "'"
                );
            }
        }
    }

    // ============================================================================
    // Debugging
    // ============================================================================

    /**
     * @brief Get string representation for debugging
     */
    [[nodiscard]] std::string toString() const {
        std::string result = "Command{type='" + type_ + "'";
        if (params_.size() > 0) {
            result += ", params=" + nb::cast<std::string>(nb::repr(params_));
        }
        result += "}";
        return result;
    }

private:
    std::string type_;              ///< Command type identifier
    nb::dict params_;               ///< Command parameters as nanobind dict
};


// ============================================================================
// Specialized Command Types (convenience classes)
// ============================================================================

/**
 * @brief Command for activating GUI programs
 */
class ActivateProgramCommand : public Command {
public:
    explicit ActivateProgramCommand(std::string programId) 
        : Command("activate_program") {
        setParam("program_id", std::move(programId));
    }

    [[nodiscard]] std::string getProgramId() const {
        return getParam<std::string>("program_id");
    }
};

/**
 * @brief Command for dropping items to world
 */
class DropToWorldCommand : public Command {
public:
    DropToWorldCommand(int itemIndex, std::string srcWindow, float x, float y)
        : Command("drop_to_world") {
        setParam("item_index", itemIndex);
        setParam("src_window", std::move(srcWindow));
        setParam("world_x", x);
        setParam("world_y", y);
    }

    [[nodiscard]] int getItemIndex() const { return getParam<int>("item_index"); }
    [[nodiscard]] std::string getSrcWindow() const { return getParam<std::string>("src_window"); }
    [[nodiscard]] float getWorldX() const { return getParam<float>("world_x"); }
    [[nodiscard]] float getWorldY() const { return getParam<float>("world_y"); }
};

/**
 * @brief Command for editor actions
 */
class EditorCommand : public Command {
public:
    enum class Action { Play, Stop, Step, ExitToEditor };

    explicit EditorCommand(Action action) : Command(std::string(actionToString(action))) {}

    [[nodiscard]] static constexpr std::string_view actionToString(Action action) noexcept {
        switch (action) {
            case Action::Play: return "editor_play";
            case Action::Stop: return "editor_stop";
            case Action::Step: return "editor_step";
            case Action::ExitToEditor: return "exit_to_editor";
        }
        return "unknown_editor_action";
    }
};
