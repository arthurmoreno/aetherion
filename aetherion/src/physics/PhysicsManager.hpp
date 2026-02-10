#ifndef PHYSICS_MANAGER_H
#define PHYSICS_MANAGER_H

#include <map>
#include <string>

class PhysicsManager {
   public:
    // Retrieves the singleton instance
    static PhysicsManager* Instance();

    // Setters
    void setGravity(float g);
    void setFriction(float f);
    void setAllowMultiDirection(bool amd);
    void setMetabolismCostToApplyForce(float value);
    void setEvaporationCoefficient(float value);
    void setHeatToWaterEvaporation(float value);
    void setWaterMinimumUnits(float value);

    // Getters
    float getGravity() const;
    float getFriction() const;
    bool getAllowMultiDirection() const;
    float getMetabolismCostToApplyForce() const;
    float getEvaporationCoefficient() const;
    float getHeatToWaterEvaporation() const;
    float getWaterMinimumUnits() const;

    // Optional: Load physics settings from a file
    bool loadSettings(const std::string& fileName);

    // Optional: Save physics settings to a file
    bool saveSettings(const std::string& fileName) const;

   private:
    // Private constructor to prevent instantiation
    PhysicsManager();

    // Private destructor
    ~PhysicsManager();

    // Delete copy constructor and assignment operator to prevent copying
    PhysicsManager(const PhysicsManager&) = delete;
    PhysicsManager& operator=(const PhysicsManager&) = delete;

    // Static instance pointer
    static PhysicsManager* s_pInstance;

    // Physics variables
    float gravity;
    float friction;
    bool allowMultiDirection;

    float EVAPORATION_COEFFICIENT = 8.0f;
    float HEAT_TO_WATER_EVAPORATION = 120.0f;
    int waterMinimumUnits = 60'000;  // Good for a 10x100x100
    // int waterMinimumUnits = 120'000;
    // const int waterMinimumUnits = 8'000'000;  // 8 million is a lot of water

    // Constant that worked well:
    // Super easy to survive
    // const float metabolismCostToApplyForce = 0.00000001f;
    // Easier to survive
    // const float metabolismCostToApplyForce = 0.000001f;
    // Harder to survive
    float metabolismCostToApplyForce = 0.000002f;
    // Very Harder to survive
    // const float metabolismCostToApplyForce = 0.000005f;
    // const float metabolismCostToApplyForce = 0.00001f;

    // Optional: Additional physics parameters can be added here
};

typedef PhysicsManager ThePhysicsManager;

#endif  // PHYSICS_MANAGER_H