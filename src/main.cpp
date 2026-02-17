#include <iostream>

#include "EntityInterface.hpp"
#include "World.hpp"

int main() {
    // Create an instance of the World class
    World world(10, 10, 10);

    // Initialize the voxel grid
    std::cout << "Initializing voxel grid..." << std::endl;
    world.initializeVoxelGrid();

    // Create a new entity in the world
    std::cout << "Creating entity..." << std::endl;
    // EntityTypeComponent entityType = EntityTypeComponent(1, 1);
    Position pos = {10, 20, 30, DirectionEnum::DOWN};
    // Velocity velocity = Velocity(0, 0, 0);
    // EntityInterface entityInterface = EntityInterface(entityType, position,
    // velocity); entt::entity entity = world.createEntity(entityInterface);

    // // Set some data in the voxel grid
    // std::cout << "Setting voxel data..." << std::endl;
    // GridData data = {1, static_cast<int>(entity), 3, 0.5f};
    // world.setVoxel(1, 2, 3, data);

    // // Retrieve and print the voxel data
    // std::cout << "Retrieving voxel data..." << std::endl;
    // GridData retrievedData = world.getVoxel(1, 2, 3);
    // std::cout << "Terrain ID: " << retrievedData.terrainID << std::endl;
    // std::cout << "Entity ID: " << retrievedData.entityID << std::endl;
    // std::cout << "Event ID: " << retrievedData.eventID << std::endl;
    // std::cout << "Lighting Level: " << retrievedData.lightingLevel <<
    // std::endl;

    // // Update the world
    // std::cout << "Updating world..." << std::endl;
    // world.update();

    // // Remove the entity
    // std::cout << "Removing entity..." << std::endl;
    // world.removeEntity(entity);

    std::cout << "Test complete!" << std::endl;

    return 0;
}