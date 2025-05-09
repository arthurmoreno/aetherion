#ifndef __ITEM_WINDOW__
#define __ITEM_WINDOW__

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>  // Updated backend header
#include <imgui/imgui.h>
#include <nanobind/nanobind.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

namespace nb = nanobind;

class GuiStateManager;

// Structure to hold inventory items
struct InventoryItem {
    std::string name;
    std::string textureId;
    SDL_Texture* texture;
    int quantity;

    // Constructor for convenience
    InventoryItem(const std::string& itemName, const std::string& texId, SDL_Texture* tex, int qty)
        : name(itemName), textureId(texId), texture(tex), quantity(qty) {}
};

struct DragPayload {
    int index;
    char window_id[64];
};

void RenderItemSlot(std::vector<InventoryItem>& items, int index, int& selected_index,
                    const ImVec2& buttonSize, const std::string& button_prefix,
                    const std::string& payload_type, const std::string& window_id,
                    nb::list& commands, const std::function<void(int, InventoryItem&)>& customLogic,
                    bool showHotkeyNumbers);

class ItemWindow {
   public:
    std::vector<InventoryItem>* items;
    nb::list* commands;
    int selected_index = -1;  // -1 indicates no selection

    // Default constructor initializes pointers to nullptr
    ItemWindow() : items(nullptr), commands(nullptr) {}

    // Parameterized constructor initializes pointers to the provided objects
    ItemWindow(std::vector<InventoryItem>& items, nb::list& commands)
        : items(&items), commands(&commands) {}

    void setItems(std::vector<InventoryItem>& items) { this->items = &items; }
    void setCommands(nb::list& commands) { this->commands = &commands; }

    virtual void Render() = 0;  // Pure virtual function to be implemented by derived classes
};

class InventoryWindow : public ItemWindow {
   public:
    int columns = 10;  // Number of columns in the inventory grid

    InventoryWindow() : ItemWindow() {}

    InventoryWindow(std::vector<InventoryItem>& items, nb::list& commands)
        : ItemWindow(items, commands) {}

    void Render() override {
        // Begin a table with 'columns' columns
        if (ImGui::BeginTable("InventoryTable", columns,
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody)) {
            for (size_t i = 0; i < items->size(); ++i) {
                // Move to the next row every 'columns' items
                if (i > 0 && i % columns == 0) {
                    ImGui::TableNextRow();
                }

                // Move to the next column
                ImGui::TableNextColumn();

                // Set up parameters for the shared function
                ImVec2 buttonSize = ImVec2(32, 32);
                std::string button_prefix = "InventoryItem_";
                std::string payload_type = "DND_ITEM";

                // Render the item slot
                RenderItemSlot(*items, i, selected_index, buttonSize, button_prefix, payload_type,
                               "inventory", *commands, nullptr, false);
            }
            ImGui::EndTable();
        }

        // Display details of the selected item below the inventory grid
        if (selected_index >= 0 && selected_index < static_cast<int>(items->size())) {
            const InventoryItem& selected_item = (*items)[selected_index];
            bool isSelectedItemEmpty = (selected_item.texture == nullptr) ||
                                       (selected_item.quantity == 0) ||
                                       (selected_item.name == "empty_slot");

            if (!isSelectedItemEmpty) {
                ImGui::Separator();
                ImGui::Text("Selected Item:");
                ImGui::Text("Name: %s", selected_item.name.c_str());
                ImGui::Text("Quantity: %d", selected_item.quantity);
            }
        }
    }
};

class HotbarWindow : public ItemWindow {
   public:
    static const int HOTBAR_SIZE = 10;  // Number of items in the hotbar

    HotbarWindow() : ItemWindow() {}

    HotbarWindow(std::vector<InventoryItem>& items, nb::list& commands)
        : ItemWindow(items, commands) {}

    void Render() override {
        // Ensure we have enough items in the inventory for the hotbar
        size_t hotbar_size = std::min(items->size(), static_cast<size_t>(HOTBAR_SIZE));

        // Window flags to create a borderless, unmovable window
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        // Position the hotbar at the bottom center of the screen
        ImVec2 window_pos =
            ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y - 10);
        ImVec2 window_pos_pivot = ImVec2(0.5f, 1.0f);  // Center bottom
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);

        // Begin the hotbar window
        ImGui::Begin("Hotbar", NULL, window_flags);

        // Create a horizontal layout for the hotbar items
        for (size_t i = 0; i < hotbar_size; ++i) {
            // Set up parameters for the shared function
            ImVec2 buttonSize = ImVec2(48, 48);
            std::string button_prefix = "HotbarItem_";
            std::string payload_type = "DND_ITEM";

            // Render the item slot
            RenderItemSlot(*items, i, selected_index, buttonSize, button_prefix, payload_type,
                           "hotbar", *commands, nullptr, true);

            // Place items horizontally
            if (i < hotbar_size - 1) {
                ImGui::SameLine();
            }
        }

        ImGui::End();  // End of hotbar window
    }
};

class EquipmentWindow : public ItemWindow {
   public:
    // Mapping of equipment slot names to item indices
    std::unordered_map<std::string, int> slotIndices;

    EquipmentWindow() : ItemWindow() {
        // Initialize slotIndices with default positions
        slotIndices["head"] = 0;
        slotIndices["chest"] = 1;
        slotIndices["legs"] = 2;
        slotIndices["feet"] = 3;
        // ... add other slots as needed
    }

    EquipmentWindow(std::vector<InventoryItem>& items, nb::list& commands)
        : ItemWindow(items, commands) {
        // Initialize slotIndices with default positions
        slotIndices["head"] = 0;
        slotIndices["chest"] = 1;
        slotIndices["legs"] = 2;
        slotIndices["feet"] = 3;
        // ... add other slots as needed
    }

    void Render() override {
        ImGui::Begin("Equipment");

        // Example positions for equipment slots
        ImVec2 headSlotPos = ImVec2(50, 50);
        ImVec2 chestSlotPos = ImVec2(50, 100);
        ImVec2 legsSlotPos = ImVec2(50, 140);
        ImVec2 feetSlotPos = ImVec2(50, 200);
        // ... other positions ...

        // Render head slot
        ImGui::SetCursorPos(headSlotPos);
        RenderItemSlot(*items, slotIndices["head"], selected_index, ImVec2(32, 32), "EquipHead_",
                       "DND_ITEM", "equipment", *commands, nullptr, false);

        // Render chest slot
        ImGui::SetCursorPos(chestSlotPos);
        RenderItemSlot(*items, slotIndices["chest"], selected_index, ImVec2(32, 32), "EquipChest_",
                       "DND_ITEM", "equipment", *commands, nullptr, false);

        // Render other slots...

        ImGui::End();
    }
};

#endif /* defined(__SDL_Game_Programming_Book__DebuggingTools__) */