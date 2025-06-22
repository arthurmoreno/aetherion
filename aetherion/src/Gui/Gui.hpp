#ifndef __DebuggingTools__
#define __DebuggingTools__

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <imgui/backends/imgui_impl_sdl2.h>
#include <imgui/backends/imgui_impl_sdlrenderer2.h>  // Updated backend header
#include <imgui/imgui.h>
#include <imgui/implot.h>
#include <nanobind/nanobind.h>

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
void imguiInit(uintptr_t window_ptr, uintptr_t renderer_ptr);

void imguiProcessEvent(nb::bytes event_bytes);

// void imguiPrepareWindows(int worldTicks, float availableFps);
void imguiPrepareWindows(int worldTicks, float availableFps, std::shared_ptr<World> world_ptr,
                         nb::dict physicsChanges, nb::dict inventoryData, nb::list& consoleLogs,
                         nb::list& entitiesData, nb::list& commands, nb::dict statistics,
                         nb::dict& shared_data,
                         std::shared_ptr<EntityInterface> entityInterface_ptr);

void imguiPrepareTitleWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareWorldFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareWorldListWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareCharacterFormWindows(nb::list& commands, nb::dict& shared_data);

void imguiPrepareCharacterListWindows(nb::list& commands, nb::dict& shared_data);

void imguiRender(uintptr_t renderer_ptr);

#endif /* defined(__SDL_Game_Programming_Book__DebuggingTools__) */