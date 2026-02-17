#ifndef PROCESS_OPTIONAL_QUERIES_HPP
#define PROCESS_OPTIONAL_QUERIES_HPP

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "GameDBHandler.hpp"
#include "PerceptionResponse.hpp"
#include "QueryCommand.hpp"
#include "QueryResponse.hpp"
#include "entt/entt.hpp"

// Main function to process optional queries
// This function is decoupled from the World class and requires explicit context passing
void _processOptionalQueries(const std::vector<QueryCommand>& commands,
                             PerceptionResponse& response, entt::registry& registry,
                             GameDBHandler* dbHandler);

#endif  // PROCESS_OPTIONAL_QUERIES_HPP
