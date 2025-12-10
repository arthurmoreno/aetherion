#pragma once

#include <imgui.h>

#include <memory>

#include "../../GuiCore/GuiProgram.hpp"
#include "World.hpp"

// Forward declare helper function
void RenderPlayerStatsWindow(std::shared_ptr<World> world_ptr);

/**
 * @brief Player character statistics display program
 *
 * Shows detailed player stats including position, velocity, physics stats,
 * and allows live editing of physics parameters.
 */
class PlayerStatsProgram : public GuiProgram {
   public:
    void render(GuiContext& context) override {
        if (!isActive_) return;

        if (ImGui::Begin("Player Stats", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderPlayerStatsWindow(context.worldPtr);
        }
        ImGui::End();
    }

    std::string getId() const override { return "player_stats"; }
    std::string getDisplayName() const override { return "Player Stats"; }
};
