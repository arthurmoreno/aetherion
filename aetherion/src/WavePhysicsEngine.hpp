#ifndef WAVEPHYSICSENGINE_HPP
#define WAVEPHYSICSENGINE_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>

#include "components/PhysicsComponents.hpp"
#include "voxelgrid/VoxelGrid.hpp"

// Forward declarations
class VoxelGrid;

// Wave types for different behaviors
enum class WaveType {
    SOUND,  // Continuous waves that can superpose with other sound waves
    IMPACT  // Impulse waves that stop when colliding with other impact waves
};

// WaveComponent: represents a propagating wave packet through the voxel grid
struct WaveComponent {
    int32_t sourceId;          // entity that emitted the wave
    WaveType type;             // type of wave (sound or impact)
    float amplitude;           // current amplitude (strength)
    float frequency;           // sound frequency or wave type
    float speed;               // propagation speed (units per second)
    float attenuationPerUnit;  // amplitude decay per unit distance
    uint32_t lifetimeTicks;    // remaining simulation ticks before expiration

    // Direction vector for spherical propagation (normalized)
    float dirX, dirY, dirZ;

    // Distance traveled from source
    float distanceTraveled;

    WaveComponent()
        : sourceId(-1),
          type(WaveType::SOUND),
          amplitude(0.0f),
          frequency(440.0f),
          speed(1.0f),
          attenuationPerUnit(0.1f),
          lifetimeTicks(0),
          dirX(0.0f),
          dirY(0.0f),
          dirZ(0.0f),
          distanceTraveled(0.0f) {}
};

// WaveEmitter: attach to entities or terrain to emit waves on demand
struct WaveEmitterComponent {
    WaveType type;
    float initialAmplitude;
    float frequency;
    float speed;
    float attenuationPerUnit;
    uint32_t maxLifetimeTicks;
    bool isActive;  // Whether this emitter is currently active

    WaveEmitterComponent()
        : type(WaveType::SOUND),
          initialAmplitude(1.0f),
          frequency(440.0f),
          speed(1.0f),
          attenuationPerUnit(0.1f),
          maxLifetimeTicks(100),
          isActive(false) {}
};

// Receiver tag to mark entities that can "hear" or be affected by waves
struct WaveReceiverComponent {
    float hearingThreshold;  // minimum amplitude required to trigger event
    float maxHearingRange;   // maximum distance to detect waves

    WaveReceiverComponent() : hearingThreshold(0.1f), maxHearingRange(10.0f) {}
};

// Event dispatched when a wave is heard by a receiver
struct SoundHeardEvent {
    entt::entity listener;
    int32_t source;
    WaveType waveType;
    float frequency;
    float amplitude;

    SoundHeardEvent(entt::entity listener, int32_t source, WaveType waveType, float frequency,
                    float amplitude)
        : listener(listener),
          source(source),
          waveType(waveType),
          frequency(frequency),
          amplitude(amplitude) {}
};

// Event for wave impacts causing physics effects
struct WaveImpactEvent {
    entt::entity target;
    int32_t source;
    float impulseX, impulseY, impulseZ;
    float amplitude;

    WaveImpactEvent(entt::entity target, int32_t source, float impulseX, float impulseY,
                    float impulseZ, float amplitude)
        : target(target),
          source(source),
          impulseX(impulseX),
          impulseY(impulseY),
          impulseZ(impulseZ),
          amplitude(amplitude) {}
};

// Physics system to simulate wave propagation and interactions
class WavePhysicsEngine {
   public:
    WavePhysicsEngine() = default;

    // Called each tick to advance waves, dispatch sound and push effects
    void processWaves(entt::registry& registry, VoxelGrid& grid, entt::dispatcher& dispatcher,
                      uint32_t dtTicks);

    // Emit a new wave from a position
    void emitWave(VoxelGrid& grid, int x, int y, int z, const WaveComponent& wave);

    // Trigger wave emission for all active emitters
    void processEmitters(entt::registry& registry, VoxelGrid& grid);

    // Register event handlers
    void registerEventHandlers(entt::dispatcher& dispatcher);

   private:
    // Wave storage: coordinate hash -> vector of waves at that position
    std::unordered_map<uint64_t, std::vector<WaveComponent>> activeWaves;

    // Coordinate to hash key conversion
    uint64_t coordToKey(int x, int y, int z) const;
    void keyToCoord(uint64_t key, int& x, int& y, int& z) const;

    // Wave propagation methods
    void propagateWaves(entt::registry& registry, VoxelGrid& grid, entt::dispatcher& dispatcher,
                        uint32_t dtTicks);
    void propagateSpherical(const WaveComponent& wave, int originX, int originY, int originZ,
                            std::vector<std::tuple<int, int, int, WaveComponent>>& newWaves);

    // Wave interaction methods
    void handleWaveInteractions(std::vector<WaveComponent>& waves, int x, int y, int z);
    void processWaveInteractions(entt::registry& registry, entt::dispatcher& dispatcher, int x,
                                 int y, int z, const std::vector<WaveComponent>& waves);

    // Physics integration
    void applyPhysicsImpulse(entt::registry& registry, entt::dispatcher& dispatcher,
                             const WaveComponent& wave, int x, int y, int z, int sourceX,
                             int sourceY, int sourceZ);

    // Utility methods
    bool isValidCoordinate(VoxelGrid& grid, int x, int y, int z) const;
    float calculateAttenuation(const WaveComponent& wave, float distance) const;
    std::vector<std::tuple<int, int, int, float>> getSphericalNeighbors(int x, int y, int z,
                                                                        float radius) const;
};

#endif  // WAVEPHYSICSENGINE_HPP
