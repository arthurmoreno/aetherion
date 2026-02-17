#include "FlatbufferUtils.hpp"

nb::object fbGetEntityById(
    int entity_id,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::EntityInterface>> &entities) {
    if (!entities.size()) {  // Since it's a reference, check size instead of null
        return nb::none();
    }

    for (auto entity : entities) {
        // std::cout << "Checking entity with ID: " << entity->entityId() <<
        // std::endl;

        if (entity->entityId() == entity_id) {
            // std::cout << "Entity found. ID: " << entity_id << std::endl;

            // Retrieve the entity_data
            auto data = entity->entity_data();
            std::vector<char> entity_buffer;
            if (data) {
                entity_buffer.resize(data->size());
                std::memcpy(entity_buffer.data(), data->data(), data->size());
            }

            // Deserialize into EntityInterface
            EntityInterface deserializedEntity =
                EntityInterface::deserialize(entity_buffer.data(), entity_buffer.size());

            // Assign the entityId (if not already handled in deserialize)
            deserializedEntity.entityId = entity->entityId();

            return nb::cast(deserializedEntity);
        }
    }

    // If no matching entity is found, return nb::none()
    return nb::none();
}

void populateEntitiesMap(
    std::unordered_map<int, EntityInterface> &entities,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::EntityInterface>>
        &flatbuffersEntities) {
    if (!flatbuffersEntities.size()) {
        return;
    }

    for (auto flatbufferEntity : flatbuffersEntities) {
        // Retrieve the entity_data
        auto data = flatbufferEntity->entity_data();
        std::vector<char> entity_buffer;
        if (data) {
            entity_buffer.resize(data->size());
            std::memcpy(entity_buffer.data(), data->data(), data->size());
        }

        // Deserialize into EntityInterface
        EntityInterface deserializedEntity =
            EntityInterface::deserialize(entity_buffer.data(), entity_buffer.size());

        // Assign the entityId (if not already handled in deserialize)
        deserializedEntity.entityId = flatbufferEntity->entityId();

        entities[flatbufferEntity->entityId()] = deserializedEntity;
    }
}

nb::object fbGetQueryResponseById(
    int query_id,
    const ::flatbuffers::Vector<::flatbuffers::Offset<GameEngine::QueryResponse>> &queryResponses) {
    if (!queryResponses.size()) {  // Since it's a reference, check size instead of null
        return nb::none();
    }

    for (auto queryResponse : queryResponses) {
        if (queryResponse->query_id() == query_id) {
            // std::cout << "Entity found. ID: " << entity_id << std::endl;

            auto data = queryResponse->query_data();
            std::vector<char> queryResponseBuffer;
            if (data) {
                queryResponseBuffer.resize(data->size());
                std::memcpy(queryResponseBuffer.data(), data->data(), data->size());
            }

            // Deserialize into EntityInterface
            auto deserializedQueryResponse =
                QueryResponse::deserialize(queryResponseBuffer.data(), queryResponseBuffer.size());

            // Assign the entityId (if not already handled in deserialize)
            // deserializedQueryResponse.entityId = entity->entityId();

            return nb::cast(deserializedQueryResponse);
        }
    }

    // If no matching entity is found, return nb::none()
    return nb::none();
}
