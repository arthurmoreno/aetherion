#ifndef VOXELGRID_VIEW_HPP
#define VOXELGRID_VIEW_HPP

#include <flatbuffers/flatbuffers.h>
#include <nanobind/nanobind.h>
#include <openvdb/openvdb.h>

#include "VoxelGridView_generated.h"

namespace nb = nanobind;

class VoxelGridViewFlatB {
   public:
    // Constructor accepts the raw FlatBuffer data pointer
    VoxelGridViewFlatB(const GameEngine::VoxelGridView* fbVoxelGridView)
        : fbVoxelGridView(fbVoxelGridView) {}

    // Constructor accepts nb::bytes
    VoxelGridViewFlatB(nb::bytes serialized_data) {
        const char* data_ptr = serialized_data.c_str();
        size_t data_size = serialized_data.size();

        if (data_size == 0) {
            throw std::runtime_error("Serialized data is empty");
        }

        // Store a copy of the serialized data to keep it alive
        serialized_buffer.assign(data_ptr, data_ptr + data_size);

        // Deserialize FlatBuffer using our owned copy
        fbVoxelGridView = GameEngine::GetVoxelGridView(serialized_buffer.data());
    }

    // Accessor methods to access fields directly
    int getWidth() const { return fbVoxelGridView->width(); }

    int getHeight() const { return fbVoxelGridView->height(); }

    int getDepth() const { return fbVoxelGridView->depth(); }

    int getXOffset() const { return fbVoxelGridView->x_offset(); }

    int getYOffset() const { return fbVoxelGridView->y_offset(); }

    int getZOffset() const { return fbVoxelGridView->z_offset(); }

    // Access voxel grid data without deserializing
    int getTerrainVoxel(int x, int y, int z) const {
        int local_x = x - getXOffset();
        int local_y = y - getYOffset();
        int local_z = z - getZOffset();

        if (local_x >= 0 && local_x < getWidth() && local_y >= 0 && local_y < getHeight() &&
            local_z >= 0 && local_z < getDepth()) {
            // Calculate the index in the FlatBuffer data array
            int index = local_x + local_y * getWidth() + local_z * getWidth() * getHeight();

            // Access the GridData at the calculated index
            return fbVoxelGridView->terrainData()->Get(index);
        } else {
            // Handle out-of-bounds access
            // std::cerr << "Attempted to get voxel out of bounds at (" << x << ", " << y << ", " <<
            // z
            //           << ")" << std::endl;
            return -1;  // Return null pointer to indicate out-of-bounds access
        }
    }

    // Access voxel grid data without deserializing
    int getEntityVoxel(int x, int y, int z) const {
        int local_x = x - getXOffset();
        int local_y = y - getYOffset();
        int local_z = z - getZOffset();

        if (local_x >= 0 && local_x < getWidth() && local_y >= 0 && local_y < getHeight() &&
            local_z >= 0 && local_z < getDepth()) {
            // Calculate the index in the FlatBuffer data array
            int index = local_x + local_y * getWidth() + local_z * getWidth() * getHeight();

            // Access the GridData at the calculated index
            return fbVoxelGridView->entityData()->Get(index);
        } else {
            // Handle out-of-bounds access
            // std::cerr << "Attempted to get voxel out of bounds at (" << x << ", " << y << ", " <<
            // z
            //           << ")" << std::endl;
            return -1;  // Return null pointer to indicate out-of-bounds access
        }
    }

   private:
    const GameEngine::VoxelGridView* fbVoxelGridView;
    std::vector<char>
        serialized_buffer;  // Owns the serialized data when constructed from nb::bytes
};

class VoxelGridView {
   public:
    int width, height, depth;
    int x_offset, y_offset, z_offset;
    std::vector<int> terrainData;
    std::vector<int> entityData;

    // Parameterized constructor
    void initVoxelGridView(int width, int height, int depth, int x_offset, int y_offset,
                           int z_offset);

    // Set and get voxel data
    void setTerrainVoxel(int x, int y, int z, const int voxelData);
    int getTerrainVoxel(int x, int y, int z) const;

    // Set and get voxel data
    void setEntityVoxel(int x, int y, int z, const int voxelData);
    int getEntityVoxel(int x, int y, int z) const;

    // FlatBuffers serialization (returns FlatBuffers offset)
    flatbuffers::Offset<GameEngine::VoxelGridView> serializeFlatBuffers(
        flatbuffers::FlatBufferBuilder& builder) const {
        // Serialize terrainData vector
        auto terrainDataOffset = builder.CreateVector(terrainData);

        // Serialize entityData vector
        auto entityDataOffset = builder.CreateVector(entityData);

        // Create the VoxelGridView FlatBuffer object
        return GameEngine::CreateVoxelGridView(builder, width, height, depth, x_offset, y_offset,
                                               z_offset, terrainDataOffset, entityDataOffset);
    }

    // FlatBuffers deserialization
    static VoxelGridView deserializeFlatBuffers(const GameEngine::VoxelGridView* fbVoxelGridView) {
        if (!fbVoxelGridView) {
            throw std::invalid_argument("fbVoxelGridView pointer is null");
        }

        VoxelGridView vgv;

        // Deserialize basic properties
        vgv.width = fbVoxelGridView->width();
        vgv.height = fbVoxelGridView->height();
        vgv.depth = fbVoxelGridView->depth();
        vgv.x_offset = fbVoxelGridView->x_offset();
        vgv.y_offset = fbVoxelGridView->y_offset();
        vgv.z_offset = fbVoxelGridView->z_offset();

        // Deserialize terrainData
        auto fbTerrainData = fbVoxelGridView->terrainData();
        if (fbTerrainData) {
            vgv.terrainData.assign(fbTerrainData->begin(), fbTerrainData->end());
        } else {
            // Handle the case where terrainData is missing
            throw std::runtime_error("terrainData is missing in FlatBuffer VoxelGridView");
        }

        // Deserialize entityData
        auto fbEntityData = fbVoxelGridView->entityData();
        if (fbEntityData) {
            vgv.entityData.assign(fbEntityData->begin(), fbEntityData->end());
        } else {
            // Handle the case where entityData is missing
            throw std::runtime_error("entityData is missing in FlatBuffer VoxelGridView");
        }

        return vgv;
    }
};

#endif  // READONLY_QUERIES_HPP