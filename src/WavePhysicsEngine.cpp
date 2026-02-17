#include "WavePhysicsEngine.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

// Coordinate to hash key conversion (simple 3D hash)
uint64_t WavePhysicsEngine::coordToKey(int x, int y, int z) const {
    // Ensure we handle negative coordinates properly
    uint64_t ux = static_cast<uint64_t>(x + 32768);  // Offset to handle negatives
    uint64_t uy = static_cast<uint64_t>(y + 32768);
    uint64_t uz = static_cast<uint64_t>(z + 32768);

    // Pack into 64-bit key (assuming coordinates fit in 16 bits each after offset)
    return (ux << 32) | (uy << 16) | uz;
}

void WavePhysicsEngine::keyToCoord(uint64_t key, int& x, int& y, int& z) const {
    uint64_t uz = key & 0xFFFF;
    uint64_t uy = (key >> 16) & 0xFFFF;
    uint64_t ux = (key >> 32) & 0xFFFF;

    x = static_cast<int>(ux) - 32768;
    y = static_cast<int>(uy) - 32768;
    z = static_cast<int>(uz) - 32768;
}

bool WavePhysicsEngine::isValidCoordinate(VoxelGrid& grid, int x, int y, int z) const {
    return x >= 0 && x < grid.width && y >= 0 && y < grid.height && z >= 0 && z < grid.depth;
}

float WavePhysicsEngine::calculateAttenuation(const WaveComponent& wave, float distance) const {
    // Apply distance-based attenuation
    float attenuatedAmplitude = wave.amplitude - (wave.attenuationPerUnit * distance);

    // Additional inverse square law for realistic wave attenuation
    if (distance > 1.0f) {
        attenuatedAmplitude *= 1.0f / (distance * distance);
    }

    return std::max(0.0f, attenuatedAmplitude);
}

std::vector<std::tuple<int, int, int, float>> WavePhysicsEngine::getSphericalNeighbors(
    int x, int y, int z, float radius) const {
    std::vector<std::tuple<int, int, int, float>> neighbors;

    // Generate points in a sphere around the origin
    int iRadius = static_cast<int>(std::ceil(radius));

    for (int dx = -iRadius; dx <= iRadius; ++dx) {
        for (int dy = -iRadius; dy <= iRadius; ++dy) {
            for (int dz = -iRadius; dz <= iRadius; ++dz) {
                if (dx == 0 && dy == 0 && dz == 0) continue;  // Skip origin

                float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (distance <= radius) {
                    neighbors.emplace_back(x + dx, y + dy, z + dz, distance);
                }
            }
        }
    }

    return neighbors;
}

void WavePhysicsEngine::emitWave(VoxelGrid& grid, int x, int y, int z, const WaveComponent& wave) {
    if (!isValidCoordinate(grid, x, y, z)) {
        return;
    }

    uint64_t key = coordToKey(x, y, z);
    activeWaves[key].push_back(wave);
}

void WavePhysicsEngine::processEmitters(entt::registry& registry, VoxelGrid& grid) {
    // Process all active wave emitters
    registry.view<WaveEmitterComponent, Position>().each(
        [&](auto entity, auto& emitter, auto& pos) {
            if (!emitter.isActive) return;

            // Create a new wave
            WaveComponent wave;
            wave.sourceId = static_cast<int32_t>(entity);
            wave.type = emitter.type;
            wave.amplitude = emitter.initialAmplitude;
            wave.frequency = emitter.frequency;
            wave.speed = emitter.speed;
            wave.attenuationPerUnit = emitter.attenuationPerUnit;
            wave.lifetimeTicks = emitter.maxLifetimeTicks;
            wave.distanceTraveled = 0.0f;

            // For initial emission, we don't need a specific direction
            // The spherical propagation will handle all directions
            wave.dirX = 0.0f;
            wave.dirY = 0.0f;
            wave.dirZ = 0.0f;

            emitWave(grid, pos.x, pos.y, pos.z, wave);

            // Deactivate single-shot emitters (can be reactivated externally)
            emitter.isActive = false;
        });
}

void WavePhysicsEngine::handleWaveInteractions(std::vector<WaveComponent>& waves, int x, int y,
                                               int z) {
    if (waves.empty()) return;

    // Separate sound waves and impact waves
    std::vector<WaveComponent*> soundWaves;
    std::vector<WaveComponent*> impactWaves;

    for (auto& wave : waves) {
        if (wave.type == WaveType::SOUND) {
            soundWaves.push_back(&wave);
        } else if (wave.type == WaveType::IMPACT) {
            impactWaves.push_back(&wave);
        }
    }

    // Handle sound wave superposition (combine amplitudes)
    if (soundWaves.size() > 1) {
        float totalAmplitude = 0.0f;
        float avgFrequency = 0.0f;

        for (auto* wave : soundWaves) {
            totalAmplitude += wave->amplitude;
            avgFrequency += wave->frequency;
        }

        avgFrequency /= soundWaves.size();

        // Keep only one sound wave with combined properties
        soundWaves[0]->amplitude = totalAmplitude;
        soundWaves[0]->frequency = avgFrequency;

        // Remove other sound waves
        waves.erase(std::remove_if(waves.begin(), waves.end(),
                                   [&](const WaveComponent& w) {
                                       return w.type == WaveType::SOUND && &w != soundWaves[0];
                                   }),
                    waves.end());
    }

    // Handle impact wave collisions (they cancel each other out)
    if (impactWaves.size() > 1) {
        // Remove all impact waves when they collide
        waves.erase(
            std::remove_if(waves.begin(), waves.end(),
                           [](const WaveComponent& w) { return w.type == WaveType::IMPACT; }),
            waves.end());
    }
}

void WavePhysicsEngine::processWaveInteractions(entt::registry& registry,
                                                entt::dispatcher& dispatcher, int x, int y, int z,
                                                const std::vector<WaveComponent>& waves) {
    // Check for entities at this position that can receive waves
    registry.view<WaveReceiverComponent, Position>().each(
        [&](auto entity, auto& receiver, auto& pos) {
            if (pos.x == x && pos.y == y && pos.z == z) {
                for (const auto& wave : waves) {
                    if (wave.amplitude >= receiver.hearingThreshold) {
                        dispatcher.enqueue<SoundHeardEvent>(entity, wave.sourceId, wave.type,
                                                            wave.frequency, wave.amplitude);
                    }
                }
            }
        });

    // Apply physics impulses to entities with velocity at this position
    registry.view<Velocity, Position>().each([&](auto entity, auto& velocity, auto& pos) {
        if (pos.x == x && pos.y == y && pos.z == z) {
            for (const auto& wave : waves) {
                if (wave.type == WaveType::IMPACT && wave.sourceId != -1) {
                    // Get source position to calculate impulse direction
                    if (registry.valid(entt::entity(wave.sourceId))) {
                        auto* sourcePos = registry.try_get<Position>(entt::entity(wave.sourceId));
                        if (sourcePos) {
                            applyPhysicsImpulse(registry, dispatcher, wave, x, y, z, sourcePos->x,
                                                sourcePos->y, sourcePos->z);
                        }
                    }
                }
            }
        }
    });
}

void WavePhysicsEngine::applyPhysicsImpulse(entt::registry& registry, entt::dispatcher& dispatcher,
                                            const WaveComponent& wave, int x, int y, int z,
                                            int sourceX, int sourceY, int sourceZ) {
    // Calculate impulse direction from source to target
    float dx = float(x - sourceX);
    float dy = float(y - sourceY);
    float dz = float(z - sourceZ);

    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance < 1e-6f) return;  // Avoid division by zero

    // Normalize direction
    dx /= distance;
    dy /= distance;
    dz /= distance;

    // Calculate impulse magnitude based on wave amplitude
    float impulseMagnitude = wave.amplitude * 0.1f;  // Scale factor for physics integration

    // Apply impulse to all entities at this position with velocity
    registry.view<Velocity, Position>().each([&](auto entity, auto& velocity, auto& pos) {
        if (pos.x == x && pos.y == y && pos.z == z) {
            velocity.vx += dx * impulseMagnitude;
            velocity.vy += dy * impulseMagnitude;
            velocity.vz += dz * impulseMagnitude;

            // Dispatch impact event for additional handling
            dispatcher.enqueue<WaveImpactEvent>(entity, wave.sourceId, dx * impulseMagnitude,
                                                dy * impulseMagnitude, dz * impulseMagnitude,
                                                wave.amplitude);
        }
    });
}

void WavePhysicsEngine::propagateSpherical(
    const WaveComponent& wave, int originX, int originY, int originZ,
    std::vector<std::tuple<int, int, int, WaveComponent>>& newWaves) {
    // Calculate propagation radius based on wave speed
    float propagationRadius = wave.speed;

    // Get all neighboring points within the propagation radius
    auto neighbors = getSphericalNeighbors(originX, originY, originZ, propagationRadius);

    for (const auto& [nx, ny, nz, distance] : neighbors) {
        // Create new wave component for this neighbor
        WaveComponent newWave = wave;

        // Update wave properties
        newWave.distanceTraveled += distance;
        newWave.amplitude = calculateAttenuation(wave, distance);

        // Set direction vector for this specific propagation direction
        float dx = float(nx - originX);
        float dy = float(ny - originY);
        float dz = float(nz - originZ);
        float normDist = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (normDist > 1e-6f) {
            newWave.dirX = dx / normDist;
            newWave.dirY = dy / normDist;
            newWave.dirZ = dz / normDist;
        }

        // Decrease lifetime
        if (newWave.lifetimeTicks > 0) {
            newWave.lifetimeTicks--;
        }

        // Only add if wave still has energy and lifetime
        if (newWave.amplitude > 0.001f && newWave.lifetimeTicks > 0) {
            newWaves.emplace_back(nx, ny, nz, newWave);
        }
    }
}

void WavePhysicsEngine::propagateWaves(entt::registry& registry, VoxelGrid& grid,
                                       entt::dispatcher& dispatcher, uint32_t dtTicks) {
    std::unordered_map<uint64_t, std::vector<WaveComponent>> nextFrameWaves;

    // Process all active waves
    for (auto& [key, waves] : activeWaves) {
        if (waves.empty()) continue;

        int x, y, z;
        keyToCoord(key, x, y, z);

        // Handle wave interactions at this position
        handleWaveInteractions(waves, x, y, z);

        // Process entity interactions
        processWaveInteractions(registry, dispatcher, x, y, z, waves);

        // Propagate each wave
        for (const auto& wave : waves) {
            if (wave.amplitude <= 0.001f || wave.lifetimeTicks <= 0) {
                continue;  // Wave has died out
            }

            std::vector<std::tuple<int, int, int, WaveComponent>> newWaves;
            propagateSpherical(wave, x, y, z, newWaves);

            // Add new waves to next frame
            for (const auto& [nx, ny, nz, newWave] : newWaves) {
                if (isValidCoordinate(grid, nx, ny, nz)) {
                    uint64_t newKey = coordToKey(nx, ny, nz);
                    nextFrameWaves[newKey].push_back(newWave);
                }
            }
        }
    }

    // Replace active waves with next frame waves
    activeWaves = std::move(nextFrameWaves);
}

void WavePhysicsEngine::processWaves(entt::registry& registry, VoxelGrid& grid,
                                     entt::dispatcher& dispatcher, uint32_t dtTicks) {
    // Process emitters first
    processEmitters(registry, grid);

    // Then propagate existing waves
    propagateWaves(registry, grid, dispatcher, dtTicks);
}

void WavePhysicsEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    // Register handlers for wave-related events
    // This can be extended to handle specific events that trigger wave emissions

    // Example: handle explosion events that create impact waves
    // dispatcher.sink<ExplosionEvent>().connect<&WavePhysicsEngine::onExplosionEvent>(*this);
}
