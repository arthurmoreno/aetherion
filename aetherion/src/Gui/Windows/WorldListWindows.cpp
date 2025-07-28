#include "Gui/Gui.hpp"

void imguiPrepareWorldListWindows(nb::list& commands, nb::dict& shared_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Center the world list window on screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(800, 600);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin("World Selection", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();

    // Window title
    ImGui::SetCursorPosX((windowSize.x - ImGui::CalcTextSize("SELECT WORLD").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "SELECT WORLD");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Static variable to track selected world
    static int selectedWorldIndex = -1;
    static std::string selectedWorldKey = "";

    // Create scrollable child region for table content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("WorldTableScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Create the world list table
    if (ImGui::BeginTable(
            "WorldTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        // Set up table headers
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        // Iterate through worlds in shared_data
        int worldIndex = 0;
        for (auto [world_key, world_data] : shared_data) {
            ImGui::TableNextRow();

            // Extract world information
            std::string worldName = "Unknown";
            std::string worldDescription = "";
            std::string worldStatus = "unknown";

            if (nb::isinstance<nb::dict>(world_data)) {
                auto world_dict = nb::cast<nb::dict>(world_data);

                if (world_dict.contains("name")) {
                    worldName = nb::cast<std::string>(world_dict["name"]);
                }
                if (world_dict.contains("description")) {
                    worldDescription = nb::cast<std::string>(world_dict["description"]);
                }
                if (world_dict.contains("status")) {
                    worldStatus = nb::cast<std::string>(world_dict["status"]);
                }
            }

            // Name column
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", worldName.c_str());

            // Description column
            ImGui::TableSetColumnIndex(1);
            if (worldDescription.empty()) {
                ImGui::TextDisabled("No description");
            } else {
                ImGui::Text("%s", worldDescription.c_str());
            }

            // Status column
            ImGui::TableSetColumnIndex(2);
            ImVec4 statusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // Default white
            if (worldStatus == "creating") {
                statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
            } else if (worldStatus == "ready") {
                statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
            } else if (worldStatus == "paused") {
                statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  // Green
            } else if (worldStatus == "error") {
                statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Red
            }
            ImGui::TextColored(statusColor, "%s", worldStatus.c_str());

            // Select column
            ImGui::TableSetColumnIndex(3);
            bool isSelected = (worldIndex == selectedWorldIndex);
            std::string radioButtonId = "##select_" + std::to_string(worldIndex);

            if (ImGui::RadioButton(radioButtonId.c_str(), isSelected)) {
                selectedWorldIndex = worldIndex;
                selectedWorldKey = nb::cast<std::string>(world_key);
            }

            worldIndex++;
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

    // New World button
    ImGui::SetCursorPosX(buttonStartX);
    if (ImGui::Button("New World", buttonSize)) {
        nb::dict command;
        command["type"] = "new_world_requested";
        commands.append(command);
    }

    // Delete button (only enabled if a world is selected)
    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x + 20);

    bool hasSelection = selectedWorldIndex >= 0 && !selectedWorldKey.empty();
    if (!hasSelection) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Delete", buttonSize)) {
        nb::dict command;
        command["type"] = "delete_world_requested";
        command["world_key"] = selectedWorldKey.c_str();
        commands.append(command);

        // Reset selection after delete request
        selectedWorldIndex = -1;
        selectedWorldKey = "";
    }

    if (!hasSelection) {
        ImGui::EndDisabled();
    }

    // Connect button (only enabled if a world is selected and ready)
    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x * 2 + 40);

    bool canConnect = false;
    if (hasSelection && shared_data.contains(selectedWorldKey.c_str())) {
        auto selected_world = nb::cast<nb::dict>(shared_data[selectedWorldKey.c_str()]);
        if (selected_world.contains("status")) {
            std::string status = nb::cast<std::string>(selected_world["status"]);
            canConnect = (status == "ready" || status == "paused");
        }
    }

    auto logger = Logger::getLogger();
    logger->info("Can connect to world '{}': {}", selectedWorldKey, canConnect);

    if (!canConnect) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Connect", buttonSize)) {
        nb::dict command;
        logger->info("Clicking connect button for world '{}'", selectedWorldKey);
        command["type"] = "connect_world_requested";
        command["world_key"] = selectedWorldKey.c_str();
        commands.append(command);
    }

    if (!canConnect) {
        ImGui::EndDisabled();
    }

    // Store selected world in shared_data for other components to access
    if (hasSelection) {
        shared_data["selected_world_key"] = selectedWorldKey.c_str();
    } else {
        if (shared_data.contains("selected_world_key")) {
            nb::del(shared_data["selected_world_key"]);
        }
    }

    ImGui::End();
}