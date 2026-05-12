#include "CommandHandlers.hpp"

#include <iostream>

#include "../QueryResponse.hpp"
#include "../components/EntityTypeComponent.hpp"
#include "../components/HealthComponents.hpp"
#include "../components/MetabolismComponents.hpp"
#include "../components/PhysicsComponents.hpp"
#include "../components/TerrainComponents.hpp"
#include "../voxelgrid/VoxelGrid.hpp"
#include "CommandConstants.hpp"

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

// QueryEntitiesDataHandler implementation
void QueryEntitiesDataHandler::execute(const QueryCommand &cmd,
                                       PerceptionResponse &response,
                                       entt::registry &registry,
                                       GameDBHandler *dbHandler,
                                       VoxelGrid * /*voxelGrid*/) {
  auto it =
      cmd.params.find(std::string(CommandConstants::Params::ENTITY_TYPE_ID));
  if (it == cmd.params.end()) {
    std::cerr
        << "Error: 'query_entities_data' missing 'entity_type_id' parameter.\n";
    return;
  }

  int entity_type_id = std::stoi(it->second);
  auto mapOfMapsResponse = std::make_shared<MapOfMapsResponse>();

  // Iterate over entities with required components
  auto view = registry.view<MetabolismComponent, DigestionComponent,
                            HealthComponent, EntityTypeComponent>();

  for (auto entity : view) {
    if (registry.all_of<MetabolismComponent, DigestionComponent,
                        HealthComponent, EntityTypeComponent>(entity)) {
      auto &healthComp = view.get<HealthComponent>(entity);
      int entityId = static_cast<int>(entity);

      auto entityIdString = std::to_string(entityId);
      auto entityHealthLevelString = std::to_string(healthComp.healthLevel);

      // Fill in mapOfMaps data
      mapOfMapsResponse->mapOfMaps[entityIdString] = {
          {"ID", entityIdString},
          {"Name", "Squirrel"},
          {"Health", entityHealthLevelString}};
    }
  }

  response.queryResponses.emplace(
      CommandConstants::QUERY_ENTITIES_DATA_RESPONSE_ID, mapOfMapsResponse);
  std::cout << "Processing 'query_entities_data' with entity_type_id: "
            << entity_type_id << std::endl;
}

bool QueryEntitiesDataHandler::validate(const QueryCommand &cmd,
                                        std::string &errorMsg) const {
  auto it =
      cmd.params.find(std::string(CommandConstants::Params::ENTITY_TYPE_ID));
  if (it == cmd.params.end()) {
    errorMsg = "Missing required parameter 'entity_type_id'";
    return false;
  }
  return true;
}

// GetAIStatisticsHandler implementation
void GetAIStatisticsHandler::execute(const QueryCommand &cmd,
                                     PerceptionResponse &response,
                                     entt::registry &registry,
                                     GameDBHandler *dbHandler,
                                     VoxelGrid * /*voxelGrid*/) {
  // #ifdef TRACY_ENABLE
  //   ZoneScopedN("optional_query.get_ai_statistics");
  // #endif
  auto itStart = cmd.params.find(std::string(CommandConstants::Params::START));
  long long start = 0;
  if (itStart != cmd.params.end()) {
    start = std::stoll(itStart->second);
  }

  auto itEnd = cmd.params.find(std::string(CommandConstants::Params::END));
  long long end = 0;
  if (itEnd != cmd.params.end()) {
    end = std::stoll(itEnd->second);
  }

  auto mapOfMapsOfDoubleResponse =
      std::make_shared<MapOfMapsOfDoubleResponse>();

  std::vector<std::string> seriesNames = {
      std::string(CommandConstants::TimeSeriesNames::POPULATION_SIZE),
      std::string(CommandConstants::TimeSeriesNames::INFERENCE_QUEUE_SIZE),
      std::string(CommandConstants::TimeSeriesNames::ACTION_QUEUE_SIZE),
      std::string(CommandConstants::TimeSeriesNames::POPULATION_MEAN),
      std::string(CommandConstants::TimeSeriesNames::POPULATION_MAX),
      std::string(CommandConstants::TimeSeriesNames::POPULATION_MIN)};

  for (const auto &seriesName : seriesNames) {
    addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start,
                                end, dbHandler);
  }

  response.queryResponses.emplace(
      CommandConstants::GET_AI_STATISTICS_RESPONSE_ID,
      mapOfMapsOfDoubleResponse);
}

bool GetAIStatisticsHandler::validate(const QueryCommand &cmd,
                                      std::string &errorMsg) const {
  // Optional parameters, so always valid
  return true;
}

void GetAIStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response,
    const std::string &seriesName, uint64_t start, uint64_t end,
    GameDBHandler *dbHandler) {
  // Avoid querying when the series name is empty
  if (seriesName.empty()) {
    return;
  }

  // Query the time series data
  std::vector<std::pair<uint64_t, double>> result =
      dbHandler->queryTimeSeries(seriesName, start, end);

  // Create a nested map for the time series data
  std::map<std::string, double> timeSeriesMap;

  // Fill the inner map with timestamp -> value pairs
  for (const auto &pair : result) {
    std::string timestampKey = std::to_string(static_cast<double>(pair.first));
    timeSeriesMap[timestampKey] = pair.second;
  }

  // Add to the response
  response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// GetPhysicsStatisticsHandler implementation
void GetPhysicsStatisticsHandler::execute(const QueryCommand &cmd,
                                          PerceptionResponse &response,
                                          entt::registry &registry,
                                          GameDBHandler *dbHandler,
                                          VoxelGrid * /*voxelGrid*/) {
  auto itStart = cmd.params.find(std::string(CommandConstants::Params::START));
  long long start = 0;
  if (itStart != cmd.params.end()) {
    start = std::stoll(itStart->second);
  }

  auto itEnd = cmd.params.find(std::string(CommandConstants::Params::END));
  long long end = 0;
  if (itEnd != cmd.params.end()) {
    end = std::stoll(itEnd->second);
  }

  auto mapOfMapsOfDoubleResponse =
      std::make_shared<MapOfMapsOfDoubleResponse>();

  std::vector<std::string> seriesNames = {
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_MOVE_GAS_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_MOVE_SOLID_ENTITY),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_EVAPORATE_WATER_ENTITY),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_CONDENSE_WATER_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_WATER_FALL_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_WATER_SPREAD),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_WATER_GRAVITY_FLOW),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_TERRAIN_PHASE_CONVERSION),
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_CREATION),
      std::string(CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_MERGE_UP),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_MERGE_SIDEWAYS),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_ADD_VAPOR_TO_TILE_ABOVE),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_CREATE_VAPOR_ENTITY),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_DELETE_OR_CONVERT_TERRAIN),
      std::string(
          CommandConstants::TimeSeriesNames::PHYSICS_INVALID_TERRAIN_FOUND)};

  for (const auto &seriesName : seriesNames) {
    addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start,
                                end, dbHandler);
  }

  response.queryResponses.emplace(
      CommandConstants::GET_PHYSICS_STATISTICS_RESPONSE_ID,
      mapOfMapsOfDoubleResponse);
}

bool GetPhysicsStatisticsHandler::validate(const QueryCommand &cmd,
                                           std::string &errorMsg) const {
  // Optional parameters, so always valid
  return true;
}

void GetPhysicsStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response,
    const std::string &seriesName, uint64_t start, uint64_t end,
    GameDBHandler *dbHandler) {
  // Avoid querying when the series name is empty
  if (seriesName.empty()) {
    std::cerr << "Error: Time series name is empty, skipping query.\n";
    return;
  }

  // Query the time series data
  std::vector<std::pair<uint64_t, double>> result =
      dbHandler->queryTimeSeries(seriesName, start, end);

  // Create a nested map for the time series data
  std::map<std::string, double> timeSeriesMap;

  // Fill the inner map with timestamp -> value pairs
  for (const auto &pair : result) {
    std::string timestampKey = std::to_string(static_cast<double>(pair.first));
    timeSeriesMap[timestampKey] = pair.second;
  }

  // Add to the response
  response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// GetLifeStatisticsHandler implementation
void GetLifeStatisticsHandler::execute(const QueryCommand &cmd,
                                       PerceptionResponse &response,
                                       entt::registry &registry,
                                       GameDBHandler *dbHandler,
                                       VoxelGrid * /*voxelGrid*/) {
  auto itStart = cmd.params.find(std::string(CommandConstants::Params::START));
  long long start = 0;
  if (itStart != cmd.params.end()) {
    start = std::stoll(itStart->second);
  }

  auto itEnd = cmd.params.find(std::string(CommandConstants::Params::END));
  long long end = 0;
  if (itEnd != cmd.params.end()) {
    end = std::stoll(itEnd->second);
  }

  auto mapOfMapsOfDoubleResponse =
      std::make_shared<MapOfMapsOfDoubleResponse>();

  std::vector<std::string> seriesNames = {
      std::string(CommandConstants::TimeSeriesNames::LIFE_KILL_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::LIFE_SOFT_KILL_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::LIFE_HARD_KILL_ENTITY),
      std::string(CommandConstants::TimeSeriesNames::LIFE_REMOVE_VELOCITY),
      std::string(
          CommandConstants::TimeSeriesNames::LIFE_REMOVE_MOVING_COMPONENT)};

  for (const auto &seriesName : seriesNames) {
    addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start,
                                end, dbHandler);
  }

  response.queryResponses.emplace(
      CommandConstants::GET_LIFE_STATISTICS_RESPONSE_ID,
      mapOfMapsOfDoubleResponse);
}

bool GetLifeStatisticsHandler::validate(const QueryCommand &cmd,
                                        std::string &errorMsg) const {
  // Optional parameters, so always valid
  return true;
}

void GetLifeStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response,
    const std::string &seriesName, uint64_t start, uint64_t end,
    GameDBHandler *dbHandler) {
  // Avoid querying when the series name is empty
  if (seriesName.empty()) {
    return;
  }

  // Query the time series data
  std::vector<std::pair<uint64_t, double>> result =
      dbHandler->queryTimeSeries(seriesName, start, end);

  // Create a nested map for the time series data
  std::map<std::string, double> timeSeriesMap;

  // Fill the inner map with timestamp -> value pairs
  for (const auto &pair : result) {
    std::string timestampKey = std::to_string(static_cast<double>(pair.first));
    timeSeriesMap[timestampKey] = pair.second;
  }

  // Add to the response
  response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// MoveCommandHandler implementation
void MoveCommandHandler::execute(const QueryCommand &cmd,
                                 PerceptionResponse &response,
                                 entt::registry &registry,
                                 GameDBHandler *dbHandler,
                                 VoxelGrid * /*voxelGrid*/) {
  int x = 0, y = 0;

  auto itx = cmd.params.find(std::string(CommandConstants::Params::X));
  if (itx != cmd.params.end()) {
    x = std::stoi(itx->second);
  }

  auto ity = cmd.params.find(std::string(CommandConstants::Params::Y));
  if (ity != cmd.params.end()) {
    y = std::stoi(ity->second);
  }

  std::cout << "Processing 'move' command to position (" << x << ", " << y
            << ")\n";
  // Perform the move operation as needed
}

bool MoveCommandHandler::validate(const QueryCommand &cmd,
                                  std::string &errorMsg) const {
  // Optional parameters, so always valid
  return true;
}

// GetEntityHandler implementation
void GetEntityHandler::execute(const QueryCommand &cmd,
                               PerceptionResponse &response,
                               entt::registry &registry,
                               GameDBHandler *dbHandler,
                               VoxelGrid * /*voxelGrid*/) {
#ifdef TRACY_ENABLE
  ZoneScopedN("optional_query.get_entity");
#endif
  int x = std::stoi(cmd.params.at(std::string(CommandConstants::Params::X)));
  int y = std::stoi(cmd.params.at(std::string(CommandConstants::Params::Y)));
  int z = std::stoi(cmd.params.at(std::string(CommandConstants::Params::Z)));

  auto mapOfMapsResponse = std::make_shared<MapOfMapsResponse>();

  auto view = registry.view<Position>();
  bool found = false;

  for (auto entity : view) {
    const auto &pos = view.get<Position>(entity);
    if (pos.x == x && pos.y == y && pos.z == z) {
      int entityId = static_cast<int>(entity);
      std::string idStr = std::to_string(entityId);

      std::map<std::string, std::string> fields;
      fields["ID"] = idStr;
      fields["Position"] = "(" + std::to_string(pos.x) + ", " +
                           std::to_string(pos.y) + ", " +
                           std::to_string(pos.z) + ")";

      if (registry.all_of<EntityTypeComponent>(entity)) {
        const auto &typeComp = registry.get<EntityTypeComponent>(entity);
        fields["EntityType"] = std::to_string(typeComp.mainType) + "/" +
                               std::to_string(typeComp.subType0);
      }

      if (registry.all_of<Velocity>(entity)) {
        const auto &vel = registry.get<Velocity>(entity);
        fields["Velocity"] = "(" + std::to_string(vel.vx) + ", " +
                             std::to_string(vel.vy) + ", " +
                             std::to_string(vel.vz) + ")";
      }

      if (registry.all_of<HealthComponent>(entity)) {
        const auto &healthComp = registry.get<HealthComponent>(entity);
        fields["Health"] = std::to_string(healthComp.healthLevel);
      }

      mapOfMapsResponse->mapOfMaps[idStr] = std::move(fields);
      found = true;
      break;
    }
  }

  if (!found) {
    mapOfMapsResponse->mapOfMaps["no_entity"] = {
        {"Message", "No entity found at (" + std::to_string(x) + ", " +
                        std::to_string(y) + ", " + std::to_string(z) + ")"}};
  }

  response.queryResponses.emplace(
      CommandConstants::QUERY_GET_ENTITY_RESPONSE_ID, mapOfMapsResponse);
}

bool GetEntityHandler::validate(const QueryCommand &cmd,
                                std::string &errorMsg) const {
  if (cmd.params.find(std::string(CommandConstants::Params::X)) ==
          cmd.params.end() ||
      cmd.params.find(std::string(CommandConstants::Params::Y)) ==
          cmd.params.end() ||
      cmd.params.find(std::string(CommandConstants::Params::Z)) ==
          cmd.params.end()) {
    errorMsg = "Missing required parameters 'x', 'y', 'z'";
    return false;
  }
  return true;
}

// GetTerrainHandler implementation
void GetTerrainHandler::execute(const QueryCommand &cmd,
                                PerceptionResponse &response,
                                entt::registry &registry,
                                GameDBHandler * /*dbHandler*/,
                                VoxelGrid *voxelGrid) {
  int x = std::stoi(cmd.params.at(std::string(CommandConstants::Params::X)));
  int y = std::stoi(cmd.params.at(std::string(CommandConstants::Params::Y)));
  int z = std::stoi(cmd.params.at(std::string(CommandConstants::Params::Z)));

  auto mapOfMapsResponse = std::make_shared<MapOfMapsResponse>();

  if (!voxelGrid->checkIfTerrainExists(x, y, z)) {
    mapOfMapsResponse->mapOfMaps["no_terrain"] = {
        {"Message", "No terrain found at (" + std::to_string(x) + ", " +
                        std::to_string(y) + ", " + std::to_string(z) + ")"}};
  } else {
    int realTerrainId = voxelGrid->getTerrain(x, y, z);

    EntityTypeComponent terrainEtc =
        voxelGrid->getTerrainEntityTypeComponent(x, y, z);
    Position pos = voxelGrid->terrainGridRepository->getPosition(x, y, z);
    MatterContainer matterContainer =
        voxelGrid->terrainGridRepository->getTerrainMatterContainer(x, y, z);

    std::map<std::string, std::string> fields;
    fields["RealEntityID"] = std::to_string(realTerrainId);
    fields["EntityTypeMain"] = std::to_string(terrainEtc.mainType);
    fields["EntityTypeSub0"] = std::to_string(terrainEtc.subType0);
    fields["EntityTypeSub1"] = std::to_string(terrainEtc.subType1);
    fields["Position"] = "(" + std::to_string(pos.x) + ", " +
                         std::to_string(pos.y) + ", " + std::to_string(pos.z) +
                         ")";
    fields["TerrainMatter"] = std::to_string(matterContainer.TerrainMatter);
    fields["WaterMatter"] = std::to_string(matterContainer.WaterMatter);
    fields["WaterVapor"] = std::to_string(matterContainer.WaterVapor);
    fields["BioMassMatter"] = std::to_string(matterContainer.BioMassMatter);

    if (realTerrainId > 0) {
      entt::entity terrainEntity = static_cast<entt::entity>(realTerrainId);
      if (registry.valid(terrainEntity) &&
          registry.all_of<TileEffectsList>(terrainEntity)) {
        const TileEffectsList &tel =
            registry.get<TileEffectsList>(terrainEntity);
        fields["TileEffectsCount"] = std::to_string(tel.tileEffectsIDs.size());
      }
    }

    mapOfMapsResponse->mapOfMaps["terrain"] = std::move(fields);
  }

  response.queryResponses.emplace(
      CommandConstants::QUERY_GET_TERRAIN_RESPONSE_ID, mapOfMapsResponse);
}

bool GetTerrainHandler::validate(const QueryCommand &cmd,
                                 std::string &errorMsg) const {
  if (cmd.params.find(std::string(CommandConstants::Params::X)) ==
          cmd.params.end() ||
      cmd.params.find(std::string(CommandConstants::Params::Y)) ==
          cmd.params.end() ||
      cmd.params.find(std::string(CommandConstants::Params::Z)) ==
          cmd.params.end()) {
    errorMsg = "Missing required parameters 'x', 'y', 'z'";
    return false;
  }
  return true;
}
