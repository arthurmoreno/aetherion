#ifndef READONLY_QUERIES_HPP
#define READONLY_QUERIES_HPP

#include <tuple>

#include <entt/entt.hpp>

// Forward declarations
class VoxelGrid;
struct MoveSolidEntityEvent;
struct Position;
enum struct DirectionEnum;
struct EntityTypeComponent;

// Helper function to determine direction from velocity
int getDirection(float velocity);

// Helper: Get DirectionEnum from velocities
DirectionEnum getDirectionFromVelocities(float velocityX, float velocityY, float velocityZ);

// // Check if entity has collision at the target position
// bool hasCollision(entt::registry& registry, VoxelGrid& voxelGrid, entt::entity entity,
//                  int movingFromX, int movingFromY, int movingFromZ,
//                  int movingToX, int movingToY, int movingToZ, bool isTerrain);

// // Check for special collisions (e.g., ramps)
// std::tuple<bool, int, int, int> hasSpecialCollision(entt::registry& registry, VoxelGrid& voxelGrid,
//                                                     Position position, int movingToX, int movingToY,
//                                                     int movingToZ);

// // Check if entity can fall from current position
// bool checkIfCanFall(entt::registry& registry, VoxelGrid& voxelGrid, int i, int j, int k);

// Helper function to get EntityTypeComponent (defined in PhysicsEngine.cpp)
// EntityTypeComponent getEntityTypeComponent(entt::registry& registry, VoxelGrid& voxelGrid,
//                                            entt::entity entity, int x, int y, int z,
//                                            bool isTerrain);

#endif  // READONLY_QUERIES_HPP
