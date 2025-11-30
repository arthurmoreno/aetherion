#include "Gui.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include <algorithm>
#include <iostream>
#include <map>  // for casting statistics dict to std::map

#include "EntityInterface.hpp"
#include "GuiStateManager.hpp"
#include "LowLevelRenderer/TextureManager.hpp"
#include "PhysicsManager.hpp"
#include "AllGuiPrograms.hpp"  // Include all GUI programs

namespace nb = nanobind;

constexpr float NormalizeColor(int value) { return static_cast<float>(value) / 255.0f; }

ImVec4 NormalizeColor(int r, int g, int b, float a = 1.0f) {
    return ImVec4(NormalizeColor(r), NormalizeColor(g), NormalizeColor(b), a);
}

// Function to apply a custom style
void ApplyCustomStyle() {
    // Start with the Dark style as the base
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    // Define palette colors (normalized RGB)
    ImVec4 color_DarkBlue = NormalizeColor(30, 49, 75);
    ImVec4 color_MidBlue = NormalizeColor(47, 76, 108);
    ImVec4 color_BlueCyan = NormalizeColor(61, 128, 163);
    ImVec4 color_BlueCyanDimmed = NormalizeColor(61 * 0.7f, 128 * 0.7f, 163 * 0.7f);
    ImVec4 color_Cyan = NormalizeColor(99, 196, 204);
    ImVec4 color_LightCyan = NormalizeColor(154, 229, 213);

    // Assign colors to ImGui style
    style.Colors[ImGuiCol_WindowBg] = color_DarkBlue;
    style.Colors[ImGuiCol_Header] = color_MidBlue;
    style.Colors[ImGuiCol_HeaderHovered] = color_BlueCyan;
    style.Colors[ImGuiCol_HeaderActive] = color_DarkBlue;
    style.Colors[ImGuiCol_Button] = color_MidBlue;
    style.Colors[ImGuiCol_ButtonHovered] = color_BlueCyan;
    style.Colors[ImGuiCol_ButtonActive] = color_DarkBlue;

    // Update FrameBg colors to make checkboxes visible
    style.Colors[ImGuiCol_FrameBg] = color_MidBlue;          // Checkbox background
    style.Colors[ImGuiCol_FrameBgHovered] = color_Cyan;      // When hovered
    style.Colors[ImGuiCol_FrameBgActive] = color_LightCyan;  // When active

    style.Colors[ImGuiCol_TitleBg] = color_DarkBlue;
    style.Colors[ImGuiCol_TitleBgActive] = color_MidBlue;
    style.Colors[ImGuiCol_TitleBgCollapsed] = color_DarkBlue;
    style.Colors[ImGuiCol_Text] = color_LightCyan;
    style.Colors[ImGuiCol_TextDisabled] = color_BlueCyanDimmed;
    style.Colors[ImGuiCol_ScrollbarBg] = color_DarkBlue;
    style.Colors[ImGuiCol_ScrollbarGrab] = color_MidBlue;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = color_BlueCyan;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = color_DarkBlue;
    style.Colors[ImGuiCol_CheckMark] = color_Cyan;
    style.Colors[ImGuiCol_SliderGrab] = color_BlueCyan;
    style.Colors[ImGuiCol_SliderGrabActive] = color_Cyan;

    // Customize other colors as needed using the defined palette
    style.Colors[ImGuiCol_Tab] = color_MidBlue;
    style.Colors[ImGuiCol_TabHovered] = color_BlueCyan;
    style.Colors[ImGuiCol_TabActive] = color_Cyan;

    // Optionally, adjust other style variables
    style.WindowPadding = ImVec2(15.0f, 15.0f);
    style.FramePadding = ImVec2(5.0f, 5.0f);
    style.ItemSpacing = ImVec2(12.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing = 25.0f;
    style.ScrollbarSize = 15.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabMinSize = 5.0f;
    style.GrabRounding = 3.0f;

    // Set rounding for various elements to achieve rounded corners
    style.WindowRounding = 10.0f;  // Rounding radius for window corners
    style.FrameRounding = 5.0f;    // Rounding radius for frame corners (buttons, input boxes, etc.)
    style.ChildRounding = 5.0f;    // Rounding for child windows
    style.PopupRounding = 5.0f;    // Rounding for popup windows
    style.ScrollbarRounding = 5.0f;  // Rounding for scrollbar grab
    style.GrabRounding = 5.0f;       // Rounding for slider and scrollbar grab
    style.TabRounding = 5.0f;        // Rounding for tabs

    const char* fontPath = "resources/Toriko.ttf";
    float fontSize = 18.0f;

    // Load Custom Font
    ImGuiIO& io = ImGui::GetIO();

    // Add the custom font
    ImFont* customFont = io.Fonts->AddFontFromFileTTF(fontPath, fontSize);
    if (customFont == nullptr) {
        fprintf(stderr, "Failed to load font: %s\n", fontPath);
    } else {
        io.FontDefault = customFont;
    }
}

// Function to check if ImGui wants to capture keyboard
bool wants_capture_keyboard() {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}

// Function to check if ImGui wants to capture mouse
bool wants_capture_mouse() {
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

// Initialize ImGui with SDL_Renderer
void imguiInit(uintptr_t window_ptr, uintptr_t renderer_ptr) {
    SDL_Window* window = reinterpret_cast<SDL_Window*>(window_ptr);
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // Configure ImGui IO
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    // #ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    // #endif

    io.WantCaptureMouse = false;
    io.WantCaptureKeyboard = false;

    // Initialize ImGui SDL2 and SDL_Renderer bindings
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);  // Corrected function

    // Optionally, set ImGui style
    // ImGui::StyleColorsDark();
    GuiStateManager::Instance();
    ApplyCustomStyle();
    
    // Initialize GUI programs (GUI OS)
    initializeGuiPrograms();
}

/**
 * @brief Initialize and register all GUI programs
 * 
 * Called during ImGui initialization to register all available GUI programs
 * with the GuiProgramManager. Programs can then be activated/deactivated
 * dynamically during runtime.
 */
void initializeGuiPrograms() {
    auto* manager = GuiProgramManager::Instance();
    
    // Register Settings programs
    manager->registerProgram(std::make_shared<SettingsProgram>());
    manager->registerProgram(std::make_shared<PhysicsSettingsProgram>());
    manager->registerProgram(std::make_shared<CameraSettingsProgram>());
    
    // Register Debug programs
    manager->registerProgram(std::make_shared<GeneralMetricsProgram>());
    manager->registerProgram(std::make_shared<EntitiesStatsProgram>());
    manager->registerProgram(std::make_shared<GadgetsProgram>());
    manager->registerProgram(std::make_shared<EntityInterfaceProgram>());
    manager->registerProgram(std::make_shared<AIStatisticsProgram>());
    manager->registerProgram(std::make_shared<ConsoleProgram>());
    
    // Register Game programs
    manager->registerProgram(std::make_shared<InventoryProgram>());
    manager->registerProgram(std::make_shared<EquipmentProgram>());
    manager->registerProgram(std::make_shared<PlayerStatsProgram>());
}

// Function to process SDL_Event using ImGui's SDL2 backend
void imguiProcessEvent(nb::bytes event_bytes) {
    // Ensure the received bytes are at least the size of SDL_Event
    std::string bytes(event_bytes.c_str(), event_bytes.size());

    if (bytes.size() < sizeof(SDL_Event)) {
        throw std::runtime_error("Received event data is too short.");
    }

    // Reinterpret the bytes as SDL_Event
    SDL_Event* event = reinterpret_cast<SDL_Event*>(const_cast<char*>(bytes.data()));

    // Process the event with Dear ImGui's SDL2 backend
    ImGui_ImplSDL2_ProcessEvent(event);
}

void RenderPhysicsSettingsWindow(nb::dict physicsChanges) {
    // Begin Custom Window
    // ImGui::Begin("Physics Settings");

    // Access the singleton instance
    PhysicsManager* physics = PhysicsManager::Instance();

    // Display and edit Gravity
    static float gravity = physics->getGravity();
    static float friction = physics->getFriction();
    static bool allowMultiDirection = physics->getAllowMultiDirection();
    if (ImGui::InputFloat("Gravity (m/s²)", &gravity)) {
        physics->setGravity(gravity);
    }
    // Display and edit Friction
    if (ImGui::InputFloat("Friction Coefficient", &friction)) {
        physics->setFriction(friction);
    }

    // Optional: Add sliders for more intuitive control
    if (ImGui::SliderFloat("Gravity (m/s²) slide", &gravity, 0.0f, 20.0f)) {
        physics->setGravity(gravity);
    }
    if (ImGui::SliderFloat("Friction Coefficient slide", &friction, 0.0f, 10.0f)) {
        physics->setFriction(friction);
    }

    // Display and edit Friction
    if (ImGui::Checkbox("Allow Multidirection", &allowMultiDirection)) {
        physics->setAllowMultiDirection(allowMultiDirection);
    }

    // Optional: Button to reset to default values
    if (ImGui::Button("Reset to Defaults")) {
        physics->setGravity(5.0f);
        physics->setFriction(1.0f);
        physics->setAllowMultiDirection(true);
        gravity = physics->getGravity();
        friction = physics->getFriction();
        allowMultiDirection = physics->getAllowMultiDirection();
    }

    physicsChanges["gravity"] = gravity;
    physicsChanges["friction"] = friction;
    physicsChanges["allowMultiDirection"] = allowMultiDirection;
    // ImGui::End();
}

void RenderGeneralMetricsWindow(int worldTicks, float availableFps) {
    // ImGui::Begin("General metrics");
    ImGui::Text("Available FPS (fixed 30 FPS): %.2f", availableFps);
    ImGui::Text("World Ticks: %d", worldTicks);
    // ImGui::End();
}

void RenderPlayerStatsWindow(std::shared_ptr<World> world_ptr) {
    if (!world_ptr) {
        return;
    }

    // ImGui::Begin("Players Stats");
    auto view = world_ptr->registry.view<EntityTypeComponent>();
    for (auto entity : view) {
        // Retrieve the entity type component
        EntityTypeComponent* entityTypeComp =
            world_ptr->registry.try_get<EntityTypeComponent>(entity);

        // If the entity's type matches the input type
        if (entityTypeComp && entityTypeComp->mainType == 2 && entityTypeComp->subType0 == 1) {
            // Retrieve additional entity components, such as position or other
            // metadata
            EntityInterface entityInterface = createEntityInterface(world_ptr->registry, entity);

            // Store the entity interface in the dictionary with the entity ID as the
            // key

            auto pos = entityInterface.getComponent<Position>();
            ImGui::Text("position x: %d", pos.x);
            ImGui::Text("position y: %d", pos.y);
            ImGui::Text("position z: %d", pos.z);

            ImGui::NewLine();

            auto velocity = entityInterface.getComponent<Velocity>();
            ImGui::Text("velocity vx: %.4f", velocity.vx);
            ImGui::Text("velocity vy: %.4f", velocity.vy);
            ImGui::Text("velocity vz: %.4f", velocity.vz);

            if (auto* physicsStats = world_ptr->registry.try_get<PhysicsStats>(entity)) {
                ImGui::NewLine();

                static float mass = physicsStats->mass;
                if (ImGui::InputFloat("Mass", &mass)) {
                    physicsStats->mass = mass;
                }

                // ImGui::Text("maxSpeed: %.4f", physicsStats->maxSpeed);
                static float maxSpeed = physicsStats->maxSpeed;
                if (ImGui::InputFloat("Max Speed", &maxSpeed)) {
                    physicsStats->maxSpeed = maxSpeed;
                }
                // ImGui::Text("minSpeed: %.4f", physicsStats->minSpeed);
                static float minSpeed = physicsStats->minSpeed;
                if (ImGui::InputFloat("Min Speed", &minSpeed)) {
                    physicsStats->minSpeed = minSpeed;
                }

                // ImGui::Text("forceX: %.4f", physicsStats->forceX);
                static float forceX = physicsStats->forceX;
                if (ImGui::InputFloat("Force X", &forceX)) {
                    physicsStats->forceX = forceX;
                }
                // ImGui::Text("forceY: %.4f", physicsStats->forceY);
                static float forceY = physicsStats->forceY;
                if (ImGui::InputFloat("Force Y", &forceY)) {
                    physicsStats->forceY = forceY;
                }
                // ImGui::Text("forceZ: %.4f", physicsStats->forceZ);
                static float forceZ = physicsStats->forceZ;
                if (ImGui::InputFloat("force Z", &forceZ)) {
                    physicsStats->forceZ = forceZ;
                }
            }
        }
    }
    // ImGui::End();
}

// Function to render the Camera Settings window
void RenderCameraSettingsWindow(int& CAMERA_SCREEN_WIDTH_ADJUST_OFFSET,
                                int& CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET) {
    // Begin the ImGui window
    // ImGui::Begin("Camera Settings");

    // Display current camera offsets
    ImGui::Text("Width Adjust Offset: %d", CAMERA_SCREEN_WIDTH_ADJUST_OFFSET);
    ImGui::Text("Height Adjust Offset: %d", CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET);

    // Slider controls for Width and Height Adjust Offsets
    ImGui::SliderInt("Width Adjust Offset", &CAMERA_SCREEN_WIDTH_ADJUST_OFFSET, -1000, 1000);
    ImGui::SliderInt("Height Adjust Offset", &CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET, -1000, 1000);
    ImGui::Separator();

    // 2D Position Plane
    // Define the size of the position plane
    const ImVec2 plane_size = ImVec2(200, 200);
    ImGui::Text("Adjust Position:");

    // Draw a child window to contain the position plane
    ImGui::BeginChild("PositionPlane", plane_size, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Get the draw list for custom drawing
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Get the absolute position of the child window
    ImVec2 p0 = ImGui::GetWindowPos();
    ImVec2 p1 = ImVec2(p0.x + plane_size.x, p0.y + plane_size.y);

    // Draw the plane background
    draw_list->AddRectFilled(p0, p1, IM_COL32(50, 50, 50, 255));

    // Draw grid lines (optional)
    int grid_size = 10;
    for (int i = 1; i < grid_size; ++i) {
        float x = p0.x + (plane_size.x / grid_size) * i;
        float y = p0.y + (plane_size.y / grid_size) * i;
        draw_list->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(100, 100, 100, 255));
        draw_list->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(100, 100, 100, 255));
    }

    // Create an InvisibleButton to capture mouse interactions within the plane
    ImGui::InvisibleButton("PositionPlaneButton", plane_size);
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();

    // Get the mouse position relative to the plane
    ImVec2 mouse_pos_in_plane = ImGui::GetIO().MousePos;
    mouse_pos_in_plane.x -= ImGui::GetWindowPos().x;
    mouse_pos_in_plane.y -= ImGui::GetWindowPos().y;

    // Define the adjustable range for offsets
    const float OFFSET_MIN = -600.0f;
    const float OFFSET_MAX = 600.0f;

    // Handle mouse dragging within the plane
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        // Clamp mouse position within the plane
        mouse_pos_in_plane.x = std::clamp(mouse_pos_in_plane.x, 0.0f, plane_size.x);
        mouse_pos_in_plane.y = std::clamp(mouse_pos_in_plane.y, 0.0f, plane_size.y);

        // Map mouse position to offset range without inverting Y-axis
        CAMERA_SCREEN_WIDTH_ADJUST_OFFSET =
            (mouse_pos_in_plane.x / plane_size.x) * (OFFSET_MAX - OFFSET_MIN) + OFFSET_MIN;

        // **Removed Y-axis inversion to align with screen coordinates (Y increases downwards)**
        CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET =
            (mouse_pos_in_plane.y / plane_size.y) * (OFFSET_MAX - OFFSET_MIN) + OFFSET_MIN;
    }

    // Calculate normalized positions for the offsets
    float normalized_x =
        (CAMERA_SCREEN_WIDTH_ADJUST_OFFSET - OFFSET_MIN) / (OFFSET_MAX - OFFSET_MIN);
    float normalized_y =
        (CAMERA_SCREEN_HEIGHT_ADJUST_OFFSET - OFFSET_MIN) / (OFFSET_MAX - OFFSET_MIN);

    // **Adjust the Y position without inversion**
    ImVec2 camera_pos_screen =
        ImVec2(p0.x + normalized_x * plane_size.x, p0.y + normalized_y * plane_size.y);

    // Draw the camera position as a circle
    draw_list->AddCircleFilled(camera_pos_screen, 5.0f, IM_COL32(255, 0, 0, 255));

    ImGui::EndChild();

    // ImGui::End();
}

// Function to load inventory from a Python dictionary
std::vector<InventoryItem> LoadInventory(nb::dict inventoryData) {
    std::vector<InventoryItem> inventory;

    // Iterate over each key-value pair in the Python dictionary
    for (auto item : inventoryData) {
        // Ensure that the value is a dictionary
        if (!nb::isinstance<nb::dict>(item.second)) {
            std::cerr << "Warning: Skipping non-dictionary entry in inventory data." << std::endl;
            continue;
        }

        nb::dict itemDict = nb::cast<nb::dict>(item.second);

        std::string name = "Unknown";
        std::string textureId = "default_texture";
        SDL_Texture* texture = nullptr;
        int quantity = 1;

        // Extract "name"
        if (itemDict.contains("name")) {
            try {
                name = nb::cast<std::string>(itemDict["name"]);
            } catch (const nb::cast_error& e) {
                std::cerr << "Error: 'name' field must be a string. Skipping item." << std::endl;
                continue;
            }
        } else {
            std::cerr << "Warning: 'name' field missing. Assigning 'Unknown'." << std::endl;
        }

        // Extract "texture_id"
        if (itemDict.contains("texture_id")) {
            try {
                textureId = nb::cast<std::string>(itemDict["texture_id"]);
            } catch (const nb::cast_error& e) {
                // std::cerr
                //     << "Error: 'texture_id' field must be a string. Assigning 'default_texture'."
                //     << std::endl;
                textureId = "default_texture";
            }
        } else {
            std::cerr << "Warning: 'texture_id' field missing. Assigning 'default_texture'."
                      << std::endl;
        }

        // Extract "quantity"
        if (itemDict.contains("quantity")) {
            try {
                quantity = nb::cast<int>(itemDict["quantity"]);
            } catch (const nb::cast_error& e) {
                std::cerr << "Error: 'quantity' field must be an integer. Assigning 1."
                          << std::endl;
                quantity = 1;
            }
        } else {
            std::cerr << "Warning: 'quantity' field missing. Assigning 1." << std::endl;
        }

        // Retrieve the texture using textureId
        texture = getTextureFromManager(textureId);
        if (!texture) {
            std::cerr << "Warning: Texture ID '" << textureId
                      << "' not found. Assigning 'default_texture'." << std::endl;
            textureId = "default_texture";
            texture = getTextureFromManager(textureId);
            if (!texture) {
                std::cerr << "Error: Default texture not found. Skipping item." << std::endl;
                continue;  // Skip this item if default texture is also not found
            }
        }

        // Add the populated InventoryItem to the inventory vector
        inventory.emplace_back(name, textureId, texture, quantity);
    }

    return inventory;
}

void HandleDragDropToWorld(nb::list& commands) {
    if (GuiStateManager::Instance()->isDraggingFromUI) {
        if (ImGui::IsMouseReleased(0)) {
            // Check if the mouse is not over any ImGui window
            if (!ImGui::GetIO().WantCaptureMouse) {
                // Get the mouse position
                ImVec2 mousePosScreen = ImGui::GetIO().MousePos;

                // Generate a command to drop the item into the game world
                nb::dict command;
                command["type"] = "drop_to_world";
                command["item_index"] = GuiStateManager::Instance()->draggedItemIndex;
                command["src_window"] = GuiStateManager::Instance()->src_window_id;
                command["world_position"] = nb::make_tuple(mousePosScreen.x, mousePosScreen.y);
                commands.append(command);

                std::cout << "Dropped item " << GuiStateManager::Instance()->draggedItemIndex
                          << " from " << GuiStateManager::Instance()->src_window_id
                          << " into the game world at position (" << mousePosScreen.x << ", "
                          << mousePosScreen.y << ")" << std::endl;

                // Reset dragging state
                GuiStateManager::Instance()->isDraggingFromUI = false;
                GuiStateManager::Instance()->draggedItemIndex = -1;
                GuiStateManager::Instance()->src_window_id.clear();
            } else {
                // Mouse released over ImGui window, reset dragging state
                GuiStateManager::Instance()->isDraggingFromUI = false;
                GuiStateManager::Instance()->draggedItemIndex = -1;
                GuiStateManager::Instance()->src_window_id.clear();
            }
        }
    }
}

const int HOTBAR_SIZE = 10;  // Number of items in the hotbar

// Define window state flags
bool showGadgets = false;
bool showEntitiesStats = false;
bool showInventory = false;
bool showEquipment = false;
bool showSettings = false;
bool showCameraSettings = false;
bool showPhysicsSettings = false;
bool showGeneralMetrics = false;
bool showPlayerStats = false;
bool showEntityInterface = false;
bool showAIStatistics = false;
bool showTextEditor = false;

void RenderTopBar() {
    // Get the window size
    ImVec2 windowPos = ImVec2(0, 0);
    ImVec2 windowSize = ImGui::GetIO().DisplaySize;

    // Set the position and size of the top bar
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(ImVec2(windowSize.x, 50));  // Height of 50 pixels

    // Begin a window with no decorations and no scrollbars
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |  // Disable scrollbars
        ImGuiWindowFlags_NoScrollWithMouse |   // Disable scroll with mouse
        ImGuiWindowFlags_NoFocusOnAppearing |  // Prevent window from taking focus on appear
        ImGuiWindowFlags_NoNav;

    ImGui::Begin("TopBar", nullptr, window_flags);

    // Push right alignment by setting cursor position
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 820);  // Adjusted for Console button

    // Console Button
    if (ImGui::Button("Console")) {
        GuiProgramManager::Instance()->toggleProgram("console");
    }

    ImGui::SameLine();

    // Text Editor Button
    if (ImGui::Button("Text Editor")) {
        showTextEditor = true;
    }

    ImGui::SameLine();

    // Gadgets Button
    if (ImGui::Button("Gadgets")) {
        GuiProgramManager::Instance()->toggleProgram("gadgets");
    }

    ImGui::SameLine();

    // AI Statistics Button
    if (ImGui::Button("AI Statistics")) {
        GuiProgramManager::Instance()->toggleProgram("ai_statistics");
    }

    ImGui::SameLine();

    // Entities Stats Button
    if (ImGui::Button("Entities Stats")) {
        GuiProgramManager::Instance()->toggleProgram("entities_stats");
    }

    ImGui::SameLine();

    // Inventory Button
    if (ImGui::Button("Inventory")) {
        GuiProgramManager::Instance()->toggleProgram("inventory");
    }

    ImGui::SameLine();

    // Equipment Button
    if (ImGui::Button("Equipment")) {
        GuiProgramManager::Instance()->toggleProgram("equipment");
    }

    ImGui::SameLine();

    // Settings Button
    if (ImGui::Button("Settings")) {
        GuiProgramManager::Instance()->toggleProgram("settings");
    }

    ImGui::End();
}

void RenderEditorDebuggerTopBar(nb::list& commands) {
    // Set initial position only on first use, then allow user to move it
    ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_FirstUseEver);

    // Allow the window to auto-resize to fit content
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize;

    // Create a collapsible, draggable window
    static bool show_editor_debugger_topbar = true;
    if (ImGui::Begin("Editor Debugger Menu", &show_editor_debugger_topbar, window_flags)) {
        // Play Button
        if (ImGui::Button("Play")) {
            nb::dict command;
            command["type"] = nb::str("editor_play");
            commands.append(command);
        }

        ImGui::SameLine();

        // Stop Button
        if (ImGui::Button("Stop")) {
            nb::dict command;
            command["type"] = nb::str("editor_stop");
            commands.append(command);
        }

        ImGui::SameLine();

        // Step Button
        if (ImGui::Button("Step")) {
            nb::dict command;
            command["type"] = nb::str("editor_step");
            commands.append(command);
        }

        ImGui::SameLine();

        // Exit to Editor Button
        if (ImGui::Button("Exit to Editor")) {
            nb::dict command;
            command["type"] = nb::str("exit_to_editor");
            commands.append(command);
        }

        ImGui::SameLine();

        // Settings Button
        if (ImGui::Button("Settings")) {
            GuiProgramManager::Instance()->toggleProgram("settings");
        }
    }
    ImGui::End();
}

// System-level command buffer renderer - displays pending commands in the command queue
// Similar to bash's command history rendering or kernel's command buffer display
static void __render_command_buffer_status(const nb::list& command_queue) {
    // Guard: only render if there are pending commands in the queue
    if (command_queue.size() == 0) {
        return;
    }

    // Set text color for system messages (light blue for visibility)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
    
    // Display command queue header with count
    ImGui::Text("[System] Processing %zu commands:", command_queue.size());
    
    // Limit display to prevent terminal spam (similar to dmesg ring buffer behavior)
    constexpr size_t MAX_COMMANDS_DISPLAY = 5;
    const size_t display_count = std::min(command_queue.size(), MAX_COMMANDS_DISPLAY);
    
    // Iterate through command queue and render each command's metadata
    for (size_t i = 0; i < display_count; ++i) {
        try {
            // Type-check: ensure we have a dict structure
            if (!nb::isinstance<nb::dict>(command_queue[i])) {
                continue;
            }
            
            nb::dict cmd = nb::cast<nb::dict>(command_queue[i]);
            
            // Extract command type (required field)
            if (!cmd.contains("type")) {
                continue;
            }
            
            std::string cmd_type = nb::cast<std::string>(cmd["type"]);
            std::string cmd_repr = "  - Command: " + cmd_type;
            
            // Append parameter indicator if present
            if (cmd.contains("params")) {
                cmd_repr += " (with params)";
            }
            
            ImGui::Text("%s", cmd_repr.c_str());
            
        } catch (const std::exception& e) {
            // Fallback for unparseable commands
            ImGui::Text("  - [Unable to parse command]");
        }
    }
    
    // Display overflow indicator if command queue exceeds display limit
    if (command_queue.size() > display_count) {
        ImGui::Text("  ... and %zu more commands", command_queue.size() - display_count);
    }
    
    // Restore previous text color
    ImGui::PopStyleColor();
}

// Function to render the console window
void RenderConsoleWindow(nb::list& consoleLogs, nb::list& commands) {
    ImVec4 color_DarkBlue = NormalizeColor(30, 49, 75);
    ImVec4 semiTransparentBg = NormalizeColor(30, 49, 75, 0.7f);  // Dark Blue with 70% opacity

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = semiTransparentBg;

    // Begin a new ImGui window titled "Console"
    ImGui::Begin("Console", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                     ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

    // Create a child region for the console logs with scroll functionality
    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Iterate over the consoleLogs Python list and display each log entry
    for (const auto& item : consoleLogs) {
        try {
            // Convert the Python object to a C++ string
            std::string logEntry = nb::cast<std::string>(item);

            // Display the log entry
            ImGui::TextUnformatted(logEntry.c_str());
        } catch (const nb::cast_error& e) {
            // If cast fails, display an error message instead of crashing
            std::string errorMsg = "[Console Error] Failed to display log entry: ";
            errorMsg += e.what();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorMsg.c_str());
        } catch (const std::exception& e) {
            std::string errorMsg = "[Console Error] Unexpected error: ";
            errorMsg += e.what();
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", errorMsg.c_str());
        }
    }

    // Render command buffer status (system-level command queue display)
    __render_command_buffer_status(commands);

    // Auto-scroll to the bottom if the user hasn't scrolled up manually
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    // End the child region
    ImGui::EndChild();

    // Add some spacing before the input field
    ImGui::Spacing();

    // Static buffer for command input
    static char inputBuf[256] = "";

    // InputTextFlags_EnterReturnsTrue makes the InputText return true when Enter is pressed
    if (ImGui::InputText("Command Input", inputBuf, sizeof(inputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string commandStr(inputBuf);

        // Trim whitespace from the command
        size_t first = commandStr.find_first_not_of(" \t\n\r");
        size_t last = commandStr.find_last_not_of(" \t\n\r");
        if (first != std::string::npos && last != std::string::npos) {
            commandStr = commandStr.substr(first, last - first + 1);
        } else {
            commandStr = "";
        }

        // If the command is not empty, append it to the commands list
        if (!commandStr.empty()) {
            try {
                // Parse the command string to extract command type and parameters
                // For simplicity, let's assume the command format is:
                // type param1=value1 param2=value2 ...
                std::istringstream iss(commandStr);
                std::string type;
                iss >> type;

                nb::dict params;
                std::string paramPair;
                bool hasParams = false;

                while (iss >> paramPair) {
                    size_t eqPos = paramPair.find('=');
                    if (eqPos != std::string::npos) {
                        hasParams = true;
                        std::string keyStr = paramPair.substr(0, eqPos);
                        nb::str key = nb::str(keyStr.c_str());

                        std::string valueStr = paramPair.substr(eqPos + 1);
                        nb::object value = nb::str(valueStr.c_str());

                        // Try to convert the value to int, float, or bool if possible
                        // Try integer conversion
                        try {
                            int intValue = std::stoi(valueStr);
                            value = nb::int_(intValue);
                        } catch (...) {
                            // Not an integer, continue
                        }

                        // Try float conversion
                        if (nb::isinstance<nb::str>(value)) {
                            try {
                                float floatValue = std::stof(valueStr);
                                value = nb::float_(floatValue);
                            } catch (...) {
                                // Not a float, continue
                            }
                        }

                        // Try boolean conversion
                        if (nb::isinstance<nb::str>(value)) {
                            if (valueStr == "true" || valueStr == "True") {
                                value = nb::bool_(true);
                            } else if (valueStr == "false" || valueStr == "False") {
                                value = nb::bool_(false);
                            }
                        }

                        params[key] = value;
                    }
                }

                // Create the command dict
                nb::dict command;
                command["type"] = nb::str(type.c_str());
                
                // Only add params if they exist
                if (hasParams) {
                    command["params"] = params;
                }

                // Append the command dict to the commands list
                commands.append(command);

                // Optionally, also append the command to the consoleLogs for display
                consoleLogs.append("> " + commandStr);

                // Clear the input buffer for the next command
                inputBuf[0] = '\0';

            } catch (const nb::cast_error& e) {
                // Log the error to console instead of crashing
                std::string errorMsg = "[Console Error] Failed to process command: ";
                errorMsg += e.what();
                consoleLogs.append(errorMsg);
                std::cerr << errorMsg << std::endl;
                
                // Clear the input buffer
                inputBuf[0] = '\0';
            } catch (const std::exception& e) {
                // Log any other exceptions
                std::string errorMsg = "[Console Error] Unexpected error processing command: ";
                errorMsg += e.what();
                consoleLogs.append(errorMsg);
                std::cerr << errorMsg << std::endl;
                
                // Clear the input buffer
                inputBuf[0] = '\0';
            }
        }
    }

    // Optional: Provide a button to clear the console logs
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        consoleLogs.clear();
    }

    // Display instructions or other UI elements
    ImGui::Text("Enter commands and press Enter to execute.");

    // End the ImGui window
    ImGui::End();

    // Restore the previous window background color
    style.Colors[ImGuiCol_WindowBg] = color_DarkBlue;
}

// Utility function to safely extract a string from a nb::dict
std::string get_dict_value(const nb::dict& dict, const std::string& key) {
    if (dict.contains(key.c_str())) {  // Convert std::string to const char*
        nb::object value = dict[key.c_str()];
        return nb::cast<std::string>(value);
    } else {
        std::cerr << "Warning: Key '" << key << "' not found in entity dictionary." << std::endl;
        return "";
    }
}

// Function to render the entities window
void RenderEntitiesWindow(nb::list& commands, nb::list& entitiesData) {
    ImVec4 color_DarkBlue = NormalizeColor(30, 49, 75);
    ImVec4 semiTransparentBg = NormalizeColor(30, 49, 75, 0.7f);  // Dark Blue with 70% opacity

    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = semiTransparentBg;

    // Begin a new ImGui window titled "Entities"
    // Static variable to hold the entity_type_id input
    static int entity_type_id = 0;

    // Input integer for entity_type_id
    ImGui::InputInt("Entity Type ID", &entity_type_id);

    // Button to query entities data
    if (ImGui::Button("Query Entities Data")) {
        // Create the params dict
        nb::dict params;
        params["entity_type_id"] = entity_type_id;

        // Create the command dict
        nb::dict command;
        command["type"] = nb::str("query_entities_data");
        command["params"] = params;

        // Append the command dict to the commands list
        commands.append(command);
    }

    // Add some spacing
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Check if entitiesData list is not empty
    if (entitiesData.size() > 0) {
        // Display entitiesData in a table
        if (ImGui::BeginTable("EntitiesDataTable", 3,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
            // Set up table columns
            ImGui::TableSetupColumn("ID");
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Health");
            ImGui::TableHeadersRow();

            // Iterate over entitiesData and populate the table
            for (const auto& item : entitiesData) {
                // Ensure each item is a dictionary
                if (!nb::isinstance<nb::dict>(item)) {
                    std::cerr << "Error: Entity data is not a dictionary." << std::endl;
                    continue;
                }

                nb::dict entity = nb::cast<nb::dict>(item);

                // Extract values by keys
                std::string id = get_dict_value(entity, "ID");
                std::string name = get_dict_value(entity, "Name");
                std::string health = get_dict_value(entity, "Health");

                // Populate the table row
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(id.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(name.c_str());

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(health.c_str());
            }

            ImGui::EndTable();
        }
    } else {
        // Display a message if entitiesData is empty
        ImGui::Text("No queried response");
    }

    // End the ImGui window

    // Restore the previous window background color
    style.Colors[ImGuiCol_WindowBg] = color_DarkBlue;
}

// Define constants for features and actions
// const auto FEATURE_WATER_CAMERA_STATS = "water_camera_stats";
// const auto FEATURE_TERRAIN_GRADIENT_CAMERA_STATS = "terrain_gradient_camera_stats";
constexpr const char* FEATURE_WATER_CAMERA_STATS = "water_camera_stats";
constexpr const char* FEATURE_TERRAIN_GRADIENT_CAMERA_STATS = "terrain_gradient_camera_stats";

const auto ACTION_TURN_ON = "turn_on";
const auto ACTION_TURN_OFF = "turn_off";

// bool* water_camera_stats_ptr;

void RenderEntityInterfaceWindow(std::shared_ptr<EntityInterface> entityInterface) {
    // Display the basic entity information
    ImGui::Text("Entity ID: %d", entityInterface->getEntityId());

    if (entityInterface->hasComponent(ComponentFlag::ENTITY_TYPE)) {
        EntityTypeComponent entityType = entityInterface->getComponent<EntityTypeComponent>();
        ImGui::Text("Entity Type: Main %d, Sub0 %d, Sub1 %d", entityType.mainType,
                    entityType.subType0, entityType.subType1);
        EntityEnum mainType_typed = static_cast<EntityEnum>(entityType.mainType);
        ImGui::Text("    Main %d (%s)", entityType.mainType,
                    entityMainTypeToString(mainType_typed));
        ImGui::Text("    Sub0 %d", entityType.subType0);
        ImGui::Text("    Sub1 %d", entityType.subType1);
    } else {
        ImGui::Text("Entity Type: N/A");
    }

    if (entityInterface->hasComponent(ComponentFlag::POSITION)) {
        Position pos = entityInterface->getComponent<Position>();
        ImGui::Text("Position X: %d", pos.x);
        ImGui::Text("Position Y: %d", pos.y);
        ImGui::Text("Position Z: %d", pos.z);
    } else {
        ImGui::Text("Position: N/A");
    }

    if (entityInterface->hasComponent(ComponentFlag::VELOCITY)) {
        Velocity vel = entityInterface->getComponent<Velocity>();
        ImGui::Text("Velocity VX: %.4f", vel.vx);
        ImGui::Text("Velocity VY: %.4f", vel.vy);
        ImGui::Text("Velocity VZ: %.4f", vel.vz);
    } else {
        ImGui::Text("Velocity: N/A");
    }

    if (entityInterface->hasComponent(ComponentFlag::HEALTH)) {
        HealthComponent health = entityInterface->getComponent<HealthComponent>();
        ImGui::Text("Health Current: %f", health.healthLevel);
        ImGui::Text("Max Health: %f", health.maxHealth);
    } else {
        ImGui::Text("Health: N/A");
    }

    if (entityInterface->hasComponent(ComponentFlag::MATTER_CONTAINER)) {
        MatterContainer matterContainer = entityInterface->getComponent<MatterContainer>();
        ImGui::Text("WaterMatter Container Current: %d", matterContainer.WaterMatter);
        ImGui::Text("WaterVapor Container Current: %d", matterContainer.WaterVapor);
    } else {
        ImGui::Text("Water Matter Container: N/A");
    }

    // Convert the component mask to a string for display
    std::string maskStr = entityInterface->componentMask.to_string();
    ImGui::Text("Component Mask: %s", maskStr.c_str());

    // Optionally, iterate over each component and show details for active ones.
    // COMPONENT_COUNT is assumed to be defined appropriately.
    for (int i = 0; i < COMPONENT_COUNT; ++i) {
        // Check if the i-th component is present.
        if (entityInterface->hasComponent(static_cast<ComponentFlag>(i))) {
            ImGui::Text("Component %d is active", i);
            // You can add more detailed component-specific info here if desired.
        }
    }
}

void ShowGadgetsWindow(nb::list& commands, nb::dict shared_data,
                       std::shared_ptr<EntityInterface> hoveredEntityInterface_ptr,
                       std::shared_ptr<EntityInterface> selectedEntityInterface_ptr) {
    bool waterCameraStats = GuiStateManager::Instance()->getWaterCameraStats();
    bool terrainCameraStats = GuiStateManager::Instance()->getTerrainCameraStats();
    bool showHoveredEntitiesStats = GuiStateManager::Instance()->getHoveredEntityInterfaceStats();
    bool showSelectedEntitiesStats = GuiStateManager::Instance()->getSelectedEntityInterfaceStats();

    // Display the checkbox
    if (ImGui::Checkbox("Water Camera Stats", &waterCameraStats)) {
        GuiStateManager::Instance()->setWaterCameraStats(waterCameraStats);
        // Log the action
        std::cout << "Water Camera Stats " << (waterCameraStats ? "turned on." : "turned off.")
                  << std::endl;
    }

    // Terrain Gradient Camera Stats Switch
    if (ImGui::Checkbox("Terrain Gradient Camera Stats", &terrainCameraStats)) {
        GuiStateManager::Instance()->setTerrainCameraStats(terrainCameraStats);
        // Log the action
        std::cout << "Terrain Gradient Camera Stats "
                  << (waterCameraStats ? "turned on." : "turned off.") << std::endl;
    }

    if (ImGui::Button("Hovered Entity Interface Stats")) {
        showHoveredEntitiesStats = !showHoveredEntitiesStats;
        GuiStateManager::Instance()->setHoveredEntityInterfaceStats(showHoveredEntitiesStats);
    }

    if (ImGui::Button("Selected Entity Interface Stats")) {
        showSelectedEntitiesStats = !showSelectedEntitiesStats;
        GuiStateManager::Instance()->setSelectedEntityInterfaceStats(showSelectedEntitiesStats);
    }

    /*──────────────── Entity interface ────────────*/

    if (showHoveredEntitiesStats) {
        if (ImGui::Begin("Hovered Entity Interface", &showHoveredEntitiesStats,
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderEntityInterfaceWindow(hoveredEntityInterface_ptr);
        }
        ImGui::End();
    }

    if (showSelectedEntitiesStats) {
        if (ImGui::Begin("Selected Entity Interface", &showSelectedEntitiesStats,
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            RenderEntityInterfaceWindow(selectedEntityInterface_ptr);
        }
        ImGui::End();
    }
}

void RenderAIStatisticPlot(const nb::dict& statistics, const std::string& plotName,
                           const std::string& plotTitle) {
    constexpr double timeWindow = 60.0;  // show last 60 time units
    std::map<std::string, double> stat_map;
    try {
        if (statistics.contains(plotName) && nb::isinstance<nb::dict>(statistics[plotName.c_str()]))
            stat_map = nb::cast<std::map<std::string, double>>(statistics[plotName.c_str()]);
        else
            stat_map = nb::cast<std::map<std::string, double>>(statistics);
    } catch (const nb::cast_error& e) {
        std::cerr << "Statistics dict cast failed: " << e.what() << '\n';
        stat_map.clear();
    }
    std::vector<double> xs, ys;
    xs.reserve(stat_map.size());
    ys.reserve(stat_map.size());
    for (auto& kv : stat_map) {
        try {
            xs.push_back(std::stod(kv.first));
            ys.push_back(kv.second);
        } catch (...) {
            // bad key – skip it
        }
    }
    if (!xs.empty() && xs.size() == ys.size()) {
        // compute rolling window limits so latest is at right edge
        double latest = xs.back();
        double window_left = latest - timeWindow;
        ImPlot::SetNextAxisLimits(ImAxis_X1, window_left, latest, ImPlotCond_Always);
        if (ImPlot::BeginPlot(plotTitle.c_str())) {
            ImPlot::SetupAxes("Timestamp", "Value");
            ImPlot::PlotBars(plotName.c_str(), xs.data(), ys.data(), static_cast<int>(xs.size()),
                             0.67);
            ImPlot::EndPlot();
        }

        // ImPlot::EndSubplots();
    } else {
        ImGui::TextUnformatted("Failed to create subplots.");
    }
}

// Render AI Statistics content
void RenderAIStatisticsWindow(const nb::dict& statistics) {
    RenderAIStatisticPlot(statistics, "population_size", "Population Size");
    RenderAIStatisticPlot(statistics, "inference_queue_size", "Inference Queue Size");
    RenderAIStatisticPlot(statistics, "action_queue_size", "Action Queue Size");
    RenderAIStatisticPlot(statistics, "population_mean", "Population inference interval Mean");
    RenderAIStatisticPlot(statistics, "population_max", "Population inference interval Max");
    RenderAIStatisticPlot(statistics, "population_min", "Population inference interval Min");
}

// Simple self-contained text editor window using ImGui's built-in text input
void renderTextEditorWindow(bool* showTextEditor, nb::dict& shared_data) {
    if (!showTextEditor || !*showTextEditor) {
        return;
    }

    // Static text buffer - persists between calls
    // Use a large enough buffer for text editing
    static char textBuffer[65536] =
        "demo: dict[str, Any] = {\n"
        "    \"name\": \"box_solid\",\n"
        "    \"dims\": [16, 16, 32],  # X,Y,Z\n"
        "    \"palette\": [\n"
        "        \"#00000000\",  # 0 transparent\n"
        "        \"#c0c0c0ff\",  # 1 light gray (faces)\n"
        "        \"#404040ff\",  # 2 dark gray  (optional edges)\n"
        "    ],\n"
        "    \"voxels_sparse\": [\n"
        "        # 8x8x8 cube centered at (8,8,8) -> x,y in [4..11], z in [4..11]\n"
        "        *[\n"
        "            {\"x\": x, \"y\": y, \"z\": z, \"color\": 1}\n"
        "            for x in range(1, 16)\n"
        "            for y in range(1, 16)\n"
        "            for z in range(1, 16)\n"
        "        ]\n"
        "    ],\n"
        "}\n"
        "dims, palette, vox = load_model(demo)\n"
        "out = render(dims, palette, vox, out_path=\"box_solid.png\", scale=1)\n"
        "editing_sprite = Sprite(renderer, \"box_solid\", \"box_solid.png\", x=150, y=150, "
        "scale_x=64, scale_y=64)\n"
        "editing_sprite.render()\n";

    // Static flags for editor state
    static bool isDirty = false;
    static bool readOnly = false;

    // Set window size on first use
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Simple Text Editor", showTextEditor, ImGuiWindowFlags_MenuBar)) {
        // Menu bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    textBuffer[0] = '\0';  // Clear the buffer
                    isDirty = false;
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    // TODO: Implement actual file saving
                    std::cout << "Saving text: " << strlen(textBuffer) << " characters"
                              << std::endl;

                    // Safely add text content to shared_data using nanobind string conversion
                    try {
                        shared_data["text_editor_content"] = nb::str(textBuffer);
                    } catch (const std::exception& e) {
                        std::cerr << "Error storing text editor content: " << e.what() << std::endl;
                    }

                    isDirty = false;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close")) {
                    *showTextEditor = false;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Read-only mode", nullptr, &readOnly)) {
                    // Toggle read-only mode
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear All")) {
                    textBuffer[0] = '\0';  // Clear the buffer
                    isDirty = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Status line
        size_t textLength = strlen(textBuffer);
        size_t lineCount = 1;
        for (size_t i = 0; i < textLength; i++) {
            if (textBuffer[i] == '\n') {
                lineCount++;
            }
        }

        ImGui::Text("Lines: %zu | Characters: %zu | %s | %s", lineCount, textLength,
                    readOnly ? "Read-Only" : "Edit", isDirty ? "Modified" : "Saved");

        ImGui::Separator();

        // Text editing area
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
        if (readOnly) {
            flags |= ImGuiInputTextFlags_ReadOnly;
        }

        // Use a resizable text input that fills the remaining space
        ImVec2 textSize = ImVec2(-1, -1);  // Fill available space

        if (ImGui::InputTextMultiline("##TextEditor", textBuffer, sizeof(textBuffer), textSize,
                                      flags)) {
            isDirty = true;
        }
    }
    ImGui::End();
}

// Simple utility functions for text editor content access
std::string getTextEditorContent() {
    // Note: This is a simplified version. To access the actual content,
    // we would need to make the textBuffer global or implement a proper class
    return "// To get actual content, the textBuffer would need to be accessible from here";
}

void setTextEditorContent(const std::string& content) {
    // Note: This is a simplified version. To actually set content,
    // the textBuffer would need to be accessible from here.
    std::cout << "Setting text content: " << content.length() << " characters" << std::endl;
}

void showTextEditorWindow() { showTextEditor = true; }

void renderInGameGuiFrame(int worldTicks, float availableFps, std::shared_ptr<World> world_ptr,
                          nb::dict physicsChanges, nb::dict inventoryData, nb::list& consoleLogs,
                          nb::list& entitiesData, nb::list& commands, nb::dict statistics,
                          nb::dict& shared_data,
                          std::shared_ptr<EntityInterface> entityInterface_ptr,
                          std::shared_ptr<EntityInterface> hoveredEntityInterface_ptr,
                          std::shared_ptr<EntityInterface> selectedEntityInterface_ptr) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    
    /*──────────────── Build GUI Context ────────────────*/
    GuiContext context{
        .worldTicks = worldTicks,
        .availableFps = availableFps,
        .worldPtr = world_ptr,
        .physicsChanges = physicsChanges,
        .inventoryData = inventoryData,
        .consoleLogs = consoleLogs,
        .entitiesData = entitiesData,
        .commands = commands,
        .statistics = statistics,
        .sharedData = shared_data,
        .entityInterfacePtr = entityInterface_ptr,
        .hoveredEntityInterfacePtr = hoveredEntityInterface_ptr,
        .selectedEntityInterfacePtr = selectedEntityInterface_ptr
    };
    
    /*──────────────── Render always-on components ────────────────*/
    RenderEditorDebuggerTopBar(commands);
    RenderTopBar();
    
    /*──────────────── Process activate_program commands ────────────────*/
    // Handle program activation commands before rendering
    std::vector<std::string> programsToActivate;
    for (size_t i = 0; i < commands.size(); ++i) {
        try {
            if (nb::isinstance<nb::dict>(commands[i])) {
                nb::dict cmd = nb::cast<nb::dict>(commands[i]);
                if (cmd.contains("type") && nb::cast<std::string>(cmd["type"]) == "activate_program") {
                    if (cmd.contains("program_id")) {
                        std::string programId = nb::cast<std::string>(cmd["program_id"]);
                        programsToActivate.push_back(programId);
                    }
                }
            }
        } catch (...) {
            // Skip malformed commands
        }
    }
    
    // Activate requested programs
    for (const auto& programId : programsToActivate) {
        GuiProgramManager::Instance()->activateProgram(programId);
    }
    
    /*──────────────── Render all active GUI programs (GUI OS) ────────────────*/
    GuiProgramManager::Instance()->renderAllPrograms(context);
    
    /*──────────────── Render hotbar (always visible when inventory closed) ────────────────*/
    // The hotbar is always shown unless inventory is active
    if (!GuiProgramManager::Instance()->isProgramActive("inventory")) {
        std::vector<InventoryItem> items = LoadInventory(inventoryData);
        GuiStateManager::Instance()->hotbarWindow.setItems(items);
        GuiStateManager::Instance()->hotbarWindow.setCommands(commands);
        GuiStateManager::Instance()->hotbarWindow.Render();
    }
    
    /*──────────────── Drag‑drop handling (always processed) ────────────────*/
    HandleDragDropToWorld(commands);
    
    // ImGui::ShowDemoWindow();  // Show demo window! :)
}

void imguiPrepareEditorWindows(nb::list& commands, nb::dict& shared_data,
                               nb::ndarray<nb::numpy> voxel_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Access numpy array properties for voxel data
    // Example usage:
    // - voxel_data.data() -> pointer to the data
    // - voxel_data.shape(i) -> size of dimension i
    // - voxel_data.ndim() -> number of dimensions
    // - voxel_data.dtype() -> data type

    // Create a dockspace over the main viewport (if docking is available)
    // #ifdef IMGUI_HAS_DOCK
    // ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(),
    // ImGuiDockNodeFlags_PassthruCentralNode); #endif
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);

    // Set initial window size and position for when not docked
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(500, 400);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    // Set position and size only on first use to allow docking to work
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

    // Remove AlwaysAutoResize to allow proper docking behavior
    ImGui::Begin("Title Screen", nullptr, ImGuiWindowFlags_None);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Game title - big text centered with larger font scale
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::SetWindowFontScale(2.5f);                  // Scale font to 2.5x larger
    ImVec2 titleSize = ImGui::CalcTextSize("LIFE SIMULATION GAME");
    ImGui::SetCursorPosX((windowSize.x - titleSize.x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "LIFE SIMULATION GAME");
    ImGui::SetWindowFontScale(1.0f);  // Reset font scale to normal
    ImGui::PopFont();

    // Add spacing before buttons
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Button styling
    ImVec2 buttonSize = ImVec2(200, 40);
    float buttonPosX = (windowSize.x - buttonSize.x) * 0.5f;

    // Start Game Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Start Game", buttonSize)) {
        // Create command to start the game
        nb::dict command;
        command["type"] = nb::str("start_game");
        commands.append(command);
    }

    ImGui::Spacing();

    // Settings Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Settings", buttonSize)) {
        // Create command to open settings
        nb::dict command;
        command["type"] = nb::str("open_settings");
        commands.append(command);
    }

    ImGui::Spacing();

    // Credits Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Credits", buttonSize)) {
        // Create command to show credits
        nb::dict command;
        command["type"] = nb::str("show_credits");
        commands.append(command);
    }

    ImGui::Spacing();

    // Quit Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Quit", buttonSize)) {
        // Create command to quit the game
        nb::dict command;
        command["type"] = nb::str("quit_game");
        commands.append(command);
    }

    ImGui::End();

    // ================================================================================================
    // 3D VIEWPORT WITH IMGUIZMO - Voxel Box Visualization
    // ================================================================================================

    // Render the 3D voxel viewport in a separate function
    render3DVoxelViewport(voxel_data, shared_data);

    showTextEditor = true;
    renderTextEditorWindow(&showTextEditor, shared_data);
}

void imguiPrepareTitleWindows(nb::list& commands, nb::dict& shared_data) {
    /*──────────────── Frame setup ────────────────*/
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // Center the title window on screen
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize = ImVec2(500, 400);
    ImVec2 windowPos =
        ImVec2((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin("Title Screen", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    // Add some spacing from the top
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Game title - big text centered with larger font scale
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);  // Use default font
    ImGui::SetWindowFontScale(2.5f);                  // Scale font to 2.5x larger
    ImVec2 titleSize = ImGui::CalcTextSize("LIFE SIMULATION GAME");
    ImGui::SetCursorPosX((windowSize.x - titleSize.x) * 0.5f);
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "LIFE SIMULATION GAME");
    ImGui::SetWindowFontScale(1.0f);  // Reset font scale to normal
    ImGui::PopFont();

    // Add spacing before buttons
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();

    // Button styling
    ImVec2 buttonSize = ImVec2(200, 40);
    float buttonPosX = (windowSize.x - buttonSize.x) * 0.5f;

    // Start Game Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Start Game", buttonSize)) {
        // Create command to start the game
        nb::dict command;
        command["type"] = nb::str("start_game");
        commands.append(command);
    }

    ImGui::Spacing();

    // Settings Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Settings", buttonSize)) {
        // Create command to open settings
        nb::dict command;
        command["type"] = nb::str("open_settings");
        commands.append(command);
    }

    ImGui::Spacing();

    // Credits Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Credits", buttonSize)) {
        // Create command to show credits
        nb::dict command;
        command["type"] = nb::str("show_credits");
        commands.append(command);
    }

    ImGui::Spacing();

    // Quit Button
    ImGui::SetCursorPosX(buttonPosX);
    if (ImGui::Button("Quit", buttonSize)) {
        // Create command to quit the game
        nb::dict command;
        command["type"] = nb::str("quit_game");
        commands.append(command);
    }

    ImGui::End();
}

void imguiRender(uintptr_t renderer_ptr) {
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    // Skip if there's nothing to draw
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) {
        return;
    }
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(renderer_ptr);
    ImGui_ImplSDLRenderer2_RenderDrawData(draw_data, renderer);
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
}
