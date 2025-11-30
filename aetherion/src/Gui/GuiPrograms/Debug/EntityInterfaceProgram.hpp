#pragma once

#include "../../GuiCore/GuiProgram.hpp"
#include "EntityInterface.hpp"
#include <imgui.h>
#include <memory>

// Forward declare helper function
void RenderEntityInterfaceWindow(std::shared_ptr<EntityInterface> entityInterface);

/**
 * @brief Entity interface inspection program
 * 
 * Displays detailed information about a specific entity including its
 * components, position, velocity, health, and component mask.
 */
class EntityInterfaceProgram : public GuiProgram {
public:
    void render(GuiContext& context) override {
        if (!isActive_) return;
        
        if (ImGui::Begin("Entity Interface", &isActive_, ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderEntityInterfaceWindow(context.entityInterfacePtr);
        }
        ImGui::End();
    }
    
    std::string getId() const override { return "entity_interface"; }
    std::string getDisplayName() const override { return "Entity Interface"; }
};
