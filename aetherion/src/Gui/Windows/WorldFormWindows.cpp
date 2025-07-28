#include "Gui/Gui.hpp"

void imguiPrepareWorldFormWindows(nb::list& commands, nb::dict& shared_data) {
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
    static char worldName[128] = "New World";
    static char worldDescription[256] = "A fresh world ready for exploration";
    static int worldWidth = 100;
    static int worldHeight = 100;
    static int worldDepth = 10;
    static int seed = 12345;
    static bool generateTerrain = true;
    static bool generateWater = true;
    static bool generateVegetation = false;
    static int difficultyLevel = 1;
    static float resourceDensity = 0.5f;

    // Physics settings
    static float gravity = 5.0f;
    static float friction = 1.0f;
    static bool allowMultiDirection = true;
    static float evaporationCoefficient = 8.0f;
    static float heatToWaterEvaporation = 120.0f;
    static int waterMinimumUnits = 120000;
    static float metabolismCostToApplyForce = 0.000002f;

    // Create scrollable child region for form content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("FormScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Form fields
    ImGui::Text("World Name:");
    ImGui::InputText("##WorldName", worldName, sizeof(worldName));

    ImGui::Spacing();

    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##WorldDescription", worldDescription, sizeof(worldDescription),
                              ImVec2(0, 60));

    ImGui::Spacing();

    ImGui::Text("World Dimensions:");
    ImGui::SliderInt("Width", &worldWidth, 1, 500);
    ImGui::SliderInt("Height", &worldHeight, 1, 500);
    ImGui::SliderInt("Depth", &worldDepth, 1, 100);

    ImGui::Spacing();

    ImGui::Text("Generation Settings:");
    ImGui::InputInt("Seed", &seed);
    ImGui::Checkbox("Generate Terrain", &generateTerrain);
    ImGui::Checkbox("Generate Water Bodies", &generateWater);
    ImGui::Checkbox("Generate Vegetation", &generateVegetation);

    ImGui::Spacing();

    ImGui::Text("Game Settings:");
    ImGui::SliderInt("Difficulty Level", &difficultyLevel, 1, 5);
    ImGui::SliderFloat("Resource Density", &resourceDensity, 0.1f, 2.0f, "%.2f");

    ImGui::Spacing();

    ImGui::Text("Physics Settings:");
    ImGui::SliderFloat("Gravity", &gravity, 0.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Friction", &friction, 0.0f, 10.0f, "%.2f");
    ImGui::Checkbox("Allow Multi Direction", &allowMultiDirection);

    ImGui::Spacing();
    ImGui::Text("Environmental Physics:");
    ImGui::SliderFloat("Evaporation Coefficient", &evaporationCoefficient, 1.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Heat to Water Evaporation", &heatToWaterEvaporation, 50.0f, 300.0f, "%.2f");
    ImGui::SliderInt("Water Minimum Units", &waterMinimumUnits, 10000, 500000);

    ImGui::Spacing();
    ImGui::Text("Metabolism Settings:");
    ImGui::SliderFloat("Metabolism Cost to Apply Force", &metabolismCostToApplyForce, 0.0000001f,
                       0.00001f, "%.8f");

    ImGui::Spacing();

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Store form data in shared_data
    shared_data["world_name"] = nb::str(worldName);
    shared_data["world_description"] = nb::str(worldDescription);
    shared_data["world_width"] = nb::int_(worldWidth);
    shared_data["world_height"] = nb::int_(worldHeight);
    shared_data["world_depth"] = nb::int_(worldDepth);
    shared_data["seed"] = nb::int_(seed);
    shared_data["generate_terrain"] = nb::bool_(generateTerrain);
    shared_data["generate_water"] = nb::bool_(generateWater);
    shared_data["generate_vegetation"] = nb::bool_(generateVegetation);
    shared_data["difficulty_level"] = nb::int_(difficultyLevel);
    shared_data["resource_density"] = nb::float_(resourceDensity);

    // Store physics settings in shared_data
    shared_data["gravity"] = nb::float_(gravity);
    shared_data["friction"] = nb::float_(friction);
    shared_data["allow_multi_direction"] = nb::bool_(allowMultiDirection);
    shared_data["evaporation_coefficient"] = nb::float_(evaporationCoefficient);
    shared_data["heat_to_water_evaporation"] = nb::float_(heatToWaterEvaporation);
    shared_data["water_minimum_units"] = nb::int_(waterMinimumUnits);
    shared_data["metabolism_cost_to_apply_force"] = nb::float_(metabolismCostToApplyForce);

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