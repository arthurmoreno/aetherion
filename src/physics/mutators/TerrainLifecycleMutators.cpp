// Implementation of the terrain-lifecycle cluster of physics mutators.
//
// All declarations live in `physics/PhysicsMutators.hpp` so callers
// (`PhysicsEngine.cpp`, `EcosystemEngine.cpp`, ...) keep including a single
// header. Bodies are moved here one at a time, byte-identical to the prior
// inline definition (only the `inline` keyword is dropped on the signature
// line).

#include "physics/PhysicsMutators.hpp"

void _handleInvalidTerrainFound(EventSink &sink, VoxelGrid &voxelGrid,
                                const InvalidTerrainFoundEvent &event) {
  voxelGrid.deleteTerrain(sink, event.x, event.y, event.z);
}
