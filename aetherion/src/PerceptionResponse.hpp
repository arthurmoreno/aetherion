#ifndef PERCEPTION_RESPONSE_HPP
#define PERCEPTION_RESPONSE_HPP

#include <cstdint>  // For uint64_t
// #include <iostream>
#include <nanobind/nanobind.h>

#include "EntityInterface.hpp"
#include "FlatbufferUtils.hpp"
#include "GameClock.hpp"
#include "PerceptionResponse_generated.h"
#include "QueryResponse.hpp"
#include "WorldView.hpp"
#include "flatbuffers/flatbuffers.h"

class PerceptionResponseFlatB {
   public:
    // Constructor that accepts the raw FlatBuffer data pointer
    PerceptionResponseFlatB(const GameEngine::PerceptionResponse* fbPerceptionResponse)
        : fbPerceptionResponse(fbPerceptionResponse) {}

    // Constructor that accepts serialized FlatBuffer bytes (as nb::bytes)
    PerceptionResponseFlatB(nb::bytes serialized_data) {
        const char* data_ptr = serialized_data.c_str();
        size_t data_size = serialized_data.size();

        if (data_size == 0) {
            throw std::runtime_error("Serialized data is empty");
        }

        // Deserialize FlatBuffer
        fbPerceptionResponse =
            GameEngine::GetPerceptionResponse(data_ptr);  // Get PerceptionResponse from FlatBuffer
    }

    // Get the WorldViewFlatB object
    WorldViewFlatB getWorldView() const {
        return WorldViewFlatB(
            fbPerceptionResponse->world_view());  // Return the deserialized WorldViewFlatB
    }

    // Get the EntityInterface object
    nb::object getEntity() const {
        const GameEngine::EntityInterface* entity_fb = fbPerceptionResponse->entity();

        // Deserialize entity data
        std::vector<char> entity_buffer(entity_fb->entity_data()->begin(),
                                        entity_fb->entity_data()->end());
        EntityInterface entity =
            EntityInterface::deserialize(entity_buffer.data(), entity_buffer.size());

        // Assign the entityId from the FlatBuffer
        entity.entityId = entity_fb->entityId();

        // Return the deserialized entity as a Python object
        return nb::cast(entity);
    }

    // Get the EntityInterface object
    nb::object getItemFromInventoryById(int item_entity_id) const {
        // std::cout << "Items entity id: " << item_entity_id << std::endl;
        const auto* items_entities = fbPerceptionResponse->items_entities();
        // std::cout << "Items entities size: " << items_entities->size() << std::endl;
        if (!items_entities) {
            return nb::none();
        }

        return fbGetEntityById(item_entity_id, *items_entities);
    }

    // Get the EntityInterface object
    nb::object getQueryResponseById(int query_response_id) const {
        const auto* query_responses = fbPerceptionResponse->query_responses();
        if (!query_responses) {
            return nb::none();
        }

        return fbGetQueryResponseById(query_response_id, *query_responses);
    }

    uint64_t getTicks() const {
        if (!fbPerceptionResponse) {
            throw std::runtime_error("PerceptionResponseFlatB is not initialized");
        } else {
            // std::cout << "Ticks inside getTicks Perception response: "
            //           << fbPerceptionResponse->game_clock_ticks() << std::endl;
            return fbPerceptionResponse->game_clock_ticks();
        }
    }

   private:
    const GameEngine::PerceptionResponse* fbPerceptionResponse;
};

struct PerceptionResponse {
    EntityInterface entity;
    WorldView world_view;
    uint64_t ticks;
    std::unordered_map<int, EntityInterface> itemsEntities;
    std::unordered_map<int, std::shared_ptr<QueryResponse>> queryResponses;

    // Default constructor
    PerceptionResponse() = default;

    PerceptionResponse(const EntityInterface& entity, const WorldView& world_view)
        : entity(entity), world_view(world_view) {}

    // Method to serialize PerceptionResponse using FlatBuffers
    std::vector<char> serializeFlatBuffer() const {
        flatbuffers::FlatBufferBuilder builder;

        // Serialize the entity
        // Serialize entity data (custom serialization using struct_pack or similar)
        std::vector<char> entity_buffer = entity.serialize();
        auto entity_data_offset = builder.CreateVector(
            reinterpret_cast<const uint8_t*>(entity_buffer.data()), entity_buffer.size());

        // Create FlatBuffer for EntityInterface
        auto entity_offset =
            GameEngine::CreateEntityInterface(builder, entity.entityId, entity_data_offset);

        // Serialize the WorldView
        auto world_view_offset = world_view.serializeFlatBuffer(builder);

        // Serialize entities using FlatBuffers
        std::vector<flatbuffers::Offset<GameEngine::EntityInterface>> itemsEntitiesOffsets;

        for (const auto& [itemsEntityId, itemsEntityInterface] : itemsEntities) {
            // Serialize the EntityInterface using struct_pack
            std::vector<char> entity_buffer =
                itemsEntityInterface.serialize();  // Custom serialization using struct_pack

            // Store the serialized data as a vector of ubyte in FlatBuffers
            auto entityDataOffset = builder.CreateVector(
                reinterpret_cast<const uint8_t*>(entity_buffer.data()), entity_buffer.size());

            // Create the FlatBuffer EntityInterface object
            auto entityOffset =
                GameEngine::CreateEntityInterface(builder, itemsEntityId, entityDataOffset);
            itemsEntitiesOffsets.push_back(entityOffset);
        }

        auto itemsEntitiesFinalOffset = builder.CreateVector(itemsEntitiesOffsets);

        // Serialize entities using FlatBuffers
        std::vector<flatbuffers::Offset<GameEngine::QueryResponse>> queryResponsesOffsets;

        for (const auto& [queryResponseId, queryResponse] : queryResponses) {
            // Serialize the QueryResponse using struct_pack
            std::vector<char> queryResponseBuffer =
                queryResponse->serialize();  // Custom serialization using struct_pack

            // Store the serialized data as a vector of ubyte in FlatBuffers
            auto queryResponseDataOffset =
                builder.CreateVector(reinterpret_cast<const uint8_t*>(queryResponseBuffer.data()),
                                     queryResponseBuffer.size());

            // Create the FlatBuffer QueryResponse object
            auto queryResponseOffset =
                GameEngine::CreateQueryResponse(builder, queryResponseId, queryResponseDataOffset);
            queryResponsesOffsets.push_back(queryResponseOffset);
        }

        auto queryResponsesFinalOffset = builder.CreateVector(queryResponsesOffsets);

        // **Include the ticks variable**
        uint64_t ticks_value = ticks;

        // Create PerceptionResponse FlatBuffer object
        auto perception_response_offset = GameEngine::CreatePerceptionResponse(
            builder, entity_offset, world_view_offset, ticks_value, itemsEntitiesFinalOffset,
            queryResponsesFinalOffset);

        // Finalize the FlatBuffer
        builder.Finish(perception_response_offset);

        // Return the serialized data as a std::vector<char>
        return std::vector<char>(builder.GetBufferPointer(),
                                 builder.GetBufferPointer() + builder.GetSize());
    }

    // Python-friendly method for FlatBuffer serialization
    nb::bytes pySerializeFlatBuffer() const {
        std::vector<char> serialized_data = serializeFlatBuffer();
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

#endif  // PERCEPTION_RESPONSE_HPP