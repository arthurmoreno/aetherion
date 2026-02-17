#ifndef COMMAND_CONSTANTS_HPP
#define COMMAND_CONSTANTS_HPP

#include <string>

namespace CommandConstants {

// Command type constants
inline const std::string QUERY_ENTITIES_DATA = "query_entities_data";
inline const std::string GET_AI_STATISTICS = "get_ai_statistics";
inline const std::string GET_PHYSICS_STATISTICS = "get_physics_statistics";
inline const std::string GET_LIFE_STATISTICS = "get_life_statistics";
inline const std::string MOVE = "move";

// Response ID constants
constexpr int QUERY_ENTITIES_DATA_RESPONSE_ID = 1;
constexpr int GET_AI_STATISTICS_RESPONSE_ID = 2;
constexpr int GET_PHYSICS_STATISTICS_RESPONSE_ID = 3;
constexpr int GET_LIFE_STATISTICS_RESPONSE_ID = 4;

// Parameter name constants
namespace Params {
inline const std::string ENTITY_TYPE_ID = "entity_type_id";
inline const std::string START = "start";
inline const std::string END = "end";
inline const std::string X = "x";
inline const std::string Y = "y";
}  // namespace Params

// Time series names for AI statistics
namespace TimeSeriesNames {
// AI population statistics
inline const std::string POPULATION_SIZE = "population_size";
inline const std::string INFERENCE_QUEUE_SIZE = "inference_queue_size";
inline const std::string ACTION_QUEUE_SIZE = "action_queue_size";
inline const std::string POPULATION_MEAN = "population_mean";
inline const std::string POPULATION_MAX = "population_max";
inline const std::string POPULATION_MIN = "population_min";

// Physics event time series
inline const std::string PHYSICS_MOVE_GAS_ENTITY = "physics_move_gas_entity";
inline const std::string PHYSICS_MOVE_SOLID_ENTITY = "physics_move_solid_entity";
inline const std::string PHYSICS_EVAPORATE_WATER_ENTITY = "physics_evaporate_water_entity";
inline const std::string PHYSICS_CONDENSE_WATER_ENTITY = "physics_condense_water_entity";
inline const std::string PHYSICS_WATER_FALL_ENTITY = "physics_water_fall_entity";
inline const std::string PHYSICS_WATER_SPREAD = "physics_water_spread";
inline const std::string PHYSICS_WATER_GRAVITY_FLOW = "physics_water_gravity_flow";
inline const std::string PHYSICS_TERRAIN_PHASE_CONVERSION = "physics_terrain_phase_conversion";
inline const std::string PHYSICS_VAPOR_CREATION = "physics_vapor_creation";
inline const std::string PHYSICS_VAPOR_MERGE_UP = "physics_vapor_merge_up";
inline const std::string PHYSICS_VAPOR_MERGE_SIDEWAYS = "physics_vapor_merge_sideways";
inline const std::string PHYSICS_ADD_VAPOR_TO_TILE_ABOVE = "physics_add_vapor_to_tile_above";
inline const std::string PHYSICS_CREATE_VAPOR_ENTITY = "physics_create_vapor_entity";
inline const std::string PHYSICS_DELETE_OR_CONVERT_TERRAIN = "physics_delete_or_convert_terrain";
inline const std::string PHYSICS_INVALID_TERRAIN_FOUND = "physics_invalid_terrain_found";

// Life event time series
inline const std::string LIFE_KILL_ENTITY = "life_kill_entity";
inline const std::string LIFE_SOFT_KILL_ENTITY = "life_soft_kill_entity";
inline const std::string LIFE_HARD_KILL_ENTITY = "life_hard_kill_entity";
inline const std::string LIFE_REMOVE_VELOCITY = "life_remove_velocity";
inline const std::string LIFE_REMOVE_MOVING_COMPONENT = "life_remove_moving_component";

}  // namespace TimeSeriesNames

}  // namespace CommandConstants

#endif  // COMMAND_CONSTANTS_HPP
