#ifndef COMMAND_VALIDATOR_HPP
#define COMMAND_VALIDATOR_HPP

#include <memory>
#include <string>
#include <unordered_map>

#include "CommandHandlers.hpp"

// Command registry that maps command types to their handlers
class CommandRegistry {
   public:
    CommandRegistry();
    ~CommandRegistry() = default;

    // Get handler for a command type
    ICommandHandler* getHandler(const std::string& commandType) const;

    // Check if a command type is registered
    bool hasHandler(const std::string& commandType) const;

   private:
    void registerHandlers();

    std::unordered_map<std::string, std::unique_ptr<ICommandHandler>> handlers_;
};

#endif  // COMMAND_VALIDATOR_HPP
