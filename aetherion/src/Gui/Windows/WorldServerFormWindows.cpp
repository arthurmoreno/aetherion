#include "Gui/Gui.hpp"

void imguiPrepareServerWorldFormWindows(nb::list& commands, nb::dict& shared_data) {
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

    ImGui::Begin("Connect to World Server", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();

    // Form title
    ImGui::SetCursorPosX((windowSize.x - ImGui::CalcTextSize("CONNECT TO WORLD SERVER").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CONNECT TO WORLD SERVER");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Static variables to hold form data
    static char worldHost[128] = "localhost";
    static char worldPort[128] = "8765";
    static char username[128] = "admin";
    static char password[128] = "password";

    // Create scrollable child region for form content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("FormScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Form fields
    ImGui::Text("World Host:");
    ImGui::InputText("##WorldHost", worldHost, sizeof(worldHost));

    ImGui::Spacing();

    ImGui::Text("World Port:");
    ImGui::InputText("##WorldPort", worldPort, sizeof(worldPort));

    ImGui::Spacing();

    ImGui::Text("Username:");
    ImGui::InputText("##Username", username, sizeof(username));

    ImGui::Spacing();

    ImGui::Text("Password:");
    ImGui::InputText("##Password", password, sizeof(password));

    ImGui::Spacing();
    ImGui::Text(
        "Note: The world server must be running and accessible at the specified host and port.");
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Store form data in shared_data
    shared_data["world_host"] = nb::str(worldHost);
    shared_data["world_port"] = nb::str(worldPort);
    shared_data["username"] = nb::str(username);
    shared_data["password"] = nb::str(password);

    // Buttons
    ImVec2 buttonSize = ImVec2(120, 35);
    float totalButtonWidth = buttonSize.x * 2 + 20;  // Two buttons + spacing
    float buttonStartX = (windowSize.x - totalButtonWidth) * 0.5f;

    ImGui::SetCursorPosX(buttonStartX);
    if (ImGui::Button("Create", buttonSize)) {
        // Create command to create the world with form data
        nb::dict command;
        command["type"] = nb::str("create_world");
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