#include "VoxelGrid.hpp"

#include <memory>

// Constructor
VoxelGrid::VoxelGrid(entt::registry& reg) : registry(reg) {
    openvdb::initialize();  // Initialize OpenVDB

    // Initialize TerrainStorage
    terrainStorage = std::make_unique<TerrainStorage>();
    terrainStorage->initialize();

    // Create TerrainGridRepository with the provided registry and storage
    terrainGridRepository = std::make_unique<TerrainGridRepository>(registry, *terrainStorage);

    // Create empty grids for entity, event, and lighting (terrain now managed by
    // TerrainGridRepository)
    entityGrid = openvdb::Int32Grid::create(defaultEmptyValue);  // Default entity ID = 0
    eventGrid = openvdb::Int32Grid::create(defaultEmptyValue);   // Default event ID = 0
    lightingGrid = openvdb::FloatGrid::create(0.0f);             // Default lighting level = 0.0
}

// Destructor
VoxelGrid::~VoxelGrid() {
    openvdb::uninitialize();  // Clean up OpenVDB
}

// Initialize all the grids (if needed, can be extended)
void VoxelGrid::initializeGrids() {
    // Terrain grid is now managed by TerrainGridRepository, so we only initialize the others
    entityGrid->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
    eventGrid->setTransform(openvdb::math::Transform::createLinearTransform(1.0));
    lightingGrid->setTransform(openvdb::math::Transform::createLinearTransform(1.0));

    // Apply transform to terrain storage as well
    if (terrainStorage) {
        terrainStorage->applyTransform(1.0);
    }
}

// Set voxel data for all grids
void VoxelGrid::setVoxel(int x, int y, int z, const GridData& data) {
    // Set terrain using TerrainGridRepository (using mainType for now as terrainID)
    if (terrainGridRepository) {
        terrainGridRepository->setTerrainMainType(x, y, z, data.terrainID);
    }

    // Set entity, event, and lighting in respective grids
    entityGrid->getAccessor().setValue(openvdb::Coord(x, y, z), data.entityID);
    eventGrid->getAccessor().setValue(openvdb::Coord(x, y, z), data.eventID);
    lightingGrid->getAccessor().setValue(openvdb::Coord(x, y, z), data.lightingLevel);
}

// Get voxel data from all grids
GridData VoxelGrid::getVoxel(int x, int y, int z) const {
    GridData data;

    // Retrieve data from terrain repository and other grids
    if (terrainGridRepository) {
        std::optional<int> terrainId = terrainGridRepository->getTerrainIdIfExists(x, y, z);
        if (terrainId && *terrainId != -2) {
            data.terrainID = *terrainId;
        }
    } else {
        data.terrainID = defaultEmptyValue;
    }

    data.entityID = entityGrid->getConstAccessor().getValue(openvdb::Coord(x, y, z));
    data.eventID = eventGrid->getConstAccessor().getValue(openvdb::Coord(x, y, z));
    data.lightingLevel = lightingGrid->getConstAccessor().getValue(openvdb::Coord(x, y, z));

    return data;
}

void VoxelGrid::setTerrain(int x, int y, int z, int terrainID) {
    if (terrainGridRepository) {
        // Instead of directly setting mainType, we need to check if there's an entity
        // at this location that needs to be migrated to OpenVDB storage

        // For now, if there's no existing entity, directly set the mainType
        // This maintains compatibility with direct terrain setting
        // terrainGridRepository->setMainType(x, y, z, terrainID);
        Position pos{x, y, z};
        entt::entity terrain = static_cast<entt::entity>(terrainID);
        registry.emplace<Position>(terrain, pos);
        terrainGridRepository->setTerrainFromEntt(terrain);
    }
}

int VoxelGrid::getTerrain(int x, int y, int z) const {
    if (terrainGridRepository) {
        std::optional<int> terrainId = terrainGridRepository->getTerrainIdIfExists(x, y, z);
        if (terrainId && *terrainId != -2) {
            return *terrainId;
        }
    }
    return defaultEmptyValue;
}

// Delete terrain at a specific voxel
void VoxelGrid::deleteTerrain(int x, int y, int z) {
    if (terrainGridRepository) {
        terrainGridRepository->deleteTerrain(x, y, z);
    }
}

bool VoxelGrid::checkIfTerrainExists(int x, int y, int z) const {
    if (terrainGridRepository) {
        return terrainGridRepository->checkIfTerrainExists(x, y, z);
    }
    return false;
}

EntityTypeComponent VoxelGrid::getTerrainEntityTypeComponent(int x, int y, int z) const {
    if (terrainGridRepository) {
        return terrainGridRepository->getTerrainEntityType(x, y, z);
    }
    return EntityTypeComponent();  // Return a default-constructed EntityTypeComponent if not found
}

// void VoxelGrid::setTerrainFromEntt(entt::entity entity) {
//     if (terrainGridRepository) {
//         terrainGridRepository->setTerrainFromEntt(entity);
//     }
// }

void VoxelGrid::setEntity(int x, int y, int z, int entityID) {
    entityGrid->tree().setValue(openvdb::Coord(x, y, z), entityID);
}

int VoxelGrid::getEntity(int x, int y, int z) const {
    return entityGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void VoxelGrid::setEvent(int x, int y, int z, int eventID) {
    eventGrid->tree().setValue(openvdb::Coord(x, y, z), eventID);
}

int VoxelGrid::getEvent(int x, int y, int z) const {
    return eventGrid->tree().getValue(openvdb::Coord(x, y, z));
}

void VoxelGrid::setLightingLevel(int x, int y, int z, float lightingLevel) {
    lightingGrid->tree().setValue(openvdb::Coord(x, y, z), lightingLevel);
}

float VoxelGrid::getLightingLevel(int x, int y, int z) const {
    return lightingGrid->tree().getValue(openvdb::Coord(x, y, z));
}

std::vector<char> VoxelGrid::serializeToBytes() const {
    std::map<VoxelGridCoordinates, GridData> voxelDataMap;

    // Populate the map with voxel data using TerrainStorage's mainTypeGrid iterator
    if (terrainStorage && terrainStorage->mainTypeGrid) {
        for (auto iter = terrainStorage->mainTypeGrid->cbeginValueOn(); iter.test(); ++iter) {
            openvdb::Coord coord = iter.getCoord();
            VoxelGridCoordinates coordinates = {coord.x(), coord.y(), coord.z()};

            GridData data;
            data.terrainID = terrainStorage->getTerrainMainType(coord.x(), coord.y(), coord.z());
            data.entityID = entityGrid->tree().getValue(coord);
            data.eventID = eventGrid->tree().getValue(coord);
            data.lightingLevel = lightingGrid->tree().getValue(coord);

            voxelDataMap[coordinates] = data;
        }
    }

    // Serialize the map using msgpack
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, voxelDataMap);

    // Convert the serialized buffer to a byte vector
    return std::vector<char>(buffer.data(), buffer.data() + buffer.size());
}

void VoxelGrid::deserializeFromBytes(const std::vector<char>& byteData) {
    // Unpack the map from the byte data using msgpack
    msgpack::object_handle oh = msgpack::unpack(byteData.data(), byteData.size());

    msgpack::object obj = oh.get();
    std::map<VoxelGridCoordinates, GridData> voxelDataMap;
    obj.convert(voxelDataMap);

    // Clear existing grids before populating them
    if (terrainStorage && terrainStorage->mainTypeGrid) {
        terrainStorage->mainTypeGrid->clear();
    }
    entityGrid->clear();
    eventGrid->clear();
    lightingGrid->clear();

    // Populate the grids using the data from the map
    for (const auto& [coordinates, data] : voxelDataMap) {
        openvdb::Coord coord(coordinates.x, coordinates.y, coordinates.z);

        // Set terrain data using TerrainGridRepository
        if (terrainGridRepository) {
            terrainGridRepository->setTerrainMainType(coordinates.x, coordinates.y, coordinates.z,
                                                      data.terrainID);
        }

        entityGrid->tree().setValue(coord, data.entityID);
        eventGrid->tree().setValue(coord, data.eventID);
        lightingGrid->tree().setValue(coord, data.lightingLevel);
    }
}

template <typename Packer>
void VoxelGrid::msgpack_pack(Packer& pk) const {
    // Serialize the grid to a byte vector using the existing method
    std::vector<char> byteData = serializeToBytes();

    // Pack the byte vector using Msgpack
    pk.pack_bin(byteData.size());  // Indicate that we are packing binary data
    pk.pack_bin_body(byteData.data(),
                     byteData.size());  // Pack the actual binary data
}

void VoxelGrid::msgpack_unpack(msgpack::object const& o) {
    // Ensure that the object contains binary data
    if (o.type != msgpack::type::BIN) {
        throw std::runtime_error("Expected binary data for VoxelGrid deserialization");
    }

    // Get the binary data from the Msgpack object
    auto bin = o.via.bin;
    std::vector<char> byteData(bin.ptr, bin.ptr + bin.size);

    // Deserialize the byte data back into the VoxelGrid
    deserializeFromBytes(byteData);
}

// --- Implementation of Utility Search Methods ---

std::vector<VoxelGridCoordinates> VoxelGrid::getAllTerrainInRegion(int x_min, int y_min, int z_min,
                                                                   int x_max, int y_max,
                                                                   int z_max) const {
    std::vector<VoxelGridCoordinates> result;

    // Use TerrainStorage's mainTypeGrid for iteration
    if (terrainStorage && terrainStorage->mainTypeGrid) {
        for (auto iter = terrainStorage->mainTypeGrid->cbeginValueOn(); iter; ++iter) {
            openvdb::Coord coord = iter.getCoord();

            // Manual bounding box check
            if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min &&
                coord.y() <= y_max && coord.z() >= z_min && coord.z() <= z_max) {
                result.emplace_back(VoxelGridCoordinates{coord.x(), coord.y(), coord.z()});
            }
        }
    }

    return result;
}

std::vector<VoxelGridCoordinates> VoxelGrid::getAllEntityInRegion(int x_min, int y_min, int z_min,
                                                                  int x_max, int y_max,
                                                                  int z_max) const {
    std::vector<VoxelGridCoordinates> result;

    // Iterate over all active entity voxels
    for (auto iter = entityGrid->cbeginValueOn(); iter; ++iter) {
        openvdb::Coord coord = iter.getCoord();

        // Manual bounding box check
        if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min && coord.y() <= y_max &&
            coord.z() >= z_min && coord.z() <= z_max) {
            result.emplace_back(VoxelGridCoordinates{coord.x(), coord.y(), coord.z()});
        }
    }

    return result;
}

std::vector<VoxelGridCoordinates> VoxelGrid::getAllEventInRegion(int x_min, int y_min, int z_min,
                                                                 int x_max, int y_max,
                                                                 int z_max) const {
    std::vector<VoxelGridCoordinates> result;

    // Iterate over all active event voxels
    for (auto iter = eventGrid->cbeginValueOn(); iter; ++iter) {
        openvdb::Coord coord = iter.getCoord();

        // Manual bounding box check
        if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min && coord.y() <= y_max &&
            coord.z() >= z_min && coord.z() <= z_max) {
            result.emplace_back(VoxelGridCoordinates{coord.x(), coord.y(), coord.z()});
        }
    }

    return result;
}

std::vector<VoxelGridCoordinates> VoxelGrid::getAllLightingInRegion(int x_min, int y_min, int z_min,
                                                                    int x_max, int y_max,
                                                                    int z_max) const {
    std::vector<VoxelGridCoordinates> result;

    // Iterate over all active lighting voxels
    for (auto iter = lightingGrid->cbeginValueOn(); iter; ++iter) {
        openvdb::Coord coord = iter.getCoord();

        // Manual bounding box check
        if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min && coord.y() <= y_max &&
            coord.z() >= z_min && coord.z() <= z_max) {
            result.emplace_back(VoxelGridCoordinates{coord.x(), coord.y(), coord.z()});
        }
    }

    return result;
}

void VoxelGridView::initVoxelGridView(int width, int height, int depth, int x_offset, int y_offset,
                                      int z_offset) {
    this->width = width;
    this->height = height;
    this->depth = depth;
    this->x_offset = x_offset;
    this->y_offset = y_offset;
    this->z_offset = z_offset;

    // Check for valid dimensions before allocating
    if (width <= 0 || height <= 0 || depth <= 0) {
        throw std::runtime_error("Invalid dimensions for VoxelGridView");
    }

    terrainData.resize(width * height * depth);
    entityData.resize(width * height * depth);
}

void VoxelGridView::setTerrainVoxel(int x, int y, int z, const int voxelData) {
    int local_x = x - x_offset;
    int local_y = y - y_offset;
    int local_z = z - z_offset;

    if (local_x >= 0 && local_x < width && local_y >= 0 && local_y < height && local_z >= 0 &&
        local_z < depth) {
        terrainData[local_x + local_y * width + local_z * width * height] = voxelData;
    } else {
        // Handle out-of-bounds access
        std::cerr << "Attempted to set voxel out of bounds at (" << x << ", " << y << ", " << z
                  << ")" << std::endl;
    }
}

int VoxelGridView::getTerrainVoxel(int x, int y, int z) const {
    int local_x = x - x_offset;
    int local_y = y - y_offset;
    int local_z = z - z_offset;

    if (local_x >= 0 && local_x < width && local_y >= 0 && local_y < height && local_z >= 0 &&
        local_z < depth) {
        int retrievedTerrainId = terrainData[local_x + local_y * width + local_z * width * height];
        if (retrievedTerrainId != 0) {
            return retrievedTerrainId;
        }
    }
    return -1;
}

void VoxelGridView::setEntityVoxel(int x, int y, int z, const int voxelData) {
    int local_x = x - x_offset;
    int local_y = y - y_offset;
    int local_z = z - z_offset;

    if (local_x >= 0 && local_x < width && local_y >= 0 && local_y < height && local_z >= 0 &&
        local_z < depth) {
        entityData[local_x + local_y * width + local_z * width * height] = voxelData;
    } else {
        // Handle out-of-bounds access
        std::cerr << "Attempted to set voxel out of bounds at (" << x << ", " << y << ", " << z
                  << ")" << std::endl;
    }
}

int VoxelGridView::getEntityVoxel(int x, int y, int z) const {
    int local_x = x - x_offset;
    int local_y = y - y_offset;
    int local_z = z - z_offset;

    if (local_x >= 0 && local_x < width && local_y >= 0 && local_y < height && local_z >= 0 &&
        local_z < depth) {
        int retrievedEntityId = entityData[local_x + local_y * width + local_z * width * height];
        if (retrievedEntityId != 0) {
            return retrievedEntityId;
        }
    }
    return -1;
}

std::vector<int> VoxelGrid::getAllTerrainIdsInRegion(int x_min, int y_min, int z_min, int x_max,
                                                     int y_max, int z_max,
                                                     VoxelGridView& gridView) const {
    std::vector<int> result;

    // Use TerrainStorage's mainTypeGrid accessor for direct voxel access
    if (!terrainStorage || !terrainStorage->mainTypeGrid) {
        return result;
    }

    openvdb::Int32Grid::ConstAccessor accessor = terrainStorage->mainTypeGrid->getConstAccessor();

// Parallelize the outer loop (x) using OpenMP
#pragma omp parallel
    {
        // Each thread maintains its own local result vector
        std::vector<int> localResult;

#pragma omp for collapse(2)  // Parallelize both x and y loops
        for (int x = x_min; x <= x_max; ++x) {
            for (int y = y_min; y <= y_max; ++y) {
                for (int z = z_min; z <= z_max; ++z) {
                    openvdb::Coord coord(x, y, z);

                    // Check if the voxel is active and get its value
                    if (accessor.isValueOn(coord)) {
                        // Retrieve the terrain ID from the voxel value
                        int terrain_id = static_cast<int>(accessor.getValue(coord));

                        gridView.setTerrainVoxel(x, y, z, terrain_id);

                        localResult.emplace_back(terrain_id);
                    }
                }
            }
        }

// Merge each thread's local result vector into the shared result vector
#pragma omp critical
        result.insert(result.end(), localResult.begin(), localResult.end());
    }

    return result;
}

std::vector<int> VoxelGrid::getAllEntityIdsInRegion(int x_min, int y_min, int z_min, int x_max,
                                                    int y_max, int z_max,
                                                    VoxelGridView& gridView) const {
    std::vector<int> result;

    // Accessor for direct voxel access within the grid
    openvdb::Int32Grid::ConstAccessor accessor = entityGrid->getConstAccessor();

// Parallelize the outer loop (x) using OpenMP
#pragma omp parallel
    {
        // Each thread maintains its own local result vector
        std::vector<int> localResult;

#pragma omp for collapse(2)  // Parallelize both x and y loops
        for (int x = x_min; x <= x_max; ++x) {
            for (int y = y_min; y <= y_max; ++y) {
                for (int z = z_min; z <= z_max; ++z) {
                    openvdb::Coord coord(x, y, z);

                    // Check if the voxel is active and get its value
                    if (accessor.isValueOn(coord)) {
                        // Retrieve and cast the entity ID from the voxel value
                        int entity_id = static_cast<int>(accessor.getValue(coord));

                        gridView.setEntityVoxel(x, y, z, entity_id);

                        localResult.emplace_back(entity_id);
                    }
                }
            }
        }

// Merge each thread's local result vector into the shared result vector
#pragma omp critical
        result.insert(result.end(), localResult.begin(), localResult.end());
    }

    return result;
}

std::vector<int> VoxelGrid::getAllEventIdsInRegion(int x_min, int y_min, int z_min, int x_max,
                                                   int y_max, int z_max) const {
    std::vector<int> result;

    // Iterate over all active event voxels
    for (auto iter = eventGrid->cbeginValueOn(); iter; ++iter) {
        openvdb::Coord coord = iter.getCoord();

        // Bounding box check
        if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min && coord.y() <= y_max &&
            coord.z() >= z_min && coord.z() <= z_max) {
            // Retrieve and cast the entity ID from the voxel value
            int entity_id = static_cast<int>(iter.getValue());
            result.emplace_back(entity_id);
        }
    }

    return result;
}

std::vector<int> VoxelGrid::getAllLightingIdsInRegion(int x_min, int y_min, int z_min, int x_max,
                                                      int y_max, int z_max) const {
    std::vector<int> result;

    // Iterate over all active lighting voxels
    for (auto iter = lightingGrid->cbeginValueOn(); iter; ++iter) {
        openvdb::Coord coord = iter.getCoord();

        // Bounding box check
        if (coord.x() >= x_min && coord.x() <= x_max && coord.y() >= y_min && coord.y() <= y_max &&
            coord.z() >= z_min && coord.z() <= z_max) {
            // Retrieve and cast the entity ID from the voxel value
            int entity_id = static_cast<int>(iter.getValue());
            result.emplace_back(entity_id);
        }
    }

    return result;
}
