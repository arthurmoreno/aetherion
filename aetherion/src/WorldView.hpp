#ifndef WORLD_VIEW_HPP
#define WORLD_VIEW_HPP

#include <nanobind/nanobind.h>

#include <msgpack.hpp>
#include <unordered_map>
#include <vector>

#include "EntityInterface.hpp"
#include "FlatbufferUtils.hpp"
#include "VoxelGrid.hpp"
#include "WorldView_generated.h"
#include "components/PerceptionComponent.hpp"
#include "components/PhysicsComponents.hpp"
#include "flatbuffers/flatbuffers.h"

namespace nb = nanobind;

class WorldViewFlatB {
   public:
    const GameEngine::WorldView* fbWorldView;
    std::unordered_map<int, EntityInterface> entities;
    // Owns the serialized data when constructed from nb::bytes
    std::vector<char> serialized_buffer;

    // Constructor accepts the raw FlatBuffer data pointer
    WorldViewFlatB(const GameEngine::WorldView* fbWorldView, const bool prePopulateEntities)
        : fbWorldView(fbWorldView), prePopulateEntities(prePopulateEntities) {
        if (prePopulateEntities) {
            const auto* flatbuffersEntities = fbWorldView->entities();
            populateEntitiesMap(entities, *flatbuffersEntities);
        }
    }

    // Constructor accepts the raw FlatBuffer data pointer
    WorldViewFlatB(const GameEngine::WorldView* fbWorldView) : fbWorldView(fbWorldView) {}

    // Constructor that accepts nb::bytes
    WorldViewFlatB(nb::bytes serialized_data) {
        const char* data_ptr = serialized_data.c_str();
        size_t data_size = serialized_data.size();

        // Validate data
        if (data_size == 0) {
            throw std::runtime_error("Serialized data is empty");
        }

        // Store a copy of the serialized data to keep it alive
        serialized_buffer.assign(data_ptr, data_ptr + data_size);

        // Deserialize FlatBuffer using our owned copy
        fbWorldView = GameEngine::GetWorldView(serialized_buffer.data());  // Deserialize FlatBuffer

        if (prePopulateEntities) {
            const auto* flatbuffersEntities = fbWorldView->entities();
            populateEntitiesMap(entities, *flatbuffersEntities);
        }
    }

    // Accessor for the width
    int getWidth() const { return fbWorldView->width(); }

    // Accessor for the height
    int getHeight() const { return fbWorldView->height(); }

    // Accessor for the depth
    int getDepth() const { return fbWorldView->depth(); }

    // Accessor for the voxel grid
    VoxelGridViewFlatB getVoxelGrid() const { return VoxelGridViewFlatB(fbWorldView->voxelGrid()); }

    nb::object getEntityByIdPrePopulated(int entity_id) const {
        auto it = entities.find(entity_id);
        if (it != entities.end()) {
            return nb::cast(it->second);
        } else {
            return nb::none();
        }
    }

    nb::object getEntityByIdLazy(int entity_id) const {
        const auto* entities = fbWorldView->entities();
        if (!entities) {
            return nb::none();
        }

        return fbGetEntityById(entity_id, *entities);
    }

    // Deserialization in WorldViewFlatB
    nb::object getEntityById(int entity_id) const {
        if (prePopulateEntities) {
            return getEntityByIdPrePopulated(entity_id);
        } else {
            return getEntityByIdLazy(entity_id);
        }
    }

    // Get the terrain ID at a specific voxel
    nb::object getTerrain(int x, int y, int z) const {
        VoxelGridViewFlatB voxelGrid = getVoxelGrid();
        int entityId = voxelGrid.getTerrainVoxel(x, y, z);
        if (entityId != -1) {
            return getEntityById(entityId);
        }
        return nb::none();
    }

    // Get the entity object at a specific voxel (by entity ID)
    nb::object getEntity(int x, int y, int z) const {
        VoxelGridViewFlatB voxelGrid = getVoxelGrid();
        int entityId = voxelGrid.getEntityVoxel(x, y, z);
        // return entityId;
        if (entityId != -1) {
            return getEntityById(entityId);
        }
        return nb::none();
    }

   private:
    const bool prePopulateEntities{false};
};

struct WorldView {
    int width;
    int height;
    int depth;

    // VoxelGridView voxelGrid;  // A voxel grid representing the world
    VoxelGridView voxelGridView;                        // A voxel grid representing the world
    std::unordered_map<int, EntityInterface> entities;  // Map of entity IDs to their interfaces
    std::unordered_map<int, EntityInterface>
        tileEffectsEntities;  // Map of entity IDs to their interfaces

    // Add an entity by ID
    void addEntity(int id, const EntityInterface& entity) {
        entities[id] = entity;
        // std::cout << "Added Entity with ID: " << id << std::endl;
    }

    // Get an entity by ID
    // py::object getEntity(int id) const {
    //     auto it = entities.find(id);
    //     if (it != entities.end()) {
    //         return py::cast(it->second);
    //     }
    //     return py::none();
    // }

    // Check if an entity exists
    bool hasEntity(int id) const { return entities.find(id) != entities.end(); }

    // Print all entity IDs for debugging
    void printAllEntityIds() const {
        std::cout << "Current Entity IDs in WorldView:" << std::endl;
        for (const auto& [id, entity] : entities) {
            std::cout << "  Entity ID: " << id << std::endl;
        }
    }

    // Retrieve an entity by its ID
    EntityInterface* getEntityById(int entity_id) const {
        auto it = entities.find(entity_id);
        if (it != entities.end()) {
            return const_cast<EntityInterface*>(&it->second);
        }
        return nullptr;
    }

    // Retrieve an entity by its ID
    nb::object pyGetEntityById(int entity_id) const {
        auto it = entities.find(entity_id);
        if (it != entities.end()) {
            return nb::cast(it->second);
        }
        return nb::none();  // Return None if entity not found
    }

    int getTerrainId(int x, int y, int z) const { return voxelGridView.getTerrainVoxel(x, y, z); }

    int getEntityId(int x, int y, int z) const { return voxelGridView.getEntityVoxel(x, y, z); }

    bool checkIfTerrainExist(int x, int y, int z) {
        return voxelGridView.getTerrainVoxel(x, y, z) != -1;
    }

    bool checkIfEntityExist(int x, int y, int z) {
        return voxelGridView.getEntityVoxel(x, y, z) != -1;
    }

    // Get the terrain ID at a specific voxel
    nb::object getTerrain(int x, int y, int z) const {
        int entityId =
            voxelGridView.getTerrainVoxel(x, y, z);  // Returns the terrain ID from the VoxelGrid
        if (entityId != -1) {
            return pyGetEntityById(entityId);  // Map the entity ID to an EntityInterface object
        }
        return nb::none();  // Return None if no entity found
    }

    // Get the entity object at a specific voxel (by entity ID)
    nb::object getEntity(int x, int y, int z) const {
        int entityId =
            voxelGridView.getEntityVoxel(x, y, z);  // Get the entity ID from the VoxelGrid
        // return entityId;
        if (entityId != -1) {
            return pyGetEntityById(entityId);  // Map the entity ID to an EntityInterface object
        }
        return nb::none();  // Return None if no entity found
    }

    // Constructor that deserializes from FlatBuffers
    // WorldView(const GameEngine::WorldView* fbWorldView) {
    static WorldView deserializeFlatBuffers(WorldViewFlatB worldViewFlatB) {
        auto fbWorldView = worldViewFlatB.fbWorldView;
        WorldView worldView;
        // Deserialize basic properties
        worldView.width = fbWorldView->width();
        worldView.height = fbWorldView->height();
        worldView.depth = fbWorldView->depth();

        // Deserialize voxel grid
        const GameEngine::VoxelGridView* fbVoxelGrid = fbWorldView->voxelGrid();
        if (fbVoxelGrid) {
            worldView.voxelGridView = VoxelGridView::deserializeFlatBuffers(fbVoxelGrid);
        }

        // Deserialize entities
        auto fbEntities = fbWorldView->entities();
        if (fbEntities) {
            for (auto fbEntity : *fbEntities) {
                int entityId = fbEntity->entityId();
                // auto fbEntityData = fbEntity->entity_data();

                // Retrieve the entity_data
                auto data = fbEntity->entity_data();
                std::vector<char> entity_buffer;
                if (data) {
                    entity_buffer.resize(data->size());
                    std::memcpy(entity_buffer.data(), data->data(), data->size());

                    // Print raw entity_data
                    // std::cout << "Raw entity_data size: " << entity_buffer.size() <<
                    // std::endl; std::cout << "Raw entity_data (hex): "; for (char byte :
                    // entity_buffer) {
                    //     printf("%02x ", static_cast<unsigned char>(byte));
                    // }
                    // std::cout << std::endl;
                }

                // Deserialize into EntityInterface
                EntityInterface deserializedEntity =
                    EntityInterface::deserialize(entity_buffer.data(), entity_buffer.size());

                // Assign the entityId (if not already handled in deserialize)
                deserializedEntity.entityId = fbEntity->entityId();

                // Use emplace for efficiency
                worldView.entities.emplace(deserializedEntity.entityId,
                                           std::move(deserializedEntity));
            }
        }
        return worldView;
    }

    // New method to handle nb::bytes deserialization and return WorldView
    static WorldView pyDeserializeFlatBuffer(nb::bytes serialized_data) {
        const char* data_ptr = serialized_data.c_str();
        size_t data_size = serialized_data.size();

        if (data_size == 0) {
            throw std::runtime_error("Serialized data is empty");
        }
        auto fbWorldView = GameEngine::GetWorldView(data_ptr);  // Deserialize FlatBuffer

        WorldViewFlatB temp(serialized_data);
        return deserializeFlatBuffers(fbWorldView);
    }

    // Serialize using FlatBuffers
    std::vector<char> serializeFlatBuffer() const {
        flatbuffers::FlatBufferBuilder builder;

        auto worldViewOffset = serializeFlatBuffer(builder);

        builder.Finish(worldViewOffset);

        return std::vector<char>(builder.GetBufferPointer(),
                                 builder.GetBufferPointer() + builder.GetSize());
    }

    std::vector<flatbuffers::Offset<GameEngine::EntityInterface>> serializeEntities(
        flatbuffers::FlatBufferBuilder& builder,
        std::unordered_map<int, EntityInterface> entitiesMap) const {
        // Serialize entities using FlatBuffers
        std::vector<flatbuffers::Offset<GameEngine::EntityInterface>> entityOffsets;

        for (const auto& [entityId, entityInterface] : entitiesMap) {
            // Serialize the EntityInterface using struct_pack
            std::vector<char> entity_buffer = entityInterface.serialize();

            // Store the serialized data as a vector of ubyte in FlatBuffers
            auto entityDataOffset = builder.CreateVector(
                reinterpret_cast<const uint8_t*>(entity_buffer.data()), entity_buffer.size());

            // Create the FlatBuffer EntityInterface object
            auto entityOffset =
                GameEngine::CreateEntityInterface(builder, entityId, entityDataOffset);
            entityOffsets.push_back(entityOffset);
        }

        return entityOffsets;
        // auto entitiesOffset = builder.CreateVector(entityOffsets);
    }

    flatbuffers::Offset<GameEngine::WorldView> serializeFlatBuffer(
        flatbuffers::FlatBufferBuilder& builder) const {
        // Serialize voxelGrid
        auto voxelGridViewOffset = voxelGridView.serializeFlatBuffers(builder);

        std::vector<flatbuffers::Offset<GameEngine::EntityInterface>> entityOffsets =
            serializeEntities(builder, entities);
        auto entitiesOffset = builder.CreateVector(entityOffsets);

        auto worldViewOffset = GameEngine::CreateWorldView(builder, width, height, depth,
                                                           voxelGridViewOffset, entitiesOffset);
        return worldViewOffset;
    }

    nb::bytes pySerializeFlatBuffers() const {
        // Call serializeFlatBuffer to get the FlatBuffer as std::vector<char>
        std::vector<char> serialized_data = serializeFlatBuffer();

        // Return as a Python bytes object
        return nb::bytes(serialized_data.data(), serialized_data.size());
    }
};

#endif  // WORLD_VIEW_HPP