#ifndef COMMAND_HANDLERS_HPP
#define COMMAND_HANDLERS_HPP

#include <entt/entt.hpp>
#include <memory>
#include <string>

#include "GameDBHandler.hpp"
#include "PerceptionCore.hpp"
#include "PerceptionResponse.hpp"
#include "QueryCommand.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Base interface for command handlers
class ICommandHandler {
public:
  virtual ~ICommandHandler() = default;

  // Validate command parameters
  virtual bool validate(const QueryCommand &cmd,
                        std::string &errorMsg) const = 0;

  // Execute the command
  virtual void execute(const QueryCommand &cmd, PerceptionResponse &response,
                       entt::registry &registry, GameDBHandler *dbHandler,
                       VoxelGrid *voxelGrid) = 0;
};

// Handler for "query_entities_data" command
class QueryEntitiesDataHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;
};

// Handler for "get_ai_statistics" command
class GetAIStatisticsHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;

private:
  // Helper method to add time series data to response
  void addTimeSeriesDataToResponse(
      std::shared_ptr<MapOfMapsOfDoubleResponse> response,
      const std::string &seriesName, uint64_t start, uint64_t end,
      GameDBHandler *dbHandler);
};

// Handler for "get_physics_statistics" command
class GetPhysicsStatisticsHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;

private:
  // Helper method to add time series data to response
  void addTimeSeriesDataToResponse(
      std::shared_ptr<MapOfMapsOfDoubleResponse> response,
      const std::string &seriesName, uint64_t start, uint64_t end,
      GameDBHandler *dbHandler);
};

// Handler for "get_life_statistics" command
class GetLifeStatisticsHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;

private:
  // Helper method to add time series data to response
  void addTimeSeriesDataToResponse(
      std::shared_ptr<MapOfMapsOfDoubleResponse> response,
      const std::string &seriesName, uint64_t start, uint64_t end,
      GameDBHandler *dbHandler);
};

// Handler for "move" command
class MoveCommandHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;
};

// Handler for "query_get_entity" command
class GetEntityHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;
};

// Handler for "query_get_terrain" command
class GetTerrainHandler : public ICommandHandler {
public:
  bool validate(const QueryCommand &cmd, std::string &errorMsg) const override;
  void execute(const QueryCommand &cmd, PerceptionResponse &response,
               entt::registry &registry, GameDBHandler *dbHandler,
               VoxelGrid *voxelGrid) override;
};

#endif // COMMAND_HANDLERS_HPP
