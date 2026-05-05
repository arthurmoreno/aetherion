// Implementation of the movement cluster of physics mutators
// (terrain/entity movement application; MovingComponent creation; zero-velocity
// cleanup).
//
// All declarations live in `physics/PhysicsMutators.hpp` so callers
// (`PhysicsEngine.cpp`, `EcosystemEngine.cpp`, ...) keep including a single
// header. Bodies are moved here one at a time, byte-identical to the prior
// inline definition (only the `inline` keyword is dropped on the signature
// line).

#include "physics/PhysicsMutators.hpp"
