#ifndef PHYSICS_SETTINGS_HPP
#define PHYSICS_SETTINGS_HPP

#include "PhysicsManager.hpp"

class PhysicsSettings {
   public:
    void setGravity(float g);
    void setFriction(float f);
    void setAllowMultiDirection(bool amd);

    float getGravity() const;
    float getFriction() const;
    bool getAllowMultiDirection() const;
};

#endif  // PHYSICS_SETTINGS_HPP
