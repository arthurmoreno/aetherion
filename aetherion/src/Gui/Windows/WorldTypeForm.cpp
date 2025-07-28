#include "Gui/Gui.hpp"

void imguiPrepareWorldTypeFormWindows(nb::list& commands, nb::dict& shared_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Center the world form window on screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(800, 600);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin("Create New World", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();

    // Form title
    ImGui::SetCursorPosX((windowSize.x - ImGui::CalcTextSize("CREATE NEW WORLD").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CREATE NEW WORLD");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Static variables to hold form data
    static char LocalWorldType[128] = "Local World";
    static char WorldServerType[128] = "Connect to World Server";

    // World type selection (0 = Local World, 1 = World Server)
    static int selectedWorldType = 0;

    // Physics settings
    static bool allowMultiDirection = true;

    // Create scrollable child region for form content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("FormScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    ImGui::Spacing();

    // World Type Selection
    ImGui::Text("World Type:");
    ImGui::Spacing();

    // Radio buttons for world type selection
    if (ImGui::RadioButton("Local World", selectedWorldType == 0)) {
        selectedWorldType = 0;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " - Create a local single-player world");

    if (ImGui::RadioButton("Connect to World Server", selectedWorldType == 1)) {
        selectedWorldType = 1;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " - Join a multiplayer server");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Physics Settings:");
    ImGui::Checkbox("Allow Multi Direction", &allowMultiDirection);

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Store form data in shared_data
    shared_data["world_type"] = nb::int_(selectedWorldType);
    shared_data["world_type_name"] =
        nb::str(selectedWorldType == 0 ? LocalWorldType : WorldServerType);
    shared_data["allow_multi_direction"] = nb::bool_(allowMultiDirection);

    // Buttons
    ImVec2 buttonSize = ImVec2(120, 35);
    float totalButtonWidth = buttonSize.x * 2 + 20;  // Two buttons + spacing
    float buttonStartX = (windowSize.x - totalButtonWidth) * 0.5f;

    ImGui::SetCursorPosX(buttonStartX);
    if (ImGui::Button("Create", buttonSize)) {
        // Create command to create the world with form data
        nb::dict command;
        command["type"] = nb::str("select_world_type");
        command["data"] = shared_data;
        commands.append(command);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x + 20);
    if (ImGui::Button("Cancel", buttonSize)) {
        // Create command to cancel world creation
        nb::dict command;
        command["type"] = nb::str("cancel_world_creation");
        commands.append(command);
    }

    ImGui::End();
}