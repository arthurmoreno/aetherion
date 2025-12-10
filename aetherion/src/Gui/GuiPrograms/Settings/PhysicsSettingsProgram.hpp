#pragma once

#include <imgui.h>
#include <nanobind/nanobind.h>

#include "../../../PhysicsManager.hpp"
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

            // Reset button
            if (ImGui::Button("Reset to Defaults")) {
                physics->setGravity(5.0f);
                physics->setFriction(1.0f);
                physics->setAllowMultiDirection(true);
                gravity = physics->getGravity();
                friction = physics->getFriction();
                allowMultiDirection = physics->getAllowMultiDirection();
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
