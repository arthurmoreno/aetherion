// DrawTerrain.cpp — see DrawTerrain.hpp for the contract.
//
// Plan:
// .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md

#include "Camera/DrawTerrain.hpp"

#include <string>
#include <vector>

#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

#include "CameraUtils.hpp"
#include "EntityInterface.hpp"
#include "LowLevelRenderer/RenderQueue.hpp"
#include "WorldView.hpp"
#include "components/EntityTypeComponent.hpp"
#include "components/HealthComponents.hpp"
#include "components/ItemsComponents.hpp"
#include "components/PhysicsComponents.hpp"
#include "components/TerrainComponents.hpp"

namespace nb = nanobind;

namespace aetherion::render {

namespace {

// Port of `aetherion/src/aetherion/camera/terrain.py:is_occluding_some_entity`.
// The Python function checks specific tile offsets around the occluding
// entity (direct + SE diagonal at same z, direct + diagonal at z-1).
// This is intentionally a *different* function from
// `aetherion::isOccludingSomeEntity` in CameraUtils — the Python local
// version has terrain-specific offsets that the generic engine helper
// does not.
bool is_occluding_some_entity_local(WorldView &world_view,
                                    EntityInterface &occluding_entity) {
  const Position &pos = occluding_entity.getComponent<Position>();
  const int x = pos.x;
  const int y = pos.y;
  const int z = pos.z;
  const EntityTypeComponent &type =
      occluding_entity.getComponent<EntityTypeComponent>();

  if (type.mainType == static_cast<int>(EntityEnum::TERRAIN)) {
    if (world_view.checkIfEntityExist(x, y, z))
      return true;
    if (world_view.checkIfEntityExist(x - 1, y - 1, z))
      return true;
    if (world_view.checkIfEntityExist(x - 1, y, z))
      return true;
    if (world_view.checkIfEntityExist(x - 1, y + 1, z))
      return true;
  }

  if (world_view.checkIfEntityExist(x, y, z - 1))
    return true;
  if (world_view.checkIfEntityExist(x - 1, y - 1, z - 1))
    return true;

  return false;
}

// Helper for selecting a water terrain's sprite variation key based on
// the current matter/vapor levels. Mirrors lines 36–48 of terrain.py.
const char *water_terrain_variation_key(int water_matter, int water_vapor) {
  if (water_matter > 0 && water_matter <= 4)
    return "level-1";
  if (water_matter > 4 && water_matter <= 12)
    return "level-2";
  if (water_vapor > 0 && water_vapor <= 4)
    return "vapor-level-1";
  if (water_vapor > 4 && water_vapor <= 12)
    return "vapor-level-2";
  if (water_vapor > 12)
    return "cloud-1";
  return "full";
}

} // namespace

DrawTerrainContext build_draw_terrain_context(
    nb::object camera, const std::string &terrain_group_0,
    const std::string &terrain_group_1, const std::string &effect_group,
    const std::string &gui_group, int tile_size_on_screen) {
  DrawTerrainContext tctx;
  tctx.views = camera.attr("views");

  // views["items"] may be missing entirely; use `dict.get("items", None)`
  // — `nb::object` does not expose `.contains()` (that lives on
  // `nb::dict`), and `.get()` returns None for missing keys.
  tctx.items_views = tctx.views.attr("get")(nb::str("items"));

  // TERRAIN_VARIATION_TYPE_MAP — Python dict at
  // `aetherion.world.constants.TERRAIN_VARIATION_TYPE_MAP`.
  nb::object world_constants =
      nb::module_::import_("aetherion.world.constants");
  tctx.terrain_variation_map =
      world_constants.attr("TERRAIN_VARIATION_TYPE_MAP");

  // Classification ctor — same one the walker caches in WalkerContext.
  // Resolved here independently so this context is self-sufficient.
  nb::object entities_base = nb::module_::import_("aetherion.entities.base");
  tctx.classification_ctor = entities_base.attr("Classification");

  tctx.terrain_group_0 = terrain_group_0;
  tctx.terrain_group_1 = terrain_group_1;
  tctx.effect_group = effect_group;
  tctx.gui_group = gui_group;
  tctx.tile_size_on_screen = tile_size_on_screen;

  return tctx;
}

bool draw_terrain_native(const DrawTerrainContext &tctx,
                         EntityInterface &terrain, nb::object terrain_obj,
                         nb::object view_object, nb::object camera_settings,
                         nb::object selected_entity, WorldView &world_view,
                         nb::object world_view_obj, nb::object mouse_state,
                         int screen_x, int screen_y, RenderQueue &render_queue,
                         nb::object render_queue_obj, int layer_index,
                         float initial_light_intensity, float sun_light,
                         nb::object player, nb::object gradient_view_object,
                         bool terrain_gradient_camera_stats,
                         nb::object water_view, bool water_camera_stats,
                         nb::object entity_hovered) {
  // ─── Sprite variation lookup (lines 164–169 of terrain.py) ─────────
  const EntityTypeComponent &terrain_type =
      terrain.getComponent<EntityTypeComponent>();
  const int sub_type0 = terrain_type.subType0;
  const int sub_type1 = terrain_type.subType1;

  if (sub_type0 == static_cast<int>(TerrainEnum::GRASS)) {
    // set_grass_terrain_view_sprite: looks up
    // TERRAIN_VARIATION_TYPE_MAP.get(sub_type1, "full") then
    // view_object.set_terrain_variation_sprite(...)
    nb::object variation =
        tctx.terrain_variation_map.attr("get")(sub_type1, nb::str("full"));
    view_object.attr("set_terrain_variation_sprite")(variation);
  } else if (sub_type0 == static_cast<int>(TerrainEnum::WATER)) {
    // set_water_terrain_view_sprite: native level-based decision +
    // one Python `set_terrain_variation_sprite` call.
    const MatterContainer &matter = terrain.getComponent<MatterContainer>();
    view_object.attr("set_terrain_variation_sprite")(nb::str(
        water_terrain_variation_key(matter.WaterMatter, matter.WaterVapor)));
  } else if (sub_type0 == static_cast<int>(TerrainEnum::EMPTY)) {
    view_object.attr("set_terrain_variation_sprite")(nb::str("full"));
  }

  // ─── Selected-entity highlight (lines 171–184) ─────────────────────
  bool current_entity_hovered = false;
  bool should_check_hover = entity_hovered.is_none();
  if (!should_check_hover) {
    // The Python condition is `or (terrain is not None and
    // terrain.get_entity_id() == selected_entity)`. terrain is never
    // None in this function (the caller already guarded). Read
    // entity-id natively.
    const int terrain_id = terrain.getEntityId();
    if (!selected_entity.is_none()) {
      const int sel_id = nb::cast<int>(selected_entity);
      should_check_hover = (terrain_id == sel_id);
    }
  }
  if (should_check_hover) {
    // The Python wrapper `aetherion.camera.mouse.get_and_draw_selected_entity`
    // is the right entry point — it dispatches to either the pure-Python
    // implementation or the C++ binding depending on its
    // `TYPE_OF_MOUSE_PROCESSOR` switch (default "python"). Calling the
    // bound C++ helper directly would skip that dispatch and use a
    // different 10-arg shape (int selected_entity_id + int tile_size_on_screen
    // instead of selected_entity + camera_settings). One nb-dispatch
    // crossing per non-empty terrain tile that needs the hover check.
    // Phase 6 candidate: refactor `getAndDrawSelectedEntity` to take
    // references so this can also be called natively.
    nb::object mouse_module = nb::module_::import_("aetherion.camera.mouse");
    nb::object get_and_draw = mouse_module.attr("get_and_draw_selected_entity");
    nb::object hover_result = get_and_draw(
        world_view_obj, terrain_obj, mouse_state, screen_x, screen_y,
        render_queue_obj, layer_index, nb::str(tctx.terrain_group_0.c_str()),
        selected_entity, camera_settings);
    current_entity_hovered = nb::cast<bool>(hover_result);
  }

  // ─── Light intensity blend (lines 186–190) ─────────────────────────
  float light_intensity = initial_light_intensity;
  if (current_entity_hovered) {
    light_intensity = std::max(light_intensity - 0.2f, 0.0f);
  }
  light_intensity += sun_light;

  // ─── Occlusion / opacity (lines 192–196) ───────────────────────────
  float opacity = 1.0f;
  EntityInterface &player_ref = nb::cast<EntityInterface &>(player);
  const bool is_occluding_player_perspective =
      isOccludingEntityPerspective(player_ref, world_view, terrain);
  if (is_occluding_player_perspective) {
    opacity = 0.4f;
  }

  // ─── Primary terrain sprite task (lines 198–206) ───────────────────
  nb::object sprite_id = view_object.attr("sprite").attr("sprite_id");
  // `sprite_id` is a Python string; the RenderQueue C++ API expects
  // `const std::string&`, so we cast once.
  const std::string sprite_id_str = nb::cast<std::string>(sprite_id);
  render_queue.add_task_by_id(layer_index, tctx.terrain_group_0, sprite_id_str,
                              screen_x, screen_y, light_intensity, opacity);

  // ─── Quadrant overlay for occluded tiles (lines 207–217) ───────────
  if (is_occluding_player_perspective ||
      is_occluding_some_entity_local(world_view, terrain)) {
    // TextureQuadrant.TOP_LEFT.value is the int 0; pass directly.
    render_queue.add_task_by_id_quadrant(
        layer_index, tctx.terrain_group_1, sprite_id_str, screen_x, screen_y,
        light_intensity, opacity, RenderQueue::TextureQuadrant::TOP_LEFT);
  }

  // ─── Water overlay on empty terrain (lines 219–233) ────────────────
  if (!water_view.is_none() && sub_type0 == 0) {
    const MatterContainer &matter = terrain.getComponent<MatterContainer>();
    if (matter.WaterMatter > 0 || matter.WaterVapor > 0) {
      water_view.attr("set_terrain_variation_sprite")(nb::str("full"));
      const std::string water_sprite_id =
          nb::cast<std::string>(water_view.attr("sprite").attr("sprite_id"));
      render_queue.add_task_by_id(layer_index, tctx.terrain_group_0,
                                  water_sprite_id, screen_x, screen_y,
                                  light_intensity, opacity);
    }
  }

  // ─── Gradient vector overlay (lines 235–246 → draw_gradient_vector)
  if (terrain_gradient_camera_stats && sub_type0 == 0 &&
      !gradient_view_object.is_none()) {
    const Position &pos = terrain.getComponent<Position>();
    const int direction = static_cast<int>(pos.direction);
    bool should_draw_gradient = false;
    if (direction == static_cast<int>(DirectionEnum::DOWN) ||
        direction == static_cast<int>(DirectionEnum::UP) ||
        direction == static_cast<int>(DirectionEnum::LEFT) ||
        direction == static_cast<int>(DirectionEnum::RIGHT)) {
      gradient_view_object.attr("set_sprite")(direction);
      should_draw_gradient = true;
    }
    if (should_draw_gradient) {
      const std::string grad_sprite_id = nb::cast<std::string>(
          gradient_view_object.attr("sprite").attr("sprite_id"));
      render_queue.add_task_by_id(layer_index, tctx.effect_group,
                                  grad_sprite_id, screen_x, screen_y,
                                  light_intensity, opacity);
    }
  }

  // ─── Inventory item rendering (lines 248–267) ──────────────────────
  if (terrain.hasComponent(ComponentFlag::INVENTORY) &&
      !tctx.items_views.is_none()) {
    const Inventory &inventory = terrain.getComponent<Inventory>();
    if (!inventory.itemIDs.empty()) {
      for (const int item_id : inventory.itemIDs) {
        EntityInterface *item = world_view.getEntityById(item_id);
        if (item == nullptr)
          continue;

        // Classification ctor + dict lookup chain — Python.
        // ItemTypeComponent struct field names are (mainType, subType0).
        const ItemTypeComponent &itc = item->getComponent<ItemTypeComponent>();
        nb::object item_enum_id =
            tctx.classification_ctor(itc.mainType, itc.subType0);
        nb::object item_view_object =
            tctx.items_views.attr("get")(item_enum_id);
        if (item_view_object.is_none())
          continue;

        item_view_object.attr("set_sprite")(nb::str("in_game_texture"));
        const std::string item_sprite_id = nb::cast<std::string>(
            item_view_object.attr("sprite").attr("sprite_id"));
        render_queue.add_task_by_id(
            layer_index, tctx.terrain_group_0, item_sprite_id,
            screen_x - tctx.tile_size_on_screen,
            screen_y - tctx.tile_size_on_screen, light_intensity, opacity);
      }
    }
  }

  // Font id None → skip text emission.
  if (water_camera_stats) {
    nb::object font_id_obj = camera_settings.attr("stats_overlay_font_id");
    if (!font_id_obj.is_none()) {
      const std::string font_id = nb::cast<std::string>(font_id_obj);
      const MatterContainer &matter = terrain.getComponent<MatterContainer>();
      static const SDL_Color sdl_color = {255, 255, 255, 255};
      if (matter.WaterMatter > 0 || sub_type0 == 1) {
        render_queue.add_task_text(layer_index, tctx.gui_group,
                                   std::to_string(matter.WaterMatter), font_id,
                                   sdl_color, screen_x, screen_y);
      }
      if (matter.WaterVapor > 0 || sub_type0 == 1) {
        render_queue.add_task_text(layer_index, tctx.gui_group,
                                   std::to_string(matter.WaterVapor), font_id,
                                   sdl_color, screen_x + 20, screen_y + 20);
      }
    }
  }

  return current_entity_hovered;
}

} // namespace aetherion::render
