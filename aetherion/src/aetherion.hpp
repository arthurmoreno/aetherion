// #include <SDL2/SDL.h>
// #include <SDL2/SDL_image.h>
// #include <imgui/backends/imgui_impl_sdl2.h>
// #include <imgui/backends/imgui_impl_sdlrenderer2.h>
#define ENTT_ENTITY_TYPE int

#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unique_ptr.h>
#include <nanobind/stl/variant.h>
#include <spdlog/spdlog.h>

#include <cstdint>
// clang-format off
#include "CameraUtils.hpp"
#include "EntityInterface.hpp"
#include "Gui/Gui.hpp"
#include "Gui/GuiStateManager.hpp"
#include "ItemConfiguration.hpp"
#include "ItemConfigurationManager.hpp"
#include "Logger.hpp"
#include "LowLevelRenderer/SceneGraph.hpp"
#include "LowLevelRenderer/RenderQueue.hpp"
#include "LowLevelRenderer/TextureManager.hpp"
#include "PyRegistry.hpp"
#include "VoxelGrid.hpp"
#include "World.hpp"
#include "neat/genome.hpp"
#include "terrain/TerrainGridRepository.hpp"
#include "terrain/TerrainStorage.hpp"
// clang-format on
