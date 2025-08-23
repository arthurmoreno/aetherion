#include <iostream>
#include <cassert>
#include <random>
#include <vector>
#include <algorithm>

#include "terrain/TerrainStorage.hpp"
#include "terrain/TerrainGridRepository.hpp"
#include "components/TerrainComponents.hpp"

// Include entt for the repository test
#include <entt/entt.hpp>

/**
 * Minimal Water Simulation Test using TerrainStorage Iterators
 * 
 * This test demonstrates the new iterator-based approach for water simulation
 * replacing the ECS-based approach shown in EcosystemEngine::loopTiles().
 * 
 * The test creates a small terrain grid, adds water matter, and uses the new
 * iterators to process water distribution and evaporation in a simplified way.
 */

// Mock PhysicsManager singleton for testing
class MockPhysicsManager {
public:
    static MockPhysicsManager* Instance() {
        static MockPhysicsManager instance;
        return &instance;
    }
    
    int getWaterMinimumUnits() const { return 100; }
    float getEvaporationCoefficient() const { return 0.1f; }
    float getHeatToWaterEvaporation() const { return 10.0f; }
    
private:
    MockPhysicsManager() = default;
};

// Mock the PhysicsManager::Instance() call
#define PhysicsManager MockPhysicsManager

// Simple water distribution logic using iterators
class WaterSimulator {
private:
    TerrainStorage& storage_;
    std::mt19937 gen_;
    
public:
    WaterSimulator(TerrainStorage& storage) 
        : storage_(storage), gen_(std::random_device{}()) {}
    
    // Simulate water flowing downward (simple gravity)
    void simulateWaterFlow() {
        std::vector<std::tuple<int, int, int, int>> waterSources;
        
        // Collect all water locations
        storage_.iterateWaterMatter([&](int x, int y, int z, int amount) {
            if (amount > 1) { // Only sources with more than 1 unit can flow
                waterSources.emplace_back(x, y, z, amount);
            }
        });
        
        // Simulate water flowing down
        for (auto [x, y, z, amount] : waterSources) {
            if (z > 0) { // Can flow down
                int belowWater = storage_.getTerrainWaterMatter(x, y, z - 1);
                if (belowWater < 10) { // Target not full (max 10 units per voxel)
                    int flowAmount = std::min(amount / 2, 10 - belowWater);
                    if (flowAmount > 0) {
                        storage_.setTerrainWaterMatter(x, y, z, amount - flowAmount);
                        storage_.setTerrainWaterMatter(x, y, z - 1, belowWater + flowAmount);
                    }
                }
            }
        }
    }
    
    // Simulate basic evaporation
    void simulateEvaporation(float sunIntensity) {
        if (sunIntensity <= 0.0f) return;
        
        std::vector<std::tuple<int, int, int, int>> evaporationCandidates;
        
        // Collect surface water (highest z-level with water)
        storage_.iterateWaterMatter([&](int x, int y, int z, int amount) {
            if (amount > 0) {
                evaporationCandidates.emplace_back(x, y, z, amount);
            }
        });
        
        // Simple evaporation: convert water to vapor
        std::uniform_real_distribution<> evapDis(0.0, 1.0);
        for (auto [x, y, z, amount] : evaporationCandidates) {
            if (evapDis(gen_) < sunIntensity * 0.1) { // 10% chance per sun intensity unit
                if (amount > 0) {
                    storage_.setTerrainWaterMatter(x, y, z, amount - 1);
                    int currentVapor = storage_.getTerrainVaporMatter(x, y, z + 1);
                    storage_.setTerrainVaporMatter(x, y, z + 1, currentVapor + 1);
                }
            }
        }
    }
    
    // Count total water and vapor in the system
    std::pair<int, int> countWaterAndVapor() {
        int totalWater = 0, totalVapor = 0;
        
        storage_.iterateWaterMatter([&](int x, int y, int z, int amount) {
            totalWater += amount;
        });
        
        storage_.iterateVaporMatter([&](int x, int y, int z, int amount) {
            totalVapor += amount;
        });
        
        return {totalWater, totalVapor};
    }
    
    // Demonstrate random iterator (if we implement it later)
    void randomWaterProcessing() {
        std::vector<std::tuple<int, int, int, int>> allWaterVoxels;
        
        // Collect all water voxels
        storage_.iterateWaterMatter([&](int x, int y, int z, int amount) {
            if (amount > 0) {
                allWaterVoxels.emplace_back(x, y, z, amount);
            }
        });
        
        // Shuffle for random processing (like EcosystemEngine does)
        std::shuffle(allWaterVoxels.begin(), allWaterVoxels.end(), gen_);
        
        // Process in random order
        int processed = 0;
        for (auto [x, y, z, amount] : allWaterVoxels) {
            // Simple processing: add heat or modify water
            storage_.setTerrainWaterMatter(x, y, z, amount); // Keep same for now
            
            processed++;
            if (processed >= 100) break; // Limit processing like EcosystemEngine
        }
    }
};

// Test functions
void testBasicWaterIteration() {
    std::cout << "Testing basic water iteration..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize(); // No parameters
    
    // Add some water at different locations
    storage.setTerrainWaterMatter(5, 5, 4, 10);  // Top level water source
    storage.setTerrainWaterMatter(3, 3, 2, 5);   // Mid level water
    storage.setTerrainWaterMatter(7, 7, 1, 3);   // Low level water
    
    int waterCount = 0;
    int totalWater = 0;
    
    storage.iterateWaterMatter([&](int x, int y, int z, int amount) {
        waterCount++;
        totalWater += amount;
        std::cout << "  Water at (" << x << "," << y << "," << z << "): " << amount << " units" << std::endl;
    });
    
    assert(waterCount == 3);
    assert(totalWater == 18);
    std::cout << "✓ Basic water iteration test passed!" << std::endl;
}

void testWaterFlowSimulation() {
    std::cout << "Testing water flow simulation..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize();
    WaterSimulator simulator(storage);
    
    // Create a water tower: water at top should flow down
    storage.setTerrainWaterMatter(2, 2, 4, 8); // High water source
    
    auto [initialWater, initialVapor] = simulator.countWaterAndVapor();
    std::cout << "  Initial: " << initialWater << " water, " << initialVapor << " vapor" << std::endl;
    
    // Simulate several flow cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        simulator.simulateWaterFlow();
    }
    
    auto [finalWater, finalVapor] = simulator.countWaterAndVapor();
    std::cout << "  After flow: " << finalWater << " water, " << finalVapor << " vapor" << std::endl;
    
    // Water should have spread down
    assert(finalWater == initialWater); // Conservation of mass
    
    // Check if water moved down
    int waterAtBottom = storage.getTerrainWaterMatter(2, 2, 0);
    std::cout << "  Water at bottom: " << waterAtBottom << " units" << std::endl;
    assert(waterAtBottom > 0); // Some water should have flowed down
    
    std::cout << "✓ Water flow simulation test passed!" << std::endl;
}

void testEvaporationSimulation() {
    std::cout << "Testing evaporation simulation..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize();
    WaterSimulator simulator(storage);
    
    // Add water for evaporation
    storage.setTerrainWaterMatter(2, 2, 2, 6);
    
    float sunIntensity = 2.0f; // Strong sun
    
    auto [initialWater, initialVapor] = simulator.countWaterAndVapor();
    std::cout << "  Initial: " << initialWater << " water, " << initialVapor << " vapor" << std::endl;
    
    // Run evaporation cycles
    for (int cycle = 0; cycle < 20; cycle++) {
        simulator.simulateEvaporation(sunIntensity);
    }
    
    auto [finalWater, finalVapor] = simulator.countWaterAndVapor();
    std::cout << "  After evaporation: " << finalWater << " water, " << finalVapor << " vapor" << std::endl;
    
    // Should have some evaporation
    assert(finalVapor > initialVapor);
    assert(finalWater + finalVapor == initialWater); // Conservation of mass
    
    std::cout << "✓ Evaporation simulation test passed!" << std::endl;
}

void testRandomProcessing() {
    std::cout << "Testing random processing (like EcosystemEngine)..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize();
    WaterSimulator simulator(storage);
    
    // Create multiple water sources
    for (int i = 0; i < 10; i++) {
        int x = i % 8;
        int y = (i * 2) % 8;
        int z = i % 3;
        storage.setTerrainWaterMatter(x, y, z, (i % 5) + 1);
    }
    
    auto [initialWater, initialVapor] = simulator.countWaterAndVapor();
    std::cout << "  Initial water sources: " << initialWater << " units" << std::endl;
    
    // Process randomly (demonstrates the pattern from EcosystemEngine)
    simulator.randomWaterProcessing();
    
    auto [finalWater, finalVapor] = simulator.countWaterAndVapor();
    std::cout << "  After random processing: " << finalWater << " water" << std::endl;
    
    assert(finalWater == initialWater); // No change in this simple test
    
    std::cout << "✓ Random processing test passed!" << std::endl;
}

void testMinimumWaterGeneration() {
    std::cout << "Testing minimum water generation (like EcosystemEngine)..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize();
    WaterSimulator simulator(storage);
    
    // Start with very little water
    storage.setTerrainWaterMatter(5, 5, 2, 2);
    
    auto [initialWater, initialVapor] = simulator.countWaterAndVapor();
    int totalMatter = initialWater + initialVapor;
    
    const int waterMinimumUnits = MockPhysicsManager::Instance()->getWaterMinimumUnits();
    std::cout << "  Initial total matter: " << totalMatter << ", minimum required: " << waterMinimumUnits << std::endl;
    
    if (totalMatter < waterMinimumUnits) {
        int waterToCreate = waterMinimumUnits - totalMatter;
        std::cout << "  Need to create " << waterToCreate << " units of water" << std::endl;
        
        // Add vapor at high altitude (like EcosystemEngine does)
        std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> disX(0, 9);
        std::uniform_int_distribution<> disY(0, 9);
        
        while (waterToCreate > 0) {
            int vaporUnits = std::min(waterToCreate, 10);
            int x = disX(gen);
            int y = disY(gen);
            int z = 4; // Top level
            
            int currentVapor = storage.getTerrainVaporMatter(x, y, z);
            storage.setTerrainVaporMatter(x, y, z, currentVapor + vaporUnits);
            waterToCreate -= vaporUnits;
        }
    }
    
    auto [finalWater, finalVapor] = simulator.countWaterAndVapor();
    int finalTotal = finalWater + finalVapor;
    std::cout << "  Final total matter: " << finalTotal << std::endl;
    
    assert(finalTotal >= waterMinimumUnits);
    
    std::cout << "✓ Minimum water generation test passed!" << std::endl;
}

void testHighLevelIterators() {
    std::cout << "Testing high-level repository iterators..." << std::endl;
    
    TerrainStorage storage;
    storage.initialize();
    
    // Create ECS registry for the repository
    entt::registry registry;
    TerrainGridRepository repository(registry, storage);
    
    // Set up some water data using TerrainStorage
    storage.setTerrainWaterMatter(1, 1, 1, 5);
    storage.setTerrainVaporMatter(2, 2, 2, 3);
    storage.setTerrainBiomassMatter(3, 3, 1, 7);
    
    // Test high-level water iterator (includes ECS integration)
    int waterCount = 0;
    repository.iterateWaterMatter([&](int x, int y, int z, int amount, const TerrainInfo& info) {
        waterCount++;
        std::cout << "  High-level water at (" << x << "," << y << "," << z 
                  << "): " << amount << " units, matter total: " 
                  << info.stat.matter.WaterMatter << std::endl;
        assert(amount == info.stat.matter.WaterMatter); // Should match
    });
    
    // Test vapor iterator
    int vaporCount = 0;
    repository.iterateVaporMatter([&](int x, int y, int z, int amount, const TerrainInfo& info) {
        vaporCount++;
        std::cout << "  High-level vapor at (" << x << "," << y << "," << z 
                  << "): " << amount << " units" << std::endl;
        assert(amount == info.stat.matter.WaterVapor);
    });
    
    assert(waterCount == 1);
    assert(vaporCount == 1);
    
    std::cout << "✓ High-level iterators test passed!" << std::endl;
}

int main() {
    std::cout << "=== Terrain Storage Water Simulation C++ Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        // Basic iterator functionality tests
        testBasicWaterIteration();
        std::cout << std::endl;
        
        // Water simulation tests using iterators
        testWaterFlowSimulation();
        std::cout << std::endl;
        
        testEvaporationSimulation();
        std::cout << std::endl;
        
        testRandomProcessing();
        std::cout << std::endl;
        
        testMinimumWaterGeneration();
        std::cout << std::endl;
        
        // Test high-level repository iterators
        testHighLevelIterators();
        std::cout << std::endl;
        
        std::cout << "🎉 All tests passed successfully!" << std::endl;
        std::cout << std::endl;
        std::cout << "This demonstrates the new iterator-based approach for water simulation" << std::endl;
        std::cout << "that can replace the ECS-based iteration shown in EcosystemEngine::loopTiles()." << std::endl;
        std::cout << "Benefits:" << std::endl;
        std::cout << "- Direct access to terrain data without ECS overhead" << std::endl;
        std::cout << "- Sparse iteration over only occupied voxels" << std::endl;
        std::cout << "- Efficient OpenVDB-based storage and access patterns" << std::endl;
        std::cout << "- Template-based callbacks for flexible processing" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
