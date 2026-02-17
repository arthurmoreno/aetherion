#include "PhysicsSettings.hpp"

void PhysicsSettings::setGravity(float g) { PhysicsManager::Instance()->setGravity(g); }

void PhysicsSettings::setFriction(float f) { PhysicsManager::Instance()->setFriction(f); }

void PhysicsSettings::setAllowMultiDirection(bool amd) {
    PhysicsManager::Instance()->setAllowMultiDirection(amd);
}

void PhysicsSettings::setMetabolismCostToApplyForce(float value) {
    PhysicsManager::Instance()->setMetabolismCostToApplyForce(value);
}

void PhysicsSettings::setEvaporationCoefficient(float value) {
    PhysicsManager::Instance()->setEvaporationCoefficient(value);
}

void PhysicsSettings::setHeatToWaterEvaporation(float value) {
    PhysicsManager::Instance()->setHeatToWaterEvaporation(value);
}

void PhysicsSettings::setWaterMinimumUnits(float value) {
    PhysicsManager::Instance()->setWaterMinimumUnits(value);
}

float PhysicsSettings::getGravity() const { return PhysicsManager::Instance()->getGravity(); }

float PhysicsSettings::getFriction() const { return PhysicsManager::Instance()->getFriction(); }

bool PhysicsSettings::getAllowMultiDirection() const {
    return PhysicsManager::Instance()->getAllowMultiDirection();
}
