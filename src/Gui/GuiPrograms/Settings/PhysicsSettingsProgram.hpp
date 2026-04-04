#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../../physics/PhysicsManager.hpp"
#include "../../GuiCore/GuiProgram.hpp"

namespace nb = nanobind;

/**
 * @brief Physics settings configuration program
 *
 * Provides controls for adjusting physics parameters like gravity, friction,
 * and movement direction modes.
 */
class PhysicsSettingsProgram : public GuiProgram {
   public:
    void render(GuiContext& context) override {
        if (!isActive_) return;

        if (ImGui::Begin("Physics Settings", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            PhysicsManager* physics = PhysicsManager::Instance();

            // Display and edit Gravity
            static float gravity = physics->getGravity();
            static float friction = physics->getFriction();
            static bool allowMultiDirection = physics->getAllowMultiDirection();
            static bool simVaporCondensation = physics->getSimulateVaporCondensation();
            static bool simVaporMovement = physics->getSimulateVaporMovement();
            static bool simWaterMovement = physics->getSimulateWaterMovement();
            static bool simWaterEvaporation = physics->getSimulateWaterEvaporation();

            if (ImGui::InputFloat("Gravity (m/s²)", &gravity)) {
                physics->setGravity(gravity);
            }

            if (ImGui::InputFloat("Friction Coefficient", &friction)) {
                physics->setFriction(friction);
            }

            // Sliders for more intuitive control
            if (ImGui::SliderFloat("Gravity (m/s²) slide", &gravity, 0.0f, 20.0f)) {
                physics->setGravity(gravity);
            }

            if (ImGui::SliderFloat("Friction Coefficient slide", &friction, 0.0f, 10.0f)) {
                physics->setFriction(friction);
            }

            if (ImGui::Checkbox("Allow Multidirection", &allowMultiDirection)) {
                physics->setAllowMultiDirection(allowMultiDirection);
            }

            ImGui::Separator();
            ImGui::Text("Water Simulation Phases");

            if (ImGui::Checkbox("Vapor Condensation", &simVaporCondensation)) {
                physics->setSimulateVaporCondensation(simVaporCondensation);
            }
            if (ImGui::Checkbox("Vapor Movement", &simVaporMovement)) {
                physics->setSimulateVaporMovement(simVaporMovement);
            }
            if (ImGui::Checkbox("Water Movement", &simWaterMovement)) {
                physics->setSimulateWaterMovement(simWaterMovement);
            }
            if (ImGui::Checkbox("Water Evaporation", &simWaterEvaporation)) {
                physics->setSimulateWaterEvaporation(simWaterEvaporation);
            }

            // Reset button
            if (ImGui::Button("Reset to Defaults")) {
                physics->setGravity(5.0f);
                physics->setFriction(1.0f);
                physics->setAllowMultiDirection(true);
                physics->setSimulateVaporCondensation(true);
                physics->setSimulateVaporMovement(true);
                physics->setSimulateWaterMovement(true);
                physics->setSimulateWaterEvaporation(true);
                gravity = physics->getGravity();
                friction = physics->getFriction();
                allowMultiDirection = physics->getAllowMultiDirection();
                simVaporCondensation = true;
                simVaporMovement = true;
                simWaterMovement = true;
                simWaterEvaporation = true;
            }

            // Update context with current values
            context.physicsChanges["gravity"] = gravity;
            context.physicsChanges["friction"] = friction;
            context.physicsChanges["allowMultiDirection"] = allowMultiDirection;
        }
        ImGui::End();
    }

    std::string getId() const override { return "physics_settings"; }
    std::string getDisplayName() const override { return "Physics Settings"; }
};
