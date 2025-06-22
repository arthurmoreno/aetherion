#include "PhysicsManager.hpp"

#include <iostream>

// Initialize the static instance pointer to nullptr
PhysicsManager* PhysicsManager::s_pInstance = nullptr;

// Retrieves the singleton instance
PhysicsManager* PhysicsManager::Instance() {
    if (s_pInstance == nullptr) {
        s_pInstance = new PhysicsManager();
    }
    return s_pInstance;
}

// Private constructor
PhysicsManager::PhysicsManager()
    : gravity(5.0f),   // Default gravity (m/s^2)
      friction(1.0f),  // Default friction coefficient
      allowMultiDirection(true) {
    // Initialization code (if any) goes here
    std::cout << "PhysicsManager initialized with Gravity: " << gravity
              << " and Friction: " << friction << std::endl;
}

// Private destructor
PhysicsManager::~PhysicsManager() {
    // Cleanup code (if any) goes here
}

// Setters
void PhysicsManager::setGravity(float g) {
    gravity = g;
    std::cout << "Gravity set to: " << gravity << std::endl;
}

void PhysicsManager::setFriction(float f) {
    friction = f;
    std::cout << "Friction set to: " << friction << std::endl;
}

void PhysicsManager::setAllowMultiDirection(bool amd) {
    allowMultiDirection = amd;
    std::cout << "AllowMultidirection set to: " << amd << std::endl;
}

void PhysicsManager::setMetabolismCostToApplyForce(float value) {
    metabolismCostToApplyForce = value;
    std::cout << "Metabolism cost to apply force set to: " << value << std::endl;
}

void PhysicsManager::setEvaporationCoefficient(float value) {
    EVAPORATION_COEFFICIENT = value;
    std::cout << "Evaporation coefficient set to: " << value << std::endl;
}

void PhysicsManager::setHeatToWaterEvaporation(float value) {
    HEAT_TO_WATER_EVAPORATION = value;
    std::cout << "Heat to water evaporation set to: " << value << std::endl;
}

void PhysicsManager::setWaterMinimumUnits(float value) {
    waterMinimumUnits = value;
    std::cout << "Water minimum units set to: " << value << std::endl;
}

// Getters
float PhysicsManager::getGravity() const { return gravity; }

float PhysicsManager::getFriction() const { return friction; }

bool PhysicsManager::getAllowMultiDirection() const { return allowMultiDirection; }

float PhysicsManager::getMetabolismCostToApplyForce() const { return metabolismCostToApplyForce; }

float PhysicsManager::getEvaporationCoefficient() const { return EVAPORATION_COEFFICIENT; }

float PhysicsManager::getHeatToWaterEvaporation() const { return HEAT_TO_WATER_EVAPORATION; }

float PhysicsManager::getWaterMinimumUnits() const { return waterMinimumUnits; }

// Optional: Load physics settings from a file
bool PhysicsManager::loadSettings(const std::string& fileName) {
    // Implement file loading logic here
    // For example, parse a config file and set gravity and friction
    // Return true if successful, false otherwise
    return false;
}

// Optional: Save physics settings to a file
bool PhysicsManager::saveSettings(const std::string& fileName) const {
    // Implement file saving logic here
    // For example, write gravity and friction to a config file
    // Return true if successful, false otherwise
    return false;
}