#ifndef __DebuggingTools__
#define __DebuggingTools__

// clang-format off
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>  // Updated backend header
#include <imgui/imgui.h>
#include <imgui/implot.h>
#include <imgui/ImGuizmo.h>
// clang-format on
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>

#include <functional>
#include <map>
#include <string>
#include <unordered_map>

#include "EntityInterface.hpp"
#include "ItemWindow.hpp"
#include "World.hpp"

namespace nb = nanobind;

class GuiStateManager;
struct BoolHandle {
    // Constructor to initialize the handle
    explicit BoolHandle(bool initial_value = false)
        : handle(std::make_unique<bool>(initial_value)) {}

    // Get a pointer to the underlying value
    bool* get() const { return handle.get(); }

    // Get the current value
    bool value() const { return *handle; }

    // Set a new value
    void set(bool new_value) { *handle = new_value; }

   private:
    std::unique_ptr<bool> handle;  // Unique ownership of the underlying bool
};

bool wants_capture_keyboard();

// Function to check if ImGui wants to capture mouse
bool wants_capture_mouse();

// Function to process SDL_Event using ImGui's SDL2 backend
void imguiInit(uintptr_t window_ptr, uintptr_t renderer_ptr, const char* fontPath);

/**
 * @brief Initialize and register all GUI programs
 *
 * Called during ImGui initialization to register all available GUI programs
 * with the GuiProgramManager.
 */
void initializeGuiPrograms();

void imguiProcessEvent(nb::bytes event_bytes);

/**
 * @brief Renders the complete ImGui frame for in-game (gameplay) mode.
 *
 * This function orchestrates the rendering of all GUI windows and panels during active gameplay.
 * It handles frame initialization, renders various game windows (settings, inventory, stats, etc.),
 * processes user input through the console, and manages drag-drop operations between UI elements
 * and the game world.
 *
 * @param worldTicks Current game world tick count (simulation time step)
 * @param availableFps Current frames per second for performance monitoring
 * @param world_ptr Shared pointer to the game world (can be nullptr when world is not loaded)
 * @param physicsChanges Dictionary containing physics settings that can be modified via GUI
 * @param inventoryData Dictionary containing current player inventory state
 * @param consoleLogs List of console log messages to display
 * @param entitiesData List of entity data for the entities stats window
 * @param commands Output list where GUI-generated commands are appended (e.g., item transfers,
 * settings changes)
 * @param statistics Dictionary containing AI statistics for visualization
 * @param shared_data Shared state dictionary for inter-module communication
 * @param entityInterface_ptr Interface to a generic entity for inspection
 * @param hoveredEntityInterface_ptr Interface to the currently hovered entity (if any)
 * @param selectedEntityInterface_ptr Interface to the currently selected entity (if any)
 *
 * @note This function is called once per frame during gameplay mode.
 * @note The function renders windows conditionally based on user interactions (toggle flags).
 */
void renderInGameGuiFrame(int worldTicks, float availableFps, std::shared_ptr<World> world_ptr,
                          nb::dict physicsChanges, nb::dict inventoryData, nb::list& consoleLogs,
                          nb::list& entitiesData, nb::list& commands, nb::dict statistics,
                          nb::dict& shared_data,
                          std::shared_ptr<EntityInterface> entityInterface_ptr,
                          std::shared_ptr<EntityInterface> hoveredEntityInterface_ptr,
                          std::shared_ptr<EntityInterface> selectedEntityInterface_ptr);

void imguiPrepareEditorRuntimeDebuggerWindows(nb::list& commands, nb::dict& shared_data);
void imguiPrepareEditorWindows(nb::list& commands, nb::dict& shared_data,
                               nb::ndarray<nb::numpy> voxel_data);
void render3DVoxelViewport(nb::ndarray<nb::numpy>& voxel_data, nb::dict& shared_data);
void imguiPrepareTitleWindows(nb::list& commands, nb::dict& shared_data);

/**
 * @brief Renders the Editor Debugger Top Bar window
 *
 * Displays a collapsible, draggable toolbar with editor controls for Play, Stop, Step,
 * Exit to Editor, and Settings. This toolbar is used during editor runtime debugging mode.
 *
 * @param commands Output list where editor commands are appended (e.g., Play, Stop, Step)
 */
void RenderEditorDebuggerTopBar(nb::list& commands);

void imguiPrepareWorldTypeFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareServerWorldFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareWorldFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareWorldListWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareCharacterFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareCharacterListWindows(nb::list& commands, nb::dict& shared_data);

void imguiRender(uintptr_t renderer_ptr);

#endif /* defined(__SDL_Game_Programming_Book__DebuggingTools__) */