#ifndef PHYSICS_SETTINGS_HPP
#define PHYSICS_SETTINGS_HPP

#include "physics/PhysicsManager.hpp"

class PhysicsSettings {
public:
  void setGravity(float g);
  void setFriction(float f);
  void setAllowMultiDirection(bool amd);
  void setMetabolismCostToApplyForce(float value);
  void setEvaporationCoefficient(float value);
  void setHeatToWaterEvaporation(float value);
  void setWaterMinimumUnits(float value);
  void setSimulateVaporCondensation(bool value);
  void setSimulateVaporMovement(bool value);
  void setSimulateWaterMovement(bool value);
  void setSimulateWaterEvaporation(bool value);

  float getGravity() const;
  float getFriction() const;
  bool getAllowMultiDirection() const;
  bool getSimulateVaporCondensation() const;
  bool getSimulateVaporMovement() const;
  bool getSimulateWaterMovement() const;
  bool getSimulateWaterEvaporation() const;
};

#endif // PHYSICS_SETTINGS_HPP
