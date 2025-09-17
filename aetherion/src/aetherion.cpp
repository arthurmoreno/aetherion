// #include <SDL2/SDL.h>
// #include <SDL2/SDL_image.h>
// #include <imgui/backends/imgui_impl_sdl2.h>
// #include <imgui/backends/imgui_impl_sdlrenderer2.h>
// #define ENTT_ENTITY_TYPE int

#include "aetherion.hpp"

// #include <nanobind/nanobind.h>
// #include <nanobind/operators.h>
// #include <nanobind/stl/bind_map.h>
// #include <nanobind/stl/bind_vector.h>
// #include <nanobind/stl/string.h>
// #include <nanobind/stl/unique_ptr.h>
// #include <nanobind/stl/variant.h>
#include <spdlog/spdlog.h>

#include <cstdint>

// #include "CameraUtils.hpp"
// #include "EntityInterface.hpp"
// #include "Gui/Gui.hpp"
// #include "Gui/GuiStateManager.hpp"
// #include "ItemConfiguration.hpp"
// #include "ItemConfigurationManager.hpp"
// #include "Logger.hpp"
// #include "LowLevelRenderer/RenderQueue.hpp"
// #include "LowLevelRenderer/TextureManager.hpp"
// #include "PyRegistry.hpp"
// #include "VoxelGrid.hpp"
// #include "World.hpp"
// #include "neat/genome.hpp"
#include "PhysicsSettings.hpp"

// Create a shortcut for nanobind
namespace nb = nanobind;
NB_MAKE_OPAQUE(entt::entity);

// Helper function to convert std::variant to nb::object
nb::object variant_to_pyobject(const ItemConfiguration::DefaultValue& var) {
    return std::visit([](auto&& arg) -> nb::object { return nb::cast(arg); }, var);
}

NB_MODULE(_aetherion, m) {
    nb::bind_map<std::map<std::string, int>>(m, "MapStrInt");
    nb::bind_map<std::map<std::string, std::string>>(m, "MapStrStr");
    nb::bind_map<std::map<std::string, double>>(m, "MapStrDouble");
    nb::bind_map<std::map<std::string, std::map<std::string, std::string>>>(m, "MapStrMapStrStr");
    nb::bind_map<std::map<std::string, std::map<std::string, double>>>(m, "MapStrMapStrDouble");
    nb::bind_vector<std::vector<std::string>>(m, "VecStr");
    nb::bind_vector<std::vector<int>>(m, "VecInt");

    // Expose EntityEnum as constants
    m.attr("EntityEnum_TERRAIN") = static_cast<int>(EntityEnum::TERRAIN);
    m.attr("EntityEnum_BEAST") = static_cast<int>(EntityEnum::BEAST);
    m.attr("EntityEnum_PLANT") = static_cast<int>(EntityEnum::PLANT);
    m.attr("EntityEnum_TILE_EFFECT") = static_cast<int>(EntityEnum::TILE_EFFECT);

    m.attr("PlantEnum_RASPBERRY") = static_cast<int>(PlantEnum::RASPBERRY);

    // Expose TerrainEnum as constants
    m.attr("TerrainEnum_EMPTY") = static_cast<int>(TerrainEnum::EMPTY);
    m.attr("TerrainEnum_GRASS") = static_cast<int>(TerrainEnum::GRASS);
    m.attr("TerrainEnum_WATER") = static_cast<int>(TerrainEnum::WATER);

    // Expose TerrainEnum as constants
    m.attr("DirectionEnum_UP") = static_cast<int>(DirectionEnum::UP);
    m.attr("DirectionEnum_RIGHT") = static_cast<int>(DirectionEnum::RIGHT);
    m.attr("DirectionEnum_DOWN") = static_cast<int>(DirectionEnum::DOWN);
    m.attr("DirectionEnum_LEFT") = static_cast<int>(DirectionEnum::LEFT);
    m.attr("DirectionEnum_UPWARD") = static_cast<int>(DirectionEnum::UPWARD);
    m.attr("DirectionEnum_DOWNWARD") = static_cast<int>(DirectionEnum::DOWNWARD);

    // Expose the Logger class
    nb::class_<Logger>(m, "Logger")
        .def_static("initialize", &Logger::initialize, "Initialize the logger")
        .def_static("get_logger", &Logger::getLogger, "Get the logger instance")
        .def("info", &Logger::info, "Log an info message")
        .def("warn", &Logger::warn, "Log a warning message")
        .def("error", &Logger::error, "Log an error message")
        .def("critical", &Logger::critical, "Log a critical message")
        .def("debug", &Logger::debug, "Log a debug message")
        .def("trace", &Logger::trace, "Log a trace message");

    m.def("imgui_init", &imguiInit, "Load an image and create a texture", nb::arg("window_ptr"),
          nb::arg("gl_context_ptr"));

    m.def(
        "imgui_prepare_windows",
        // lambda wrapper that takes a nullable shared_ptr<World>
        [](int worldTicks, float availableFps,
           std::shared_ptr<World> world_ptr,  // now can be nullptr
           nb::dict physicsChanges, nb::dict inventoryData, nb::list consoleLogs,
           nb::list entitiesData, nb::list commands, nb::dict statistics, nb::dict& shared_data,
           std::shared_ptr<EntityInterface> entityInterface_ptr,
           std::shared_ptr<EntityInterface> hoveredEntityInterface_ptr,
           std::shared_ptr<EntityInterface> selectedEntityInterface_ptr) {
            imguiPrepareWindows(worldTicks, availableFps, world_ptr, physicsChanges, inventoryData,
                                consoleLogs, entitiesData, commands, statistics, shared_data,
                                entityInterface_ptr, hoveredEntityInterface_ptr,
                                selectedEntityInterface_ptr);
        },
        nb::arg("worldTicks"), nb::arg("availableFps"),
        nb::arg("world_ptr") = nullptr,  // <-- makes None legal
        nb::arg("physicsChanges"), nb::arg("inventoryData"), nb::arg("consoleLogs"),
        nb::arg("entitiesData"), nb::arg("commands"), nb::arg("statistics"), nb::arg("shared_data"),
        nb::arg("entityInterface_ptr"), nb::arg("hoveredEntityInterface_ptr"),
        nb::arg("selectedEntityInterface_ptr"));
    m.def("imgui_prepare_title_windows", &imguiPrepareTitleWindows, nb::arg("commands"),
          nb::arg("shared_data"), "Prepare the title windows for ImGui rendering");
    m.def("imgui_prepare_world_type_form_windows", &imguiPrepareWorldTypeFormWindows,
          nb::arg("commands"), nb::arg("shared_data"),
          "Prepare the world type selection form windows for ImGui rendering");
    m.def("imgui_prepare_server_world_form_windows", &imguiPrepareServerWorldFormWindows,
          nb::arg("commands"), nb::arg("shared_data"),
          "Prepare the server world form windows for ImGui rendering");
    m.def("imgui_prepare_world_form_windows", &imguiPrepareWorldFormWindows, nb::arg("commands"),
          nb::arg("shared_data"), "Prepare the world creation form windows for ImGui rendering");
    m.def("imgui_prepare_world_list_windows", &imguiPrepareWorldListWindows, nb::arg("commands"),
          nb::arg("shared_data"), "Prepare the world list selection windows for ImGui rendering");
    m.def("imgui_prepare_character_form_windows", &imguiPrepareCharacterFormWindows,
          nb::arg("commands"), nb::arg("shared_data"),
          "Prepare the character creation form windows for ImGui rendering");
    m.def("imgui_prepare_character_list_windows", &imguiPrepareCharacterListWindows,
          nb::arg("commands"), nb::arg("shared_data"),
          "Prepare the character list selection windows for ImGui rendering");
    m.def("imgui_process_event", &imguiProcessEvent);
    m.def("imgui_render", &imguiRender, "Load an image and create a texture",
          nb::arg("renderer_ptr"));
    m.def("load_texture", &load_texture, "Load an image and create a texture",
          nb::arg("renderer_ptr"), nb::arg("image_path"));
    m.def("render_texture", &render_texture, "Render a texture", nb::arg("renderer_ptr"),
          nb::arg("texture_ptr"), nb::arg("x"), nb::arg("y"));
    m.def("destroy_texture", &destroy_texture, "Destroy a texture", nb::arg("texture_ptr"));

    m.def("load_texture_on_manager", &loadTextureOnManager);
    m.def("render_texture_from_manager", &renderTextureFromManager);

    // OpenGL texture functions
    m.def("load_texture_gl", &load_texture_gl, "Load an image and create an OpenGL texture",
          nb::arg("gl_context_ptr"), nb::arg("image_path"));
    m.def("render_texture_gl", &render_texture_gl, "Render an OpenGL texture",
          nb::arg("gl_context_ptr"), nb::arg("texture_id"), nb::arg("x"), nb::arg("y"),
          nb::arg("width") = -1, nb::arg("height") = -1);
    m.def("destroy_texture_gl", &destroy_texture_gl, "Destroy an OpenGL texture",
          nb::arg("texture_id"));

    // OpenGL texture manager functions
    m.def("load_texture_on_manager_gl", &loadTextureOnManagerGL, "Load texture into OpenGL manager",
          nb::arg("gl_context_ptr"), nb::arg("imagePath"), nb::arg("id"), nb::arg("newWidth") = -1,
          nb::arg("newHeight") = -1);
    m.def("render_texture_from_manager_gl", &renderTextureFromManagerGL,
          "Render texture from OpenGL manager", nb::arg("gl_context_ptr"), nb::arg("id"),
          nb::arg("x"), nb::arg("y"), nb::arg("width") = -1, nb::arg("height") = -1);
    m.def("get_texture_from_manager_gl", &getTextureFromManagerGL,
          "Get OpenGL texture ID from manager", nb::arg("id"));

    m.def("wants_capture_keyboard", &wants_capture_keyboard,
          "Returns True if ImGui wants to capture keyboard inputs, False otherwise");

    m.def("wants_capture_mouse", &wants_capture_mouse,
          "Returns True if ImGui wants to capture mouse inputs, False otherwise");

    m.def("get_water_camera_stats", &getWaterCameraStats,
          "Returns value if the water camera feature is on or off.");
    m.def("get_terrain_camera_stats", &getTerrainCameraStats,
          "Returns value if the water camera feature is on or off");

    // TerrainStorage bindings
    nb::class_<TerrainStorage>(m, "TerrainStorage")
        .def(nb::init<>())
        .def("initialize", &TerrainStorage::initialize)
        .def("apply_transform", &TerrainStorage::applyTransform, nb::arg("voxel_size"))
        .def("mem_usage", &TerrainStorage::memUsage)
        // Entity type components
        .def("set_terrain_main_type", &TerrainStorage::setTerrainMainType)
        .def("get_terrain_main_type", &TerrainStorage::getTerrainMainType)
        .def("set_terrain_sub_type0", &TerrainStorage::setTerrainSubType0)
        .def("get_terrain_sub_type0", &TerrainStorage::getTerrainSubType0)
        .def("set_terrain_sub_type1", &TerrainStorage::setTerrainSubType1)
        .def("get_terrain_sub_type1", &TerrainStorage::getTerrainSubType1)
        // Matter containers
        .def("set_terrain_matter", &TerrainStorage::setTerrainMatter)
        .def("get_terrain_matter", &TerrainStorage::getTerrainMatter)
        .def("set_terrain_water_matter", &TerrainStorage::setTerrainWaterMatter)
        .def("get_terrain_water_matter", &TerrainStorage::getTerrainWaterMatter)
        .def("set_terrain_vapor_matter", &TerrainStorage::setTerrainVaporMatter)
        .def("get_terrain_vapor_matter", &TerrainStorage::getTerrainVaporMatter)
        .def("set_terrain_biomass_matter", &TerrainStorage::setTerrainBiomassMatter)
        .def("get_terrain_biomass_matter", &TerrainStorage::getTerrainBiomassMatter)
        // Physics
        .def("set_terrain_mass", &TerrainStorage::setTerrainMass)
        .def("get_terrain_mass", &TerrainStorage::getTerrainMass)
        .def("set_terrain_max_speed", &TerrainStorage::setTerrainMaxSpeed)
        .def("get_terrain_max_speed", &TerrainStorage::getTerrainMaxSpeed)
        .def("set_terrain_min_speed", &TerrainStorage::setTerrainMinSpeed)
        .def("get_terrain_min_speed", &TerrainStorage::getTerrainMinSpeed)
        // Flags
        .def("set_flag_bits", &TerrainStorage::setFlagBits)
        .def("get_flag_bits", &TerrainStorage::getFlagBits)
        .def("set_terrain_max_load_capacity", &TerrainStorage::setTerrainMaxLoadCapacity)
        .def("get_terrain_max_load_capacity", &TerrainStorage::getTerrainMaxLoadCapacity)
        // Activity and maintenance
        .def("is_active", &TerrainStorage::isActive)
        .def("prune", &TerrainStorage::prune);

    nb::class_<TerrainGridRepository>(m, "TerrainGridRepository")
        .def(nb::init<entt::registry&, TerrainStorage&>())
        .def("is_active", &TerrainGridRepository::isActive)
        .def("read_terrain_info", &TerrainGridRepository::readTerrainInfo)
        .def("get_terrain_entity_type", &TerrainGridRepository::getTerrainEntityType)
        .def("set_terrain_entity_type", &TerrainGridRepository::setTerrainEntityType)
        .def("get_terrain_matter_container", &TerrainGridRepository::getTerrainMatterContainer)
        .def("set_terrain_matter_container", &TerrainGridRepository::setTerrainMatterContainer)
        .def("get_matter_state", &TerrainGridRepository::getMatterState)
        .def("set_matter_state", &TerrainGridRepository::setMatterState)
        .def("get_position", &TerrainGridRepository::getPosition)
        .def("set_position", &TerrainGridRepository::setPosition)
        .def("get_physics_stats", &TerrainGridRepository::getPhysicsStats)
        .def("set_physics_stats", &TerrainGridRepository::setPhysicsStats);

    // nb::class_<RenderTask>(m, "RenderTask")
    //     .def(nb::init<SDL_Texture*, int, int>())
    //     .def_rw("texture", &RenderTask::texture)
    //     .def_rw("x", &RenderTask::x)
    //     .def_rw("y", &RenderTask::y);

    // Expose TextureQuadrant enum for partial rendering
    nb::enum_<RenderQueue::TextureQuadrant>(m, "TextureQuadrant")
        .value("TOP_LEFT", RenderQueue::TextureQuadrant::TOP_LEFT)
        .value("TOP_RIGHT", RenderQueue::TextureQuadrant::TOP_RIGHT)
        .value("BOTTOM_LEFT", RenderQueue::TextureQuadrant::BOTTOM_LEFT)
        .value("BOTTOM_RIGHT", RenderQueue::TextureQuadrant::BOTTOM_RIGHT)
        .export_values();

    nb::class_<RenderQueue>(m, "RenderQueue")
        .def(nb::init<>())
        .def("add_task_by_id", &RenderQueue::add_task_by_id)
        .def("add_task_by_texture", &RenderQueue::add_task_by_texture)

        // Partial texture rendering methods
        .def("add_task_by_id_partial", &RenderQueue::add_task_by_id_partial, nb::arg("z_layer"),
             nb::arg("priority_group"), nb::arg("texture_id"), nb::arg("x"), nb::arg("y"),
             nb::arg("lightIntensity"), nb::arg("opacity"), nb::arg("src_x"), nb::arg("src_y"),
             nb::arg("src_w"), nb::arg("src_h"),
             "Add a RenderTextureTask with partial texture rendering by texture ID")
        .def("add_task_by_texture_partial", &RenderQueue::add_task_by_texture_partial,
             nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("texture_ptr"), nb::arg("x"),
             nb::arg("y"), nb::arg("lightIntensity"), nb::arg("opacity"), nb::arg("src_x"),
             nb::arg("src_y"), nb::arg("src_w"), nb::arg("src_h"),
             "Add a RenderTextureTask with partial texture rendering by texture pointer")

        // Convenience methods for common partial texture rendering scenarios
        .def(
            "add_task_by_id_quadrant",
            [](RenderQueue& self, int z_layer, const std::string& priority_group,
               const std::string& texture_id, int x, int y, float lightIntensity, float opacity,
               int quadrant) {
                RenderQueue::TextureQuadrant quad =
                    static_cast<RenderQueue::TextureQuadrant>(quadrant);
                self.add_task_by_id_quadrant(z_layer, priority_group, texture_id, x, y,
                                             lightIntensity, opacity, quad);
            },
            nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("texture_id"), nb::arg("x"),
            nb::arg("y"), nb::arg("lightIntensity"), nb::arg("opacity"), nb::arg("quadrant"),
            "Add a RenderTextureTask for a specific quadrant (0=TOP_LEFT, 1=TOP_RIGHT, "
            "2=BOTTOM_LEFT, 3=BOTTOM_RIGHT)")
        .def("add_task_by_id_fraction", &RenderQueue::add_task_by_id_fraction, nb::arg("z_layer"),
             nb::arg("priority_group"), nb::arg("texture_id"), nb::arg("x"), nb::arg("y"),
             nb::arg("lightIntensity"), nb::arg("opacity"), nb::arg("x_start_ratio"),
             nb::arg("y_start_ratio"), nb::arg("width_ratio"), nb::arg("height_ratio"),
             "Add a RenderTextureTask for a fractional portion of texture (ratios 0.0-1.0)")
        // Updated add_task_rect binding to accept a tuple
        .def(
            "add_task_rect",
            [](RenderQueue& self, int z_layer, const std::string& priority_group, int x, int y,
               int width, int height, nb::tuple color) {
                // Default alpha to 255 (opaque) if not provided
                SDL_Color sdl_color = {0, 0, 0, 255};

                if (color.size() == 3) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                } else if (color.size() == 4) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                    sdl_color.a = nb::cast<uint8_t>(color[3]);
                } else {
                    throw std::invalid_argument(
                        "Color tuple must have 3 (RGB) or 4 (RGBA) elements");
                }

                self.add_task_rect(z_layer, priority_group, x, y, width, height, sdl_color);
            },
            nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("x"), nb::arg("y"),
            nb::arg("width"), nb::arg("height"), nb::arg("color"),
            "Add a RenderRectTask with color as a tuple (R, G, B[, A])")
        .def(
            "add_task_draw_rect",
            [](RenderQueue& self, int z_layer, const std::string& priority_group, int x, int y,
               int width, int height, int thickness, nb::tuple color) {
                // Default alpha to 255 (opaque) if not provided
                SDL_Color sdl_color = {0, 0, 0, 255};

                if (color.size() == 3) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                } else if (color.size() == 4) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                    sdl_color.a = nb::cast<uint8_t>(color[3]);
                } else {
                    throw std::invalid_argument(
                        "Color tuple must have 3 (RGB) or 4 (RGBA) elements");
                }

                self.add_task_draw_rect(z_layer, priority_group, x, y, width, height, thickness,
                                        sdl_color);
            },
            nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("x"), nb::arg("y"),
            nb::arg("width"), nb::arg("height"), nb::arg("thickness"), nb::arg("color"),
            "Add a RenderRectTask with color as a tuple (R, G, B[, A])")

        // Updated add_task_line binding to accept a tuple
        .def(
            "add_task_line",
            [](RenderQueue& self, int z_layer, const std::string& priority_group, int x1, int y1,
               int x2, int y2, nb::tuple color) {
                // Default alpha to 255 (opaque) if not provided
                SDL_Color sdl_color = {0, 0, 0, 255};

                if (color.size() == 3) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                } else if (color.size() == 4) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                    sdl_color.a = nb::cast<uint8_t>(color[3]);
                } else {
                    throw std::invalid_argument(
                        "Color tuple must have 3 (RGB) or 4 (RGBA) elements");
                }

                self.add_task_line(z_layer, priority_group, x1, y1, x2, y2, sdl_color);
            },
            nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("x1"), nb::arg("y1"),
            nb::arg("x2"), nb::arg("y2"), nb::arg("color"),
            "Add a RenderLineTask with color as a tuple (R, G, B[, A])")
        // Updated add_task_line binding to accept a tuple
        .def(
            "add_task_text",
            [](RenderQueue& self, int z_layer, const std::string& priority_group,
               const std::string& text, const std::string& font_id, nb::tuple color, int x, int y) {
                // Default alpha to 255 (opaque) if not provided
                SDL_Color sdl_color = {0, 0, 0, 255};

                if (color.size() == 3) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                } else if (color.size() == 4) {
                    sdl_color.r = nb::cast<uint8_t>(color[0]);
                    sdl_color.g = nb::cast<uint8_t>(color[1]);
                    sdl_color.b = nb::cast<uint8_t>(color[2]);
                    sdl_color.a = nb::cast<uint8_t>(color[3]);
                } else {
                    throw std::invalid_argument(
                        "Color tuple must have 3 (RGB) or 4 (RGBA) elements");
                }

                self.add_task_text(z_layer, priority_group, text, font_id, sdl_color, x, y);
            },
            nb::arg("z_layer"), nb::arg("priority_group"), nb::arg("x1"), nb::arg("y1"),
            nb::arg("x2"), nb::arg("y2"), nb::arg("color"),
            "Add a RenderLineTask with color as a tuple (R, G, B[, A])")
        .def("clear", &RenderQueue::clear)
        .def("get_sorted_layers", &RenderQueue::get_sorted_layers)
        .def("get_priority_order_value", &RenderQueue::get_priority_order_value)
        .def("get_sorted_priority_groups", &RenderQueue::get_sorted_priority_groups)
        .def("set_priority_order", &RenderQueue::set_priority_order)
        .def("render", &RenderQueue::render);

    // Bind the ItemConfiguration class
    nb::class_<ItemConfiguration>(m, "ItemConfiguration")
        .def(nb::init<const std::string&>(), nb::arg("item_id"))
        .def("set_in_game_textures", &ItemConfiguration::setInGameTextures, nb::arg("textures"))
        .def("set_inventory_textures", &ItemConfiguration::setInventoryTextures,
             nb::arg("textures"))
        .def("set_default_value", &ItemConfiguration::setDefaultValue, nb::arg("key"),
             nb::arg("value"))
        .def("get_item_id", &ItemConfiguration::getItemId)
        .def("get_in_game_textures", &ItemConfiguration::getInGameTextures)
        .def("get_inventory_textures", &ItemConfiguration::getInventoryTextures)
        .def(
            "get_default_value",
            [](const ItemConfiguration& self, const std::string& key) -> nb::object {
                const auto* value = self.getDefaultValue(key);
                if (!value) {
                    return nb::none();
                }
                return variant_to_pyobject(*value);
            },
            nb::arg("key"))
        .def("__del__", [](ItemConfiguration* self) { delete self; });
    ;

    m.def("register_item_configuration", &registerItemConfigurationOnManager, nb::arg("config"),
          "Register an ItemConfiguration and return a handle (uintptr_t).");
    m.def("get_item_configuration", &getItemConfigurationOnManager, nb::arg("item_id"),
          "Retrieve an ItemConfiguration by item_id.");
    m.def("deregister_item_configuration", &deregisterItemConfigurationOnManager,
          nb::arg("item_id"), "Deregister an ItemConfiguration by item_id.");

    // Expose the GridData struct to Python
    nb::class_<GridData>(m, "GridData")
        .def(nb::init<>())
        .def(nb::init<int, int, int, float>(), nb::arg("terrainID"), nb::arg("entityID"),
             nb::arg("eventID"), nb::arg("lightingLevel"))
        .def_rw("terrainID", &GridData::terrainID)
        .def_rw("entityID", &GridData::entityID)
        .def_rw("eventID", &GridData::eventID)
        .def_rw("lightingLevel", &GridData::lightingLevel);

    // Exposing EntityEnum
    nb::enum_<GridType>(m, "GridType")
        .value("TERRAIN", GridType::TERRAIN)
        .value("ENTITY", GridType::ENTITY)
        .export_values();

    nb::class_<World>(m, "World")
        .def(nb::init<int, int, int>())
        .def_rw("width", &World::width)
        .def_rw("height", &World::height)
        .def_rw("depth", &World::depth)
        .def_rw("game_clock", &World::gameClock)
        .def("initialize_voxel_grid", &World::initializeVoxelGrid)
        .def("set_voxel", &World::setVoxel)
        .def("get_voxel", &World::getVoxel)
        .def("create_entity", &World::createEntityFromPython)
        .def("remove_entity", &World::removeEntity)
        .def("get_entities_by_type", &World::getEntitiesByType)
        .def("get_entity_ids_by_type", &World::getEntityIdsByType)
        .def("get_entity_by_id", &World::getEntityById, "Retrieve an EntityInterface by entity ID")
        .def("create_perception_response", &World::createPerceptionResponse)
        .def("create_perception_responses", &World::createPerceptionResponses)
        .def("set_terrain", &World::setTerrain)
        .def("get_terrain", &World::getTerrain)
        .def("get_entity", &World::getEntity)
        .def("add_python_system", &World::addPythonSystem)
        .def("get_python_system", &World::getPythonSystem)
        .def("add_python_script", &World::addPythonScript)
        .def("run_python_script", &World::runPythonScript)
        .def("register_python_event_handler", &World::registerPythonEventHandler)
        .def("update", &World::update)
        .def("dispatch_move_entity_event_by_pos", &World::dispatchMoveSolidEntityEventByPosition,
             nb::sig("def dispatch_move_entity_event_by_pos(self, arg0: int, arg1: "
                     "list[DirectionEnum], /) -> None: ..."))
        .def("dispatch_move_entity_event_by_id", &World::dispatchMoveSolidEntityEventById,
             nb::sig("def dispatch_move_entity_event_by_id(self, arg0: int, arg1: "
                     "list[DirectionEnum], /) -> None: ..."))
        .def("dispatch_take_item_event_by_id", &World::dispatchTakeItemEventById)
        .def("dispatch_use_item_event_by_id", &World::dispatchUseItemEventById)
        .def("dispatch_set_entity_to_debug", &World::dispatchSetEntityToDebug)
        .def("get_ptr", &World::get_ptr)
        // Updated method bindings for GameDBHandler interface with automatic type conversion:
        .def("put_time_series",
             [](World& w, nb::object seriesName, nb::object timestamp, nb::object value) {
                 std::string name = nb::cast<std::string>(seriesName);
                 long long ts = nb::cast<long long>(timestamp);
                 double v = nb::cast<double>(value);
                 w.putTimeSeries(name, ts, v);
             })
        .def("query_time_series",
             [](World& w, nb::object seriesName, nb::object start,
                nb::object end) -> std::vector<std::pair<uint64_t, double>> {
                 std::string name = nb::cast<std::string>(seriesName);
                 long long s = nb::cast<long long>(start);
                 long long e = nb::cast<long long>(end);
                 return w.queryTimeSeries(name, s, e);
             })
        .def("execute_sql", [](World& w, nb::object sql) {
            std::string s = nb::cast<std::string>(sql);
            w.executeSQL(s);
        });

    // Exposing entt::entity as an unsigned integer to Python

    nb::class_<entt::entity>(m, "Entity")
        .def(nb::init<>())
        .def("__repr__",
             [](const entt::entity& e) {
                 return "<Entity " + std::to_string(static_cast<uint32_t>(e)) + ">";
             })
        .def(
            "get_id", [](const entt::entity& e) { return static_cast<uint32_t>(e); },
            "Get the raw ID of the entity");

    nb::class_<WorldView>(m, "WorldView")
        .def(nb::init<>())  // Expose the constructor

        .def_rw("width", &WorldView::width)
        .def_rw("height", &WorldView::height)
        .def_rw("depth", &WorldView::depth)
        .def_rw("voxelGridView", &WorldView::voxelGridView)
        .def_rw("entities", &WorldView::entities)

        .def("addEntity", &WorldView::addEntity, "Add an entity with a given ID", nb::arg("id"),
             nb::arg("entity"))
        .def("hasEntity", &WorldView::hasEntity, "Check if an entity exists by its ID",
             nb::arg("id"))
        .def("printAllEntityIds", &WorldView::printAllEntityIds,
             "Print all entity IDs for debugging")
        .def("serialize_flatbuffer", &WorldView::pySerializeFlatBuffers)
        .def_static("deserialize_flatbuffer", &WorldView::deserializeFlatBuffers)
        .def("get_entity_by_id", &WorldView::pyGetEntityById,
             "Retrieve an EntityInterface by entity ID", nb::rv_policy::reference_internal)
        .def("get_terrain_id", &WorldView::getTerrainId, "Retrieve an EntityInterface by coords",
             nb::rv_policy::reference_internal)
        .def("get_entity_id", &WorldView::getEntityId, "Retrieve an EntityInterface by coords",
             nb::rv_policy::reference_internal)
        .def("get_terrain", &WorldView::getTerrain, nb::rv_policy::reference_internal)
        .def("get_entity", &WorldView::getEntity, nb::rv_policy::reference_internal)
        .def("check_if_terrain_exist", &WorldView::checkIfTerrainExist)
        .def("check_if_entity_exist", &WorldView::checkIfEntityExist);

    nb::class_<WorldViewFlatB>(m, "WorldViewFlatB")
        .def(nb::init<nb::bytes>())
        .def("getWidth", &WorldViewFlatB::getWidth, nb::rv_policy::reference_internal)
        .def("getHeight", &WorldViewFlatB::getHeight, nb::rv_policy::reference_internal)
        .def("getDepth", &WorldViewFlatB::getDepth, nb::rv_policy::reference_internal)
        .def("getVoxelGrid", &WorldViewFlatB::getVoxelGrid, nb::rv_policy::reference_internal)
        .def("getEntityById", &WorldViewFlatB::getEntityById, nb::rv_policy::reference_internal)
        .def("get_terrain", &WorldViewFlatB::getTerrain, nb::rv_policy::reference_internal)
        .def("get_entity", &WorldViewFlatB::getEntity, nb::rv_policy::reference_internal);

    nb::class_<PerceptionResponse>(m, "PerceptionResponse")
        .def(nb::init<>())
        .def(nb::init<const EntityInterface&, const WorldView&>())
        .def("serialize_flatbuffer", &PerceptionResponse::pySerializeFlatBuffer)
        .def_rw("entity", &PerceptionResponse::entity, nb::rv_policy::reference_internal)
        .def_rw("world_view", &PerceptionResponse::world_view, nb::rv_policy::reference_internal);

    nb::class_<QueryResponse>(m, "QueryResponse").def("serialize", &QueryResponse::py_serialize);

    // Expose ListStringResponse derived from QueryResponse
    nb::class_<ListStringResponse, QueryResponse>(m, "ListStringResponse")
        .def(nb::init<>())
        .def(nb::init<const std::vector<std::string>&>())
        .def_rw("strings", &ListStringResponse::strings)
        .def("serialize", &ListStringResponse::py_serialize);

    // Expose MapOfMapsResponse derived from QueryResponse
    nb::class_<MapOfMapsResponse, QueryResponse>(m, "MapOfMapsResponse")
        .def(nb::init<>())
        .def(nb::init<const std::map<std::string, std::map<std::string, std::string>>&>())
        .def_rw("mapOfMaps", &MapOfMapsResponse::mapOfMaps)
        .def("serialize", &MapOfMapsResponse::py_serialize);

    // Expose MapOfMapsOfDoubleResponse derived from QueryResponse
    nb::class_<MapOfMapsOfDoubleResponse, QueryResponse>(m, "MapOfMapsOfDoubleResponse")
        .def(nb::init<>())
        .def(nb::init<const std::map<std::string, std::map<std::string, double>>&>())
        .def_rw("mapOfMaps", &MapOfMapsOfDoubleResponse::mapOfMaps)
        .def("serialize", &MapOfMapsOfDoubleResponse::py_serialize);

    nb::class_<PerceptionResponseFlatB>(m, "PerceptionResponseFlatB")
        .def(nb::init<nb::bytes>())
        .def("getWorldView", &PerceptionResponseFlatB::getWorldView,
             nb::rv_policy::reference_internal)
        .def("getEntity", &PerceptionResponseFlatB::getEntity, nb::rv_policy::reference_internal)
        .def("get_item_from_inventory_by_id", &PerceptionResponseFlatB::getItemFromInventoryById,
             nb::rv_policy::reference_internal)
        .def("get_query_response_by_id", &PerceptionResponseFlatB::getQueryResponseById,
             nb::rv_policy::reference_internal)
        .def("get_ticks", &PerceptionResponseFlatB::getTicks, nb::rv_policy::reference_internal);

    nb::class_<PhysicsStats>(m, "PhysicsStats")
        .def(nb::init<>())
        .def(nb::init<float>())
        .def("print", &PhysicsStats::print)
        .def_rw("mass", &PhysicsStats::mass)
        .def_rw("max_speed", &PhysicsStats::maxSpeed)
        .def_rw("min_speed", &PhysicsStats::minSpeed)
        .def_rw("force_x", &PhysicsStats::forceX)
        .def_rw("force_y", &PhysicsStats::forceY)
        .def_rw("force_z", &PhysicsStats::forceZ);

    nb::class_<PhysicsSettings>(m, "PhysicsSettings")
        .def(nb::init<>())
        .def("set_gravity", &PhysicsSettings::setGravity)
        .def("set_friction", &PhysicsSettings::setFriction)
        .def("set_allow_multi_direction", &PhysicsSettings::setAllowMultiDirection)
        .def("set_metabolism_cost_to_apply_force", &PhysicsSettings::setMetabolismCostToApplyForce)
        .def("set_evaporation_coefficient", &PhysicsSettings::setEvaporationCoefficient)
        .def("set_heat_to_water_evaporation", &PhysicsSettings::setHeatToWaterEvaporation)
        .def("set_water_minimum_units", &PhysicsSettings::setWaterMinimumUnits)
        .def("get_gravity", &PhysicsSettings::getGravity)
        .def("get_friction", &PhysicsSettings::getFriction)
        .def("get_allow_multi_direction", &PhysicsSettings::getAllowMultiDirection);

    nb::enum_<DirectionEnum>(m, "DirectionEnum")
        .value("UP", DirectionEnum::UP)
        .value("RIGHT", DirectionEnum::RIGHT)
        .value("DOWN", DirectionEnum::DOWN)
        .value("LEFT", DirectionEnum::LEFT)
        .value("UPWARD", DirectionEnum::UPWARD)
        .value("DOWNWARD", DirectionEnum::DOWNWARD)
        .export_values();
    nb::bind_vector<std::vector<DirectionEnum>>(m, "VecDirectionEnum");

    nb::enum_<MatterState>(m, "MatterState")
        .value("SOLID", MatterState::SOLID)
        .value("LIQUID", MatterState::LIQUID)
        .value("GAS", MatterState::GAS)
        .value("PLASMA", MatterState::PLASMA)
        .export_values();

    nb::class_<Position>(m, "Position")
        .def(nb::init<>())
        .def("get_direction_as_int", &Position::getDirectionAsInt)
        .def("print", &Position::print)
        .def("distance", &Position::distance)
        .def_rw("x", &Position::x)
        .def_rw("y", &Position::y)
        .def_rw("z", &Position::z)
        .def_rw("direction", &Position::direction);

    nb::class_<Velocity>(m, "Velocity")
        .def(nb::init<>())
        .def("print", &Velocity::print)
        .def("speed", &Velocity::speed)
        .def_rw("vx", &Velocity::vx)
        .def_rw("vy", &Velocity::vy)
        .def_rw("vz", &Velocity::vz);

    nb::class_<GradientVector>(m, "GradientVector")
        .def(nb::init<>())
        .def_rw("gx", &GradientVector::gx)
        .def_rw("gy", &GradientVector::gy)
        .def_rw("gz", &GradientVector::gz);

    nb::class_<StructuralIntegrityComponent>(m, "StructuralIntegrityComponent")
        .def(nb::init<>())
        .def_rw("can_stack_entities", &StructuralIntegrityComponent::canStackEntities)
        .def_rw("max_load_capacity", &StructuralIntegrityComponent::maxLoadCapacity)
        .def_rw("matter_state", &StructuralIntegrityComponent::matterState);

    nb::class_<MovingComponent>(m, "MovingComponent")
        .def(nb::init<>())
        .def_rw("is_moving", &MovingComponent::isMoving)
        .def_rw("moving_from_x", &MovingComponent::movingFromX)
        .def_rw("moving_from_y", &MovingComponent::movingFromY)
        .def_rw("moving_from_z", &MovingComponent::movingFromZ)
        .def_rw("moving_to_x", &MovingComponent::movingToX)
        .def_rw("moving_to_y", &MovingComponent::movingToY)
        .def_rw("moving_to_z", &MovingComponent::movingToZ)
        .def_rw("vx", &MovingComponent::vx)
        .def_rw("vy", &MovingComponent::vy)
        .def_rw("vz", &MovingComponent::vz)
        .def_rw("will_stop_x", &MovingComponent::willStopX)
        .def_rw("will_stop_y", &MovingComponent::willStopY)
        .def_rw("will_stop_z", &MovingComponent::willStopZ)
        .def_rw("completion_time", &MovingComponent::completionTime)
        .def_rw("time_remaining", &MovingComponent::timeRemaining)
        .def_rw("direction", &MovingComponent::direction);

    nb::class_<HealthComponent>(m, "HealthComponent")
        .def(nb::init<>())
        .def("print", &HealthComponent::print)
        .def_rw("health_level", &HealthComponent::healthLevel)
        .def_rw("max_health", &HealthComponent::maxHealth);

    nb::class_<PerceptionComponent>(m, "PerceptionComponent")
        .def(nb::init<>())  // Default constructor
        .def_rw("perception_area", &PerceptionComponent::perception_area)
        .def_rw("z_perception_area", &PerceptionComponent::z_perception_area)
        .def("get_perception_area", &PerceptionComponent::getPerceptionArea)
        .def("set_perception_area", &PerceptionComponent::setPerceptionArea)
        .def("get_z_perception_area", &PerceptionComponent::getZPerceptionArea)
        .def("set_z_perception_area", &PerceptionComponent::setZPerceptionArea)
        .def("print", &PerceptionComponent::print);

    // Exposing EntityEnum
    nb::enum_<EntityEnum>(m, "EntityEnum")
        .value("TERRAIN", EntityEnum::TERRAIN)
        .value("BEAST", EntityEnum::BEAST)
        .value("PLANT", EntityEnum::PLANT)
        .value("TILE_EFFECT", EntityEnum::TILE_EFFECT)
        .export_values();

    // Exposing TerrainEnum
    nb::enum_<TerrainEnum>(m, "TerrainEnum")
        .value("EMPTY", TerrainEnum::EMPTY)
        .value("GRASS", TerrainEnum::GRASS)
        .value("WATER", TerrainEnum::WATER)
        .export_values();

    nb::enum_<TerrainVariantEnum>(m, "TerrainVariantEnum")
        .value("FULL", TerrainVariantEnum::FULL)
        .value("RAMP_EAST", TerrainVariantEnum::RAMP_EAST)
        .value("RAMP_WEST", TerrainVariantEnum::RAMP_WEST)
        .value("CORNER_SOUTH_EAST", TerrainVariantEnum::CORNER_SOUTH_EAST)
        .value("CORNER_SOUTH_EAST_INV", TerrainVariantEnum::CORNER_SOUTH_EAST_INV)
        .value("CORNER_NORTH_EAST", TerrainVariantEnum::CORNER_NORTH_EAST)
        .value("CORNER_NORTH_EAST_INV", TerrainVariantEnum::CORNER_NORTH_EAST_INV)
        .value("RAMP_SOUTH", TerrainVariantEnum::RAMP_SOUTH)
        .value("RAMP_NORTH", TerrainVariantEnum::RAMP_NORTH)
        .value("CORNER_SOUTH_WEST", TerrainVariantEnum::CORNER_SOUTH_WEST)
        .value("CORNER_NORTH_WEST", TerrainVariantEnum::CORNER_NORTH_WEST)
        .export_values();

    nb::class_<EntityTypeComponent>(m, "EntityTypeComponent")
        .def(nb::init<>())
        .def_rw("main_type", &EntityTypeComponent::mainType)
        .def_rw("sub_type0", &EntityTypeComponent::subType0)
        .def_rw("sub_type1", &EntityTypeComponent::subType1)
        .def("print", &EntityTypeComponent::print);

    nb::enum_<ItemEnum>(m, "ItemEnum")
        .value("FOOD", ItemEnum::FOOD)
        .value("TOOL", ItemEnum::TOOL)
        .value("WEAPON", ItemEnum::WEAPON)
        .value("ARMOR", ItemEnum::ARMOR)
        .export_values();

    nb::enum_<ItemFoodEnum>(m, "ItemFoodEnum")
        .value("RASPBERRY_FRUIT", ItemFoodEnum::RASPBERRY_FRUIT)
        .value("RASPBERRY_LEAF", ItemFoodEnum::RASPBERRY_LEAF)
        .export_values();

    nb::enum_<ItemToolEnum>(m, "ItemToolEnum")
        .value("STONE_AXE", ItemToolEnum::STONE_AXE)
        .export_values();

    nb::class_<ItemTypeComponent>(m, "ItemTypeComponent")
        .def(nb::init<>())
        .def_rw("main_type", &ItemTypeComponent::mainType)
        .def_rw("sub_type0", &ItemTypeComponent::subType0)
        .def_rw("sub_type1", &ItemTypeComponent::subType1);

    nb::class_<Inventory>(m, "Inventory")
        .def(nb::init<>())
        .def_rw("max_items", &Inventory::maxItems)
        .def_ro("item_ids", &Inventory::itemIDs)
        .def("add_item", &Inventory::addItem, nb::arg("itemID"))
        .def("resize", &Inventory::resizeInventory, nb::arg("newSize"))
        .def("is_full", &Inventory::isFull)
        .def("is_empty", &Inventory::isEmpty)
        .def("pop_item", &Inventory::popItem)
        .def("remove_item_by_slot", &Inventory::removeItemBySlot, nb::arg("slotID"))
        .def("remove_item", &Inventory::removeItemById, nb::arg("itemID"));

    nb::class_<DropRates>(m, "DropRates")
        .def(nb::init<>())
        .def_rw("itemDropRates", &DropRates::itemDropRates)
        .def("add_item", &DropRates::addItem, nb::arg("itemID"), nb::arg("dropRate"),
             nb::arg("minDrop"), nb::arg("maxDrop"));

    nb::class_<FoodItem>(m, "FoodItem")
        .def(nb::init<>())
        .def_rw("energy_density", &FoodItem::energyDensity)
        .def_rw("mass", &FoodItem::mass)
        .def_rw("volume", &FoodItem::volume)
        .def_rw("convertion_efficiency", &FoodItem::convertionEfficiency)
        .def_rw("energy_health_ratio", &FoodItem::energyHealthRatio);

    nb::class_<WeaponAttributes>(m, "WeaponAttributes")
        .def(nb::init<>())
        .def_rw("damage", &WeaponAttributes::damage)
        .def_rw("defense", &WeaponAttributes::defense);

    nb::class_<Durability>(m, "Durability")
        .def(nb::init<>())
        .def_rw("current", &Durability::current)
        .def_rw("max", &Durability::max);

    nb::class_<MeeleAttackComponent>(m, "MeeleAttackComponent")
        .def(nb::init<>())
        .def_rw("weapon", &MeeleAttackComponent::weapon)
        .def_rw("hovered_entity", &MeeleAttackComponent::hoveredEntity)
        .def_rw("selected_entity", &MeeleAttackComponent::selectedEntity);

    nb::class_<MetabolismComponent>(m, "MetabolismComponent")
        .def(nb::init<>())
        .def_rw("energy_reserve", &MetabolismComponent::energyReserve)
        .def_rw("max_energy_reserve", &MetabolismComponent::maxEnergyReserve);

    nb::class_<DigestingFoodItem>(m, "DigestingFoodItem")
        .def(nb::init<>())
        .def_rw("food_item_id", &DigestingFoodItem::foodItemID)
        .def_rw("processing_time", &DigestingFoodItem::processingTime)
        .def_rw("energy_left", &DigestingFoodItem::energyLeft)
        .def_rw("energy_density", &DigestingFoodItem::energyDensity)
        .def_rw("mass", &DigestingFoodItem::mass)
        .def_rw("volume", &DigestingFoodItem::volume)
        .def_rw("convertion_efficiency", &DigestingFoodItem::convertionEfficiency)
        .def_rw("energy_health_ratio", &DigestingFoodItem::energyHealthRatio);

    nb::class_<DigestionComponent>(m, "DigestionComponent")
        .def(nb::init<>())
        .def_rw("size_of_stomach", &DigestionComponent::sizeOfStomach)
        .def_ro("digesting_items", &DigestionComponent::digestingItems)
        .def("add_item", &DigestionComponent::addItem)
        .def("remove_item", &DigestionComponent::removeItem);

    nb::class_<ConsoleLogsComponent>(m, "ConsoleLogsComponent")
        .def(nb::init<>())
        .def_rw("max_size", &ConsoleLogsComponent::max_size)
        .def_ro("log_buffer", &ConsoleLogsComponent::log_buffer)
        .def("add_log", &ConsoleLogsComponent::add_log);

    nb::class_<FruitGrowth>(m, "FruitGrowth")
        .def(nb::init<>())
        .def_rw("energy_needed", &FruitGrowth::energyNeeded)
        .def_rw("current_energy", &FruitGrowth::currentEnergy);

    nb::class_<MatterContainer>(m, "MatterContainer")
        .def(nb::init<>())
        .def_rw("terrain_matter", &MatterContainer::TerrainMatter)
        .def_rw("water_vapor", &MatterContainer::WaterVapor)
        .def_rw("water_matter", &MatterContainer::WaterMatter)
        .def_rw("bio_mass_matter", &MatterContainer::BioMassMatter);

    nb::enum_<TileEffectTypeEnum>(m, "TileEffectTypeEnum")
        .value("EMPTY", TileEffectTypeEnum::EMPTY)
        .value("BLOOD_DAMAGE", TileEffectTypeEnum::BLOOD_DAMAGE)
        .value("GREEN_DAMAGE", TileEffectTypeEnum::GREEN_DAMAGE)
        .export_values();

    nb::class_<TileEffectComponent>(m, "TileEffectComponent")
        .def(nb::init<>())
        .def_rw("tile_effect_type", &TileEffectComponent::tileEffectType)
        .def_rw("damage_value", &TileEffectComponent::damageValue)
        .def_rw("effect_total_time", &TileEffectComponent::effectTotalTime)
        .def_rw("effect_remaining_time", &TileEffectComponent::effectRemainingTime);

    nb::class_<TileEffectsList>(m, "TileEffectsList")
        .def(nb::init<>())
        .def_rw("tile_effects_ids", &TileEffectsList::tileEffectsIDs)
        .def("add_effect", &TileEffectsList::addEffect, nb::arg("TileEffectID"));

    nb::class_<ParentsComponent>(m, "ParentsComponent")
        .def(nb::init<>())
        .def_rw("parents", &ParentsComponent::parents);

    nb::enum_<ComponentFlag>(m, "ComponentFlag")
        .value("ENTITY_TYPE", ComponentFlag::ENTITY_TYPE)
        .value("MASS", ComponentFlag::MASS)
        .value("POSITION", ComponentFlag::POSITION)
        .value("VELOCITY", ComponentFlag::VELOCITY)
        .value("MOVING_COMPONENT", ComponentFlag::MOVING_COMPONENT)
        .value("HEALTH", ComponentFlag::HEALTH)
        .value("PERCEPTION", ComponentFlag::PERCEPTION)
        .value("COMPONENT_COUNT", ComponentFlag::COMPONENT_COUNT)
        .export_values();

    // Expose the EntityInterface class to Python
    nb::class_<EntityInterface>(m, "EntityInterface")
        .def(nb::init<>())
        // Entity ID accessors
        .def("get_entity_id", &EntityInterface::getEntityId)
        .def("set_entity_id", &EntityInterface::setEntityId)

        // Component accessors using lambdas
        .def("get_entity_type",
             [](EntityInterface& self) -> const EntityTypeComponent& {
                 return self.getComponent<EntityTypeComponent>();
             })
        .def("set_entity_type",
             [](EntityInterface& self, const EntityTypeComponent& value) {
                 self.setComponent<EntityTypeComponent>(value);
             })

        .def("get_position",
             [](EntityInterface& self) -> const Position& { return self.getComponent<Position>(); })
        .def("set_position", [](EntityInterface& self,
                                const Position& value) { self.setComponent<Position>(value); })

        .def("get_velocity",
             [](EntityInterface& self) -> const Velocity& { return self.getComponent<Velocity>(); })
        .def("set_velocity", [](EntityInterface& self,
                                const Velocity& value) { self.setComponent<Velocity>(value); })

        .def("get_moving_component",
             [](EntityInterface& self) -> const MovingComponent& {
                 return self.getComponent<MovingComponent>();
             })
        .def("set_moving_component",
             [](EntityInterface& self, const MovingComponent& value) {
                 self.setComponent<MovingComponent>(value);
             })

        .def("get_health",
             [](EntityInterface& self) -> const HealthComponent& {
                 return self.getComponent<HealthComponent>();
             })
        .def("set_health",
             [](EntityInterface& self, const HealthComponent& value) {
                 self.setComponent<HealthComponent>(value);
             })

        .def(
            "get_perception",
            [](EntityInterface& self) -> const PerceptionComponent& {
                return self.getComponent<PerceptionComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_perception",
             [](EntityInterface& self, const PerceptionComponent& value) {
                 self.setComponent<PerceptionComponent>(value);
             })

        .def(
            "get_inventory",
            [](EntityInterface& self) -> const Inventory& {
                return self.getComponent<Inventory>();
            },
            nb::rv_policy::reference_internal)
        .def("set_inventory", [](EntityInterface& self,
                                 const Inventory& value) { self.setComponent<Inventory>(value); })

        .def(
            "get_console_logs",
            [](EntityInterface& self) -> const ConsoleLogsComponent& {
                return self.getComponent<ConsoleLogsComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_console_logs",
             [](EntityInterface& self, const ConsoleLogsComponent& value) {
                 self.setComponent<ConsoleLogsComponent>(value);
             })

        .def(
            "get_matter_container",
            [](EntityInterface& self) -> const MatterContainer& {
                return self.getComponent<MatterContainer>();
            },
            nb::rv_policy::reference_internal)
        .def("set_matter_container",
             [](EntityInterface& self, const MatterContainer& value) {
                 self.setComponent<MatterContainer>(value);
             })

        .def(
            "get_item_enum",
            [](EntityInterface& self) -> const ItemEnum& { return self.getComponent<ItemEnum>(); },
            nb::rv_policy::reference_internal)
        .def("set_item_enum", [](EntityInterface& self,
                                 const ItemEnum& value) { self.setComponent<ItemEnum>(value); })
        .def(
            "get_item_type_comp",
            [](EntityInterface& self) -> const ItemTypeComponent& {
                return self.getComponent<ItemTypeComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_item_type_comp",
             [](EntityInterface& self, const ItemTypeComponent& value) {
                 self.setComponent<ItemTypeComponent>(value);
             })
        .def(
            "get_tile_effect_comp",
            [](EntityInterface& self) -> const TileEffectComponent& {
                return self.getComponent<TileEffectComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_tile_effect_comp",
             [](EntityInterface& self, const TileEffectComponent& value) {
                 self.setComponent<TileEffectComponent>(value);
             })

        .def(
            "get_tile_effects_list",
            [](EntityInterface& self) -> const TileEffectsList& {
                return self.getComponent<TileEffectsList>();
            },
            nb::rv_policy::reference_internal)
        .def("set_tile_effects_list",
             [](EntityInterface& self, const TileEffectsList& value) {
                 self.setComponent<TileEffectsList>(value);
             })

        .def(
            "get_food_item",
            [](EntityInterface& self) -> const FoodItem& { return self.getComponent<FoodItem>(); },
            nb::rv_policy::reference_internal)
        .def("set_food_item", [](EntityInterface& self,
                                 const FoodItem& value) { self.setComponent<FoodItem>(value); })

        .def(
            "get_metabolism",
            [](EntityInterface& self) -> const MetabolismComponent& {
                return self.getComponent<MetabolismComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_metabolism",
             [](EntityInterface& self, const MetabolismComponent& value) {
                 self.setComponent<MetabolismComponent>(value);
             })

        .def(
            "get_parents",
            [](EntityInterface& self) -> const ParentsComponent& {
                return self.getComponent<ParentsComponent>();
            },
            nb::rv_policy::reference_internal)
        .def("set_parents",
             [](EntityInterface& self, const ParentsComponent& value) {
                 self.setComponent<ParentsComponent>(value);
             })
        .def(
            "get_physics_stats",
            [](EntityInterface& self) -> const PhysicsStats& {
                return self.getComponent<PhysicsStats>();
            },
            nb::rv_policy::reference_internal)
        .def("set_physics_stats",
             [](EntityInterface& self, const PhysicsStats& value) {
                 self.setComponent<PhysicsStats>(value);
             })

        // has_component method
        .def(
            "has_component",
            [](const EntityInterface& self, int flag) {
                return self.hasComponent(static_cast<ComponentFlag>(flag));
            },
            nb::arg("flag"))

        // Serialization methods
        .def("serialize", &EntityInterface::py_serialize)
        .def_static("deserialize", &EntityInterface::py_deserialize);

    nb::bind_map<std::unordered_map<int, EntityInterface>>(m, "MapEntityInterface");

    nb::class_<VoxelGridView>(m, "VoxelGridView")
        .def(nb::init<>())
        .def_rw("width", &VoxelGridView::width)
        .def_rw("height", &VoxelGridView::height)
        .def_rw("depth", &VoxelGridView::depth)
        .def("initVoxelGridView", &VoxelGridView::initVoxelGridView)
        .def("set_terrain", &VoxelGridView::setTerrainVoxel)
        .def("get_terrain", &VoxelGridView::getTerrainVoxel)
        .def("set_entity", &VoxelGridView::setEntityVoxel)
        .def("get_entity", &VoxelGridView::getEntityVoxel)
        .def("serialize_flatbuffer", &VoxelGridView::serializeFlatBuffers)
        .def("deserialize_flatbuffer", &VoxelGridView::deserializeFlatBuffers);

    nb::class_<VoxelGridViewFlatB>(m, "VoxelGridViewFlatB")
        // Exposing the constructor
        .def(nb::init<nb::bytes>())

        // Exposing member methods
        .def("getWidth", &VoxelGridViewFlatB::getWidth)
        .def("getHeight", &VoxelGridViewFlatB::getHeight)
        .def("getDepth", &VoxelGridViewFlatB::getDepth)
        .def("getXOffset", &VoxelGridViewFlatB::getXOffset)
        .def("getYOffset", &VoxelGridViewFlatB::getYOffset)
        .def("getZOffset", &VoxelGridViewFlatB::getZOffset)
        .def("get_terrain", &VoxelGridViewFlatB::getTerrainVoxel)
        .def("get_entity", &VoxelGridViewFlatB::getEntityVoxel);

    // Bind VoxelGridCoordinates struct
    nb::class_<VoxelGridCoordinates>(m, "VoxelGridCoordinates")
        .def(nb::init<>())
        .def(nb::init<int, int, int>(), nb::arg("x"), nb::arg("y"), nb::arg("z"),
             "Initialize VoxelGridCoordinates with x, y, z")
        .def_ro("x", &VoxelGridCoordinates::x)
        .def_ro("y", &VoxelGridCoordinates::y)
        .def_ro("z", &VoxelGridCoordinates::z)
        .def("__repr__",
             [](const VoxelGridCoordinates& c) {
                 return "<VoxelGridCoordinates x=" + std::to_string(c.x) +
                        ", y=" + std::to_string(c.y) + ", z=" + std::to_string(c.z) + ">";
             })
        .def(nb::self == nb::self);
    nb::bind_vector<std::vector<VoxelGridCoordinates>>(m, "VecVoxelGridCoordinates");

    // Bind VoxelGrid class
    nb::class_<VoxelGrid>(m, "VoxelGrid")
        .def(nb::init<entt::registry&>())
        .def("initialize_grids", &VoxelGrid::initializeGrids)
        .def("set_voxel", &VoxelGrid::setVoxel, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             nb::arg("data"), "Set voxel data at (x, y, z)")
        .def("get_voxel", &VoxelGrid::getVoxel, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             "Get voxel data at (x, y, z)")
        .def("set_terrain", &VoxelGrid::setTerrain, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             nb::arg("terrainID"), "Set terrainID at (x, y, z)")
        .def("get_terrain", &VoxelGrid::getTerrain, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             "Get terrainID at (x, y, z)")
        .def("set_entity", &VoxelGrid::setEntity, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             nb::arg("entityID"), "Set entityID at (x, y, z)")
        .def("get_entity", &VoxelGrid::getEntity, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             "Get entityID at (x, y, z)")
        .def("set_event", &VoxelGrid::setEvent, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             nb::arg("eventID"), "Set eventID at (x, y, z)")
        .def("get_event", &VoxelGrid::getEvent, nb::arg("x"), nb::arg("y"), nb::arg("z"),
             "Get eventID at (x, y, z)")
        .def("set_lighting_level", &VoxelGrid::setLightingLevel, nb::arg("x"), nb::arg("y"),
             nb::arg("z"), nb::arg("lightingLevel"), "Set lightingLevel at (x, y, z)")
        .def("get_lighting_level", &VoxelGrid::getLightingLevel, nb::arg("x"), nb::arg("y"),
             nb::arg("z"), "Get lightingLevel at (x, y, z)")

        // Bind the new utility search methods
        .def("get_all_terrain_in_region", &VoxelGrid::getAllTerrainInRegion, nb::arg("x_min"),
             nb::arg("y_min"), nb::arg("z_min"), nb::arg("x_max"), nb::arg("y_max"),
             nb::arg("z_max"), "Retrieve all terrain voxel coordinates within a specified region.")
        .def("get_all_entity_in_region", &VoxelGrid::getAllEntityInRegion, nb::arg("x_min"),
             nb::arg("y_min"), nb::arg("z_min"), nb::arg("x_max"), nb::arg("y_max"),
             nb::arg("z_max"), "Retrieve all entity voxel coordinates within a specified region.")
        .def("get_all_event_in_region", &VoxelGrid::getAllEventInRegion, nb::arg("x_min"),
             nb::arg("y_min"), nb::arg("z_min"), nb::arg("x_max"), nb::arg("y_max"),
             nb::arg("z_max"), "Retrieve all event voxel coordinates within a specified region.")
        .def("get_all_lighting_in_region", &VoxelGrid::getAllLightingInRegion, nb::arg("x_min"),
             nb::arg("y_min"), nb::arg("z_min"), nb::arg("x_max"), nb::arg("y_max"),
             nb::arg("z_max"),
             "Retrieve all lighting voxel coordinates within a specified region.");

    // Binding the GameClock class
    nb::class_<GameClock>(m, "GameClock")
        .def(nb::init<>())
        .def(nb::init<uint64_t>())
        .def("tick", &GameClock::tick, "Advances the clock by one tick (one in-game second)")
        .def("get_ticks", &GameClock::getTicks, "Returns the total number of ticks elapsed")
        .def("set_ticks", &GameClock::setTicks, nb::arg("ticks"), "Sets the total number of ticks")
        .def("get_seconds", &GameClock::getSeconds,
             "Returns the total number of ticks (seconds) elapsed")
        .def("get_second", &GameClock::getSecond, "Returns the current second within the minute")
        .def("get_minute", &GameClock::getMinute, "Returns the current minute within the hour")
        .def("get_hour", &GameClock::getHour, "Returns the current hour within the day")
        .def("get_day", &GameClock::getDay, "Returns the current day within the month (1-28)")
        .def("get_month", &GameClock::getMonth,
             "Returns the current month (season) within the year (1-4)")
        .def("get_year", &GameClock::getYear, "Returns the current year")
        .def("get_season", &GameClock::getSeason, "Returns the current season as a string");

    // Binding the SunIntensity class
    nb::class_<SunIntensity>(m, "SunIntensity")
        .def_static("getIntensity", &SunIntensity::getIntensity, nb::arg("clock"),
                    "Returns the sun intensity as a value between 0.0 and 1.0");

    // Expose PyRegistry
    nb::class_<PyRegistry>(m, "PyRegistry")
        // Remove the constructor binding since PyRegistry cannot be constructed from Python
        .def("create_entity", &PyRegistry::create_entity)
        .def("destroy_entity", &PyRegistry::destroy_entity)
        .def("view", &PyRegistry::view)
        .def("has_all_components", &PyRegistry::has_all_components)
        .def("get_component", &PyRegistry::get_component, nb::rv_policy::copy)
        .def("set_component", &PyRegistry::set_component)
        .def("remove_component", &PyRegistry::remove_component)
        .def("is_valid", &PyRegistry::is_valid);

    m.def("get_and_draw_selected_entity", &getAndDrawSelectedEntity);
    m.def("draw_tile_effects", &drawTileEffects);
    m.def("should_draw_terrain", &shouldDrawTerrain);
    m.def("is_terrain_an_empty_water", &isTerrainAnEmptyWater);
    m.def("is_occluding_entity_perspective", &isOccludingEntityPerspective);
    m.def("is_occluding_some_entity", &isOccludingSomeEntity);

    nb::class_<GenomeParams>(m, "GenomeParams")
        .def(nb::init<>())
        .def_rw("num_inputs", &GenomeParams::num_inputs)
        .def_rw("num_outputs", &GenomeParams::num_outputs)
        .def_rw("num_hidden", &GenomeParams::num_hidden)
        .def_rw("feed_forward", &GenomeParams::feed_forward)
        .def_rw("compatibility_disjoint_coefficient",
                &GenomeParams::compatibility_disjoint_coefficient)
        .def_rw("compatibility_weight_coefficient", &GenomeParams::compatibility_weight_coefficient)
        .def_rw("conn_add_prob", &GenomeParams::conn_add_prob)
        .def_rw("conn_delete_prob", &GenomeParams::conn_delete_prob)
        .def_rw("node_add_prob", &GenomeParams::node_add_prob)
        .def_rw("node_delete_prob", &GenomeParams::node_delete_prob)
        .def_rw("single_structural_mutation", &GenomeParams::single_structural_mutation)
        .def_rw("structural_mutation_surer", &GenomeParams::structural_mutation_surer)
        .def_rw("initial_connection", &GenomeParams::initial_connection);

    nb::class_<DefaultGenomeConfig>(m, "DefaultGenomeConfig")
        .def(nb::init<const GenomeParams&>())
        .def_rw("num_inputs", &DefaultGenomeConfig::num_inputs)
        .def_rw("num_outputs", &DefaultGenomeConfig::num_outputs)
        .def_rw("num_hidden", &DefaultGenomeConfig::num_hidden)
        .def_rw("feed_forward", &DefaultGenomeConfig::feed_forward)
        .def_rw("compatibility_disjoint_coefficient",
                &DefaultGenomeConfig::compatibility_disjoint_coefficient)
        .def_rw("compatibility_weight_coefficient",
                &DefaultGenomeConfig::compatibility_weight_coefficient)
        .def_rw("conn_add_prob", &DefaultGenomeConfig::conn_add_prob)
        .def_rw("conn_delete_prob", &DefaultGenomeConfig::conn_delete_prob)
        .def_rw("node_add_prob", &DefaultGenomeConfig::node_add_prob)
        .def_rw("node_delete_prob", &DefaultGenomeConfig::node_delete_prob)
        .def_rw("single_structural_mutation", &DefaultGenomeConfig::single_structural_mutation)
        .def_rw("structural_mutation_surer", &DefaultGenomeConfig::structural_mutation_surer)
        .def_rw("initial_connection", &DefaultGenomeConfig::initial_connection)
        .def_rw("connection_fraction", &DefaultGenomeConfig::connection_fraction)
        .def_rw("input_keys", &DefaultGenomeConfig::input_keys)
        .def_rw("output_keys", &DefaultGenomeConfig::output_keys)
        .def("get_new_node_key", &DefaultGenomeConfig::get_new_node_key);

    // nb::class_<DefaultNodeGene>(m, "DefaultNodeGene")
    //     .def(nb::init<>())
    //     .def_rw("key", &DefaultNodeGene::key)
    //     .def("copy", &DefaultNodeGene::copy)
    //     .def("distance", &DefaultNodeGene::distance);
    //     // .def("mutate", &DefaultNodeGene::mutate);

    // nb::class_<DefaultConnectionGene>(m, "DefaultConnectionGene")
    //     .def(nb::init<>())
    //     .def_rw("weight", &DefaultConnectionGene::weight)
    //     .def_rw("enabled", &DefaultConnectionGene::enabled)
    //     .def("copy", &DefaultConnectionGene::copy)
    //     .def("distance", &DefaultConnectionGene::distance);
    //     // .def("mutate", &DefaultConnectionGene::mutate)
    //     // .def("crossover", &DefaultConnectionGene::crossover);

    // nb::class_<DefaultGenome>(m, "DefaultGenome")
    //     .def(nb::init<>())
    //     .def_rw("key", &DefaultGenome::key)
    //     .def_rw("fitness", &DefaultGenome::fitness)
    //     .def_rw("nodes", &DefaultGenome::nodes)
    //     .def_rw("connections", &DefaultGenome::connections)
    //     .def("configure_new", &DefaultGenome::configure_new)
    //     .def("mutate_add_node", &DefaultGenome::mutate_add_node);

    m.def("get_pruned_copy", &get_pruned_copy, "Get a pruned copy of the genome");
}