#include "ProcessOptionalQueries.hpp"

#include <iostream>

#include "CommandValidator.hpp"

void _processOptionalQueries(const std::vector<QueryCommand>& commands,
                             PerceptionResponse& response, entt::registry& registry,
                             GameDBHandler* dbHandler) {
    // Create command registry
    static CommandRegistry commandRegistry;

    // Process each command
    for (const auto& cmd : commands) {
        // Get handler for the command type
        ICommandHandler* handler = commandRegistry.getHandler(cmd.type);

        if (handler != nullptr) {
            // Validate command
            std::string errorMsg;
            if (handler->validate(cmd, errorMsg)) {
                // Execute the command
                handler->execute(cmd, response, registry, dbHandler);
            } else {
                std::cerr << "Error: Command validation failed for '" << cmd.type
                          << "': " << errorMsg << std::endl;
            }
        } else {
            // Handle unknown command types
            std::cerr << "Error: Unknown command type '" << cmd.type << "'." << std::endl;
        }
    }
}
