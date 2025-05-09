#include "ItemWindow.hpp"

#include <nanobind/nanobind.h>

#include <algorithm>
#include <iostream>

#include "GuiStateManager.hpp"
#include "LowLevelRenderer/TextureManager.hpp"
void RenderItemSlot(std::vector<InventoryItem>& items, int index, int& selected_index,
                    const ImVec2& buttonSize, const std::string& button_prefix,
                    const std::string& payload_type, const std::string& window_id,
                    nb::list& commands, const std::function<void(int, InventoryItem&)>& customLogic,
                    bool showHotkeyNumbers) {
    InventoryItem& current_item = items[index];

    // Push a unique ID for each item to avoid ID conflicts
    ImGui::PushID(index);

    // Determine if the slot is empty
    bool isEmptySlot = (current_item.texture == nullptr) || (current_item.quantity == 0) ||
                       (current_item.name == "empty_slot");

    // Generate a unique identifier string for the button
    std::string button_id = button_prefix + std::to_string(index);

    bool isSelected = (selected_index == index);

    // If the current item is selected
    if (isSelected) {
        // Push style for border
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));  // Yellow border
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);  // Increase border size
    }

    bool clicked = false;

    // Render the item as an ImageButton
    ImTextureID textureID = reinterpret_cast<ImTextureID>(current_item.texture);
    clicked = ImGui::ImageButton(button_id.c_str(), textureID, buttonSize);

    // Render hotkey number if applicable
    if (showHotkeyNumbers) {
        // Get the position and size of the ImageButton
        ImVec2 pos = ImGui::GetItemRectMin();
        // Calculate the position to render the index number (e.g., top-left corner)
        ImVec2 textPos = ImVec2(pos.x + 2, pos.y + 2);  // Adjust as needed

        // Map index to hotkey number (e.g., index 0 corresponds to hotkey '1', index 9 to '0')
        std::string hotkeyStr;
        if (index == 9)
            hotkeyStr = "0";
        else
            hotkeyStr = std::to_string(index + 1);

        // Use ImGui::GetWindowDrawList()->AddText() to draw over the ImageButton
        ImGui::GetWindowDrawList()->AddText(textPos, ImColor(255, 255, 255), hotkeyStr.c_str());
    }

    // Handle clicks
    if (clicked) {
        if (isEmptySlot) {
            // Optionally handle clicks on empty slots
        } else {
            if (selected_index == index) {
                // Deselect if already selected
                selected_index = -1;
            } else {
                // Select the clicked item
                selected_index = index;
                std::cout << "Selected Item: " << current_item.name << std::endl;
            }
        }
    }

    // Pop the border style if the item was selected
    if (isSelected) {
        ImGui::PopStyleVar();    // Pop border size
        ImGui::PopStyleColor();  // Pop border color
    }

    // For non-empty slots, handle tooltips and drag-and-drop
    if (!isEmptySlot) {
        // Display a tooltip when hovering over the item
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Item: %s", current_item.name.c_str());
            ImGui::Text("Quantity: %d", current_item.quantity);
            ImGui::EndTooltip();
        }

        // Begin Drag Source for the item
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            // Create a struct to hold the payload data
            DragPayload payload_data = {index};
            strncpy(payload_data.window_id, window_id.c_str(), sizeof(payload_data.window_id) - 1);
            payload_data.window_id[sizeof(payload_data.window_id) - 1] =
                '\0';  // Ensure null-termination

            // Set the payload with the index and window_id
            ImGui::SetDragDropPayload(payload_type.c_str(), &payload_data, sizeof(DragPayload));
            ImGui::Text("%s", current_item.name.c_str());
            ImGui::EndDragDropSource();

            // **Set dragging state**
            GuiStateManager::Instance()->isDraggingFromUI = true;
            GuiStateManager::Instance()->draggedItemIndex = index;
            GuiStateManager::Instance()->src_window_id = window_id;
        }
    }

    // Accept dropped items (even on empty slots)
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type.c_str())) {
            if (payload->DataSize == sizeof(DragPayload)) {
                const DragPayload* drag_payload = static_cast<const DragPayload*>(payload->Data);
                int src_index = drag_payload->index;
                std::string src_window_id(drag_payload->window_id);

                // Ensure the source and target indices are valid and different
                if (!(src_window_id == window_id && src_index == index)) {
                    // Record the move command
                    nb::dict command;
                    command["type"] = "move_item";
                    command["src_window"] = src_window_id;
                    command["src_index"] = src_index;
                    command["dst_window"] = window_id;
                    command["dst_index"] = index;
                    commands.append(command);

                    // **Do not move the items directly here**

                    std::cout << "Generated move_item command: from " << src_window_id << "["
                              << src_index << "] to " << window_id << "[" << index << "]"
                              << std::endl;
                }

                GuiStateManager::Instance()->isDraggingFromUI = false;
                GuiStateManager::Instance()->draggedItemIndex = -1;
                GuiStateManager::Instance()->src_window_id.clear();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Execute custom logic if provided
    if (customLogic) {
        customLogic(index, current_item);
    }

    // Pop the unique ID
    ImGui::PopID();
}