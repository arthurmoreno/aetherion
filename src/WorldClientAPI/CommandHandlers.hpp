#ifndef COMMAND_HANDLERS_HPP
#define COMMAND_HANDLERS_HPP

#include <entt/entt.hpp>
#include <memory>
#include <string>

#include "GameDBHandler.hpp"
#include "PerceptionCore.hpp"
#include "PerceptionResponse.hpp"
#include "QueryCommand.hpp"

// Base interface for command handlers
class ICommandHandler {
   public:
    virtual ~ICommandHandler() = default;

    // Validate command parameters
    virtual bool validate(const QueryCommand& cmd, std::string& errorMsg) const = 0;

    // Execute the command
    virtual void execute(const QueryCommand& cmd, PerceptionResponse& response,
                         entt::registry& registry, GameDBHandler* dbHandler) = 0;
};

// Handler for "query_entities_data" command
class QueryEntitiesDataHandler : public ICommandHandler {
   public:
    bool validate(const QueryCommand& cmd, std::string& errorMsg) const override;
    void execute(const QueryCommand& cmd, PerceptionResponse& response, entt::registry& registry,
                 GameDBHandler* dbHandler) override;
};

// Handler for "get_ai_statistics" command
class GetAIStatisticsHandler : public ICommandHandler {
   public:
    bool validate(const QueryCommand& cmd, std::string& errorMsg) const override;
    void execute(const QueryCommand& cmd, PerceptionResponse& response, entt::registry& registry,
                 GameDBHandler* dbHandler) override;

   private:
    // Helper method to add time series data to response
    void addTimeSeriesDataToResponse(std::shared_ptr<MapOfMapsOfDoubleResponse> response,
                                     const std::string& seriesName, uint64_t start, uint64_t end,
                                     GameDBHandler* dbHandler);
};

// Handler for "get_physics_statistics" command
class GetPhysicsStatisticsHandler : public ICommandHandler {
   public:
    bool validate(const QueryCommand& cmd, std::string& errorMsg) const override;
    void execute(const QueryCommand& cmd, PerceptionResponse& response, entt::registry& registry,
                 GameDBHandler* dbHandler) override;

   private:
    // Helper method to add time series data to response
    void addTimeSeriesDataToResponse(std::shared_ptr<MapOfMapsOfDoubleResponse> response,
                                     const std::string& seriesName, uint64_t start, uint64_t end,
                                     GameDBHandler* dbHandler);
};

// Handler for "get_life_statistics" command
class GetLifeStatisticsHandler : public ICommandHandler {
   public:
    bool validate(const QueryCommand& cmd, std::string& errorMsg) const override;
    void execute(const QueryCommand& cmd, PerceptionResponse& response, entt::registry& registry,
                 GameDBHandler* dbHandler) override;

   private:
    // Helper method to add time series data to response
    void addTimeSeriesDataToResponse(std::shared_ptr<MapOfMapsOfDoubleResponse> response,
                                     const std::string& seriesName, uint64_t start, uint64_t end,
                                     GameDBHandler* dbHandler);
};

// Handler for "move" command
class MoveCommandHandler : public ICommandHandler {
   public:
    bool validate(const QueryCommand& cmd, std::string& errorMsg) const override;
    void execute(const QueryCommand& cmd, PerceptionResponse& response, entt::registry& registry,
                 GameDBHandler* dbHandler) override;
};

#endif  // COMMAND_HANDLERS_HPP
