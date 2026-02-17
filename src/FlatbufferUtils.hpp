#ifndef FLATBUFFER_UTILS_HPP
#define FLATBUFFER_UTILS_HPP

#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>

#include "EntityInterface.hpp"
#include "EntityInterface_generated.h"
#include "PerceptionResponse_generated.h"
#include "QueryResponse.hpp"
#include "flatbuffers/flatbuffers.h"

namespace nb = nanobind;

nb::object fbGetEntityById(
    int entity_id,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::EntityInterface>>& entities);

void populateEntitiesMap(
    std::unordered_map<int, EntityInterface>& entities,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::EntityInterface>>&
        flatbuffersEntities);

nb::object fbGetQueryResponseById(
    int entity_id,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::QueryResponse>>& queryResponses);

#endif  // WORLD_VIEW_HPP