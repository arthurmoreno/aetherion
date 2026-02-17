#include "Gui/Gui.hpp"
#include "components/core/Command.hpp"

void imguiPrepareCharacterListWindows(nb::list& commands, nb::dict& shared_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Center the character list window on screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(850, 650);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin("Character Selection", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();

    // Window title
    ImGui::SetCursorPosX((windowSize.x - ImGui::CalcTextSize("SELECT CHARACTER").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "SELECT CHARACTER");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Static variable to track selected character
    static int selectedCharacterIndex = -1;
    static std::string selectedCharacterKey = "";

    // Create scrollable child region for table content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("CharacterTableScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Create the character list table
    if (ImGui::BeginTable(
            "CharacterTable", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        // Set up table headers
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        // Iterate through characters in shared_data
        int characterIndex = 0;
        for (auto [character_key, character_data] : shared_data) {
            ImGui::TableNextRow();

            // Extract character information
            std::string characterName = "Unknown";
            std::string characterClass = "Warrior";
            int characterLevel = 1;
            std::string characterStatus = "unknown";

            if (nb::isinstance<nb::dict>(character_data)) {
                auto character_dict = nb::cast<nb::dict>(character_data);

                if (character_dict.contains("name")) {
                    characterName = nb::cast<std::string>(character_dict["name"]);
                }
                if (character_dict.contains("class")) {
                    characterClass = nb::cast<std::string>(character_dict["class"]);
                }
                if (character_dict.contains("level")) {
                    characterLevel = nb::cast<int>(character_dict["level"]);
                }
                if (character_dict.contains("status")) {
                    characterStatus = nb::cast<std::string>(character_dict["status"]);
                }
            }

            // Name column
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", characterName.c_str());

            // Class column
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", characterClass.c_str());

            // Level column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", characterLevel);

            // Status column
            ImGui::TableSetColumnIndex(3);
            ImVec4 statusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // Default white
            if (characterStatus == "creating") {
                statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
            } else if (characterStatus == "ready") {
                statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
            } else if (characterStatus == "in_game") {
                statusColor = ImVec4(0.0f, 0.8f, 1.0f, 1.0f);  // Cyan
            } else if (characterStatus == "error") {
                statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
            }
            ImGui::TextColored(statusColor, "%s", characterStatus.c_str());

            // Select column
            ImGui::TableSetColumnIndex(4);
            bool isSelected = (characterIndex == selectedCharacterIndex);
            std::string radioButtonId = "##select_" + std::to_string(characterIndex);

            if (ImGui::RadioButton(radioButtonId.c_str(), isSelected)) {
                selectedCharacterIndex = characterIndex;
                selectedCharacterKey = nb::cast<std::string>(character_key);
            }

            characterIndex++;
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Action buttons
    ImVec2 buttonSize = ImVec2(120, 35);
    float totalButtonWidth = buttonSize.x * 3 + 40;  // Three buttons + spacing
    float buttonStartX = (windowSize.x - totalButtonWidth) * 0.5f;

    // New Character button
    ImGui::SetCursorPosX(buttonStartX);
    if (ImGui::Button("New Character", buttonSize)) {
        Command command("new_character_requested");
        commands.append(command);
    }

    // Delete button (only enabled if a character is selected)
    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x + 20);

    bool hasSelection = selectedCharacterIndex >= 0 && !selectedCharacterKey.empty();
    if (!hasSelection) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Delete", buttonSize)) {
        Command command("delete_character_requested");
        command.setParam("world_key", selectedCharacterKey);
        commands.append(command);

        // Reset selection after delete request
        selectedCharacterIndex = -1;
        selectedCharacterKey = "";
    }

    if (!hasSelection) {
        ImGui::EndDisabled();
    }

    // Play button (only enabled if a character is selected and ready)
    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x * 2 + 40);

    bool canPlay = false;
    if (hasSelection && shared_data.contains(selectedCharacterKey.c_str())) {
        auto selected_character = nb::cast<nb::dict>(shared_data[selectedCharacterKey.c_str()]);
        if (selected_character.contains("status")) {
            std::string status = nb::cast<std::string>(selected_character["status"]);
            canPlay = (status == "ready");
        }
    }

    if (!canPlay) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Play", buttonSize)) {
        Command command("play_character_requested");
        command.setParam("character_key", selectedCharacterKey);
        commands.append(command);
    }

    if (!canPlay) {
        ImGui::EndDisabled();
    }

    // Store selected character in shared_data for other components to access
    if (hasSelection) {
        shared_data["selected_character_key"] = selectedCharacterKey.c_str();
    } else {
        if (shared_data.contains("selected_character_key")) {
            nb::del(shared_data["selected_character_key"]);
        }
    }

    ImGui::End();
}