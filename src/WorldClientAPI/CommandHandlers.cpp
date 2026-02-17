#include "CommandHandlers.hpp"

#include <iostream>

#include "../QueryResponse.hpp"
#include "../components/EntityTypeComponent.hpp"
#include "../components/HealthComponents.hpp"
#include "../components/MetabolismComponents.hpp"
#include "CommandConstants.hpp"

// QueryEntitiesDataHandler implementation
void QueryEntitiesDataHandler::execute(const QueryCommand& cmd, PerceptionResponse& response,
                                       entt::registry& registry, GameDBHandler* dbHandler) {
    auto it = cmd.params.find(std::string(CommandConstants::Params::ENTITY_TYPE_ID));
    if (it == cmd.params.end()) {
        std::cerr << "Error: 'query_entities_data' missing 'entity_type_id' parameter.\n";
        return;
    }

    int entity_type_id = std::stoi(it->second);
    auto mapOfMapsResponse = std::make_shared<MapOfMapsResponse>();

    // Iterate over entities with required components
    auto view =
        registry
            .view<MetabolismComponent, DigestionComponent, HealthComponent, EntityTypeComponent>();

    for (auto entity : view) {
        if (registry.all_of<MetabolismComponent, DigestionComponent, HealthComponent,
                            EntityTypeComponent>(entity)) {
            auto& healthComp = view.get<HealthComponent>(entity);
            int entityId = static_cast<int>(entity);

            auto entityIdString = std::to_string(entityId);
            auto entityHealthLevelString = std::to_string(healthComp.healthLevel);

            // Fill in mapOfMaps data
            mapOfMapsResponse->mapOfMaps[entityIdString] = {
                {"ID", entityIdString}, {"Name", "Squirrel"}, {"Health", entityHealthLevelString}};
        }
    }

    response.queryResponses.emplace(CommandConstants::QUERY_ENTITIES_DATA_RESPONSE_ID,
                                    mapOfMapsResponse);
    std::cout << "Processing 'query_entities_data' with entity_type_id: " << entity_type_id
              << std::endl;
}

bool QueryEntitiesDataHandler::validate(const QueryCommand& cmd, std::string& errorMsg) const {
    auto it = cmd.params.find(std::string(CommandConstants::Params::ENTITY_TYPE_ID));
    if (it == cmd.params.end()) {
        errorMsg = "Missing required parameter 'entity_type_id'";
        return false;
    }
    return true;
}

// GetAIStatisticsHandler implementation
void GetAIStatisticsHandler::execute(const QueryCommand& cmd, PerceptionResponse& response,
                                     entt::registry& registry, GameDBHandler* dbHandler) {
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

    auto mapOfMapsOfDoubleResponse = std::make_shared<MapOfMapsOfDoubleResponse>();

    std::vector<std::string> seriesNames = {
        std::string(CommandConstants::TimeSeriesNames::POPULATION_SIZE),
        std::string(CommandConstants::TimeSeriesNames::INFERENCE_QUEUE_SIZE),
        std::string(CommandConstants::TimeSeriesNames::ACTION_QUEUE_SIZE),
        std::string(CommandConstants::TimeSeriesNames::POPULATION_MEAN),
        std::string(CommandConstants::TimeSeriesNames::POPULATION_MAX),
        std::string(CommandConstants::TimeSeriesNames::POPULATION_MIN)};

    for (const auto& seriesName : seriesNames) {
        addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start, end, dbHandler);
    }

    response.queryResponses.emplace(CommandConstants::GET_AI_STATISTICS_RESPONSE_ID,
                                    mapOfMapsOfDoubleResponse);
}

bool GetAIStatisticsHandler::validate(const QueryCommand& cmd, std::string& errorMsg) const {
    // Optional parameters, so always valid
    return true;
}

void GetAIStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response, const std::string& seriesName,
    uint64_t start, uint64_t end, GameDBHandler* dbHandler) {
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
    for (const auto& pair : result) {
        std::string timestampKey = std::to_string(static_cast<double>(pair.first));
        timeSeriesMap[timestampKey] = pair.second;
    }

    // Add to the response
    response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// GetPhysicsStatisticsHandler implementation
void GetPhysicsStatisticsHandler::execute(const QueryCommand& cmd, PerceptionResponse& response,
                                          entt::registry& registry, GameDBHandler* dbHandler) {
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

    auto mapOfMapsOfDoubleResponse = std::make_shared<MapOfMapsOfDoubleResponse>();

    std::vector<std::string> seriesNames = {
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_MOVE_GAS_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_MOVE_SOLID_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_EVAPORATE_WATER_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_CONDENSE_WATER_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_WATER_FALL_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_WATER_SPREAD),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_WATER_GRAVITY_FLOW),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_TERRAIN_PHASE_CONVERSION),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_CREATION),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_MERGE_UP),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_VAPOR_MERGE_SIDEWAYS),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_ADD_VAPOR_TO_TILE_ABOVE),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_CREATE_VAPOR_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_DELETE_OR_CONVERT_TERRAIN),
        std::string(CommandConstants::TimeSeriesNames::PHYSICS_INVALID_TERRAIN_FOUND)};

    for (const auto& seriesName : seriesNames) {
        addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start, end, dbHandler);
    }

    response.queryResponses.emplace(CommandConstants::GET_PHYSICS_STATISTICS_RESPONSE_ID,
                                    mapOfMapsOfDoubleResponse);
}

bool GetPhysicsStatisticsHandler::validate(const QueryCommand& cmd, std::string& errorMsg) const {
    // Optional parameters, so always valid
    return true;
}

void GetPhysicsStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response, const std::string& seriesName,
    uint64_t start, uint64_t end, GameDBHandler* dbHandler) {
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
    for (const auto& pair : result) {
        std::string timestampKey = std::to_string(static_cast<double>(pair.first));
        timeSeriesMap[timestampKey] = pair.second;
    }

    // Add to the response
    response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// GetLifeStatisticsHandler implementation
void GetLifeStatisticsHandler::execute(const QueryCommand& cmd, PerceptionResponse& response,
                                       entt::registry& registry, GameDBHandler* dbHandler) {
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

    auto mapOfMapsOfDoubleResponse = std::make_shared<MapOfMapsOfDoubleResponse>();

    std::vector<std::string> seriesNames = {
        std::string(CommandConstants::TimeSeriesNames::LIFE_KILL_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::LIFE_SOFT_KILL_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::LIFE_HARD_KILL_ENTITY),
        std::string(CommandConstants::TimeSeriesNames::LIFE_REMOVE_VELOCITY),
        std::string(CommandConstants::TimeSeriesNames::LIFE_REMOVE_MOVING_COMPONENT)};

    for (const auto& seriesName : seriesNames) {
        addTimeSeriesDataToResponse(mapOfMapsOfDoubleResponse, seriesName, start, end, dbHandler);
    }

    response.queryResponses.emplace(CommandConstants::GET_LIFE_STATISTICS_RESPONSE_ID,
                                    mapOfMapsOfDoubleResponse);
}

bool GetLifeStatisticsHandler::validate(const QueryCommand& cmd, std::string& errorMsg) const {
    // Optional parameters, so always valid
    return true;
}

void GetLifeStatisticsHandler::addTimeSeriesDataToResponse(
    std::shared_ptr<MapOfMapsOfDoubleResponse> response, const std::string& seriesName,
    uint64_t start, uint64_t end, GameDBHandler* dbHandler) {
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
    for (const auto& pair : result) {
        std::string timestampKey = std::to_string(static_cast<double>(pair.first));
        timeSeriesMap[timestampKey] = pair.second;
    }

    // Add to the response
    response->mapOfMaps[seriesName] = std::move(timeSeriesMap);
}

// MoveCommandHandler implementation
void MoveCommandHandler::execute(const QueryCommand& cmd, PerceptionResponse& response,
                                 entt::registry& registry, GameDBHandler* dbHandler) {
    int x = 0, y = 0;

    auto itx = cmd.params.find(std::string(CommandConstants::Params::X));
    if (itx != cmd.params.end()) {
        x = std::stoi(itx->second);
    }

    auto ity = cmd.params.find(std::string(CommandConstants::Params::Y));
    if (ity != cmd.params.end()) {
        y = std::stoi(ity->second);
    }

    std::cout << "Processing 'move' command to position (" << x << ", " << y << ")\n";
    // Perform the move operation as needed
}

bool MoveCommandHandler::validate(const QueryCommand& cmd, std::string& errorMsg) const {
    // Optional parameters, so always valid
    return true;
}
