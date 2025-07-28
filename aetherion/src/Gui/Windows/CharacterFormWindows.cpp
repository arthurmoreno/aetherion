#include "Gui/Gui.hpp"

void imguiPrepareCharacterFormWindows(nb::list& commands, nb::dict& shared_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Center the character form window on screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(650, 550);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin("Create New Character", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();

    // Form title
    ImGui::SetCursorPosX((windowSize.x - ImGui::CalcTextSize("CREATE NEW CHARACTER").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "CREATE NEW CHARACTER");

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Static variables to hold form data
    static char characterName[128] = "Hero";
    static char characterDescription[256] = "A brave adventurer ready to explore the world";
    static int characterClass = 0;  // 0=Warrior, 1=Mage, 2=Archer, 3=Rogue
    static int strength = 10;
    static int intelligence = 10;
    static int dexterity = 10;
    static int constitution = 10;
    static int startingLevel = 1;
    static float experienceMultiplier = 1.0f;
    static bool enablePvP = false;
    static bool enableMagic = true;
    static bool enableCrafting = true;

    const char* characterClasses[] = {"Warrior", "Mage", "Archer", "Rogue"};

    // Create scrollable child region for form content, leaving space for buttons
    float buttonAreaHeight = 60.0f;  // Height for buttons and spacing
    ImGui::BeginChild("CharacterFormScrollRegion", ImVec2(0, -buttonAreaHeight), false,
                      ImGuiWindowFlags_None);

    // Form fields
    ImGui::Text("Character Name:");
    ImGui::InputText("##CharacterName", characterName, sizeof(characterName));

    ImGui::Spacing();

    ImGui::Text("Description:");
    ImGui::InputTextMultiline("##CharacterDescription", characterDescription,
                              sizeof(characterDescription), ImVec2(0, 60));

    ImGui::Spacing();

    ImGui::Text("Character Class:");
    ImGui::Combo("##CharacterClass", &characterClass, characterClasses,
                 IM_ARRAYSIZE(characterClasses));

    ImGui::Spacing();

    ImGui::Text("Attributes:");
    ImGui::SliderInt("Strength", &strength, 1, 20);
    ImGui::SliderInt("Intelligence", &intelligence, 1, 20);
    ImGui::SliderInt("Dexterity", &dexterity, 1, 20);
    ImGui::SliderInt("Constitution", &constitution, 1, 20);

    ImGui::Spacing();

    ImGui::Text("Character Settings:");
    ImGui::SliderInt("Starting Level", &startingLevel, 1, 10);
    ImGui::SliderFloat("Experience Multiplier", &experienceMultiplier, 0.5f, 3.0f, "%.2f");

    ImGui::Spacing();

    ImGui::Text("Game Features:");
    ImGui::Checkbox("Enable PvP", &enablePvP);
    ImGui::Checkbox("Enable Magic", &enableMagic);
    ImGui::Checkbox("Enable Crafting", &enableCrafting);

    ImGui::Spacing();

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Store form data in shared_data
    shared_data["character_name"] = characterName;
    shared_data["character_description"] = characterDescription;
    shared_data["character_class"] = characterClasses[characterClass];
    shared_data["strength"] = nb::int_(strength);
    shared_data["intelligence"] = nb::int_(intelligence);
    shared_data["dexterity"] = nb::int_(dexterity);
    shared_data["constitution"] = nb::int_(constitution);
    shared_data["starting_level"] = nb::int_(startingLevel);
    shared_data["experience_multiplier"] = nb::float_(experienceMultiplier);
    shared_data["enable_pvp"] = nb::bool_(enablePvP);
    shared_data["enable_magic"] = nb::bool_(enableMagic);
    shared_data["enable_crafting"] = nb::bool_(enableCrafting);

    // Buttons
    ImVec2 buttonSize = ImVec2(120, 35);
    float totalButtonWidth = buttonSize.x * 2 + 20;  // Two buttons + spacing
    float buttonStartX = (windowSize.x - totalButtonWidth) * 0.5f;

    ImGui::SetCursorPosX(buttonStartX);
    if (ImGui::Button("Create", buttonSize)) {
        // Create command to create the character with form data
        nb::dict command;
        command["type"] = "create_character";
        command["data"] = shared_data;
        commands.append(command);
    }

    ImGui::SameLine();
    ImGui::SetCursorPosX(buttonStartX + buttonSize.x + 20);
    if (ImGui::Button("Cancel", buttonSize)) {
        // Create command to cancel character creation
        nb::dict command;
        command["type"] = "cancel_character_creation";
        commands.append(command);
    }

    ImGui::End();
}