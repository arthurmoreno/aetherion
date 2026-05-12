// Native C++ port of `aetherion/src/aetherion/camera/terrain.py:draw_terrain`.
//
// Used by the dimetric tile walker as the default terrain rendering path
// when no `terrain_handler` callback is registered on the `Camera` (i.e.
// when `Camera(..., terrain_handler=None)`). Consumers that pass a custom
// `terrain_handler` Python callable bypass this entirely.
//
// Most of the per-tile work runs natively: component reads
// (`EntityTypeComponent`, `MatterContainer`, `Inventory`),
// occlusion checks, render-queue task emission, inventory iteration,
// water-stats text overlay, gradient arrow dispatch. The remaining
// Python crossings are limited to the `BaseView` method calls
// (`set_terrain_variation_sprite`, `set_sprite`), the
// `TERRAIN_VARIATION_TYPE_MAP.get` dict lookup, and the
// `views["items"].get(Classification(...))` chain inside the inventory
// loop — these depend on consumer-supplied Python view objects and
// can't be moved native without breaking the consumer flexibility
// contract.
//
// Plan:
// .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md

#pragma once

#include <string>

#include <nanobind/nanobind.h>

class EntityInterface;
class WorldView;
class RenderQueue;

namespace aetherion::render {

// Cached loop-invariants needed by `draw_terrain_native`. Pre-resolved
// once per `dimetric_tile_walker` call inside `build_context` so the
// per-tile path does not redo any attr lookups or type imports.
struct DrawTerrainContext {
  nanobind::object views;       // Python dict — views["items"] lookup
  nanobind::object items_views; // = views["items"] (or nb::none())
  nanobind::object
      terrain_variation_map; // aetherion.world.constants.TERRAIN_VARIATION_TYPE_MAP
  nanobind::object
      classification_ctor; // aetherion.entities.base.Classification

  std::string terrain_group_0;
  std::string terrain_group_1;
  std::string effect_group;
  std::string gui_group;

  int tile_size_on_screen;
};

// Build the per-walker-call helper context for the native terrain
// handler. Cheap; resolved once at walker entry.
DrawTerrainContext build_draw_terrain_context(
    nanobind::object camera, const std::string &terrain_group_0,
    const std::string &terrain_group_1, const std::string &effect_group,
    const std::string &gui_group, int tile_size_on_screen);

// Native terrain handler. Returns `true` if this tile became the new
// hovered entity (caller should then set `entity_hovered = terrain`),
// `false` otherwise. Matches the truthy semantics of the Python
// `default_terrain_handler`.
bool draw_terrain_native(
    const DrawTerrainContext &tctx, EntityInterface &terrain,
    nanobind::object terrain_obj, nanobind::object view_object,
    nanobind::object camera_settings, nanobind::object selected_entity,
    WorldView &world_view, nanobind::object world_view_obj,
    nanobind::object mouse_state, int screen_x, int screen_y,
    RenderQueue &render_queue, nanobind::object render_queue_obj,
    int layer_index, float initial_light_intensity, float sun_light,
    nanobind::object player, nanobind::object gradient_view_object,
    bool terrain_gradient_camera_stats, nanobind::object water_view,
    bool water_camera_stats, nanobind::object entity_hovered);

} // namespace aetherion::render
