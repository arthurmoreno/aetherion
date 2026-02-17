#include "CommandValidator.hpp"

#include "CommandConstants.hpp"

CommandRegistry::CommandRegistry() { registerHandlers(); }

void CommandRegistry::registerHandlers() {
    handlers_[std::string(CommandConstants::QUERY_ENTITIES_DATA)] =
        std::make_unique<QueryEntitiesDataHandler>();
    handlers_[std::string(CommandConstants::GET_AI_STATISTICS)] =
        std::make_unique<GetAIStatisticsHandler>();
    handlers_[std::string(CommandConstants::GET_PHYSICS_STATISTICS)] =
        std::make_unique<GetPhysicsStatisticsHandler>();
    handlers_[std::string(CommandConstants::GET_LIFE_STATISTICS)] =
        std::make_unique<GetLifeStatisticsHandler>();
    handlers_[std::string(CommandConstants::MOVE)] = std::make_unique<MoveCommandHandler>();
}

ICommandHandler* CommandRegistry::getHandler(const std::string& commandType) const {
    auto it = handlers_.find(commandType);
    if (it != handlers_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool CommandRegistry::hasHandler(const std::string& commandType) const {
    return handlers_.find(commandType) != handlers_.end();
}
