// DimetricTileWalker — implementation.
//
// Plan:
// .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md

#include "Camera/DimetricTileWalker.hpp"

#include <optional>
#include <string>

#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

// CameraUtils.hpp brings in `EntityInterface`, the component headers
// (EntityTypeComponent / HealthComponent / etc.), `WorldView`, and the
// free C++ helpers `shouldDrawTerrain` / `isTerrainAnEmptyWater` /
// `drawTileEffects`. The walker calls those helpers and reads component
// fields directly — no per-call nanobind crossing into Python for any of
// it.
#include "Camera/DrawTerrain.hpp"
#include "CameraUtils.hpp"
#include "LowLevelRenderer/RenderQueue.hpp"

namespace nb = nanobind;

namespace aetherion::render {

namespace {

// Resolve and cache the loop-invariants we need from the Python `Camera`
// instance so the hot loop body only does pointer arithmetic and direct
// C++ calls on the `WorldView`. Anything that depends on per-tile state
// (terrain object, classifications, screen coords) is resolved inside the
// loop.
struct WalkerContext {
  // Cached references / dicts pulled off the Python Camera once.
  //
  // `should_draw_terrain` / `is_terrain_an_empty_water` / `draw_tile_effects`
  // are NOT cached as `nb::object` callables — they're called directly as
  // C++ free functions (see `CameraUtils.hpp`). Same for `hasComponent` and
  // component-getter access on entities, which the walker invokes via the
  // C++ `EntityInterface` API rather than `entity.attr(...)`.
  nb::object aetherion_module;
  nb::object terrain_handler;          // may be `None` → use native default
  bool terrain_handler_is_none;        // cached `terrain_handler.is_none()`
  DrawTerrainContext draw_terrain_ctx; // populated when handler is None
  nb::object beast_handler;
  nb::object plant_handler;
  nb::object camera_model;
  nb::object settings;
  nb::object render_queue;         // nb::object form, needed by the lifebar
                                   // Python view's set_position_draw call
  RenderQueue *render_queue_ref;   // native pointer for drawTileEffects
                                   // (and any future direct C++ render
                                   // queue calls — the lifebar still
                                   // needs the nb::object form above)
  std::string gui_group_str;       // native form for drawTileEffects
  nb::object gui_group;            // nb::object form for lifebar kwargs
  nb::object terrain_views;        // Python dict — view_object lookup
  nb::object entity_views;         // Python dict — view_object lookup
  nb::object water_view;           // may be nb::none()
  nb::object gradient_view_object; // may be nb::none()
  nb::object lifebar_view;         // may be nb::none()
  nb::object classification_ctor;  // aetherion.entities.base.Classification
  int tile_size;
  int beast_enum_value; // cached int for the dispatch comparison
  bool empty_tile_debugging;
};

WalkerContext build_context(nb::object camera) {
  WalkerContext ctx;
  ctx.aetherion_module = nb::module_::import_("aetherion");

  // The three consumer-provided handler callables, plus the camera
  // model / settings / render-queue references the handlers need to be
  // invoked with. `_terrain_handler` is allowed to be `None`, in which
  // case the walker dispatches to the native C++ `draw_terrain_native`
  // instead of crossing into Python — see `Camera/DrawTerrain.hpp` and
  // the terrain branch below.
  ctx.terrain_handler = camera.attr("_terrain_handler");
  ctx.terrain_handler_is_none = ctx.terrain_handler.is_none();
  ctx.beast_handler = camera.attr("_beast_entity_handler");
  ctx.plant_handler = camera.attr("_plant_entity_handler");
  ctx.camera_model = camera.attr("_camera_model");
  ctx.settings = camera.attr("settings");
  ctx.render_queue = ctx.camera_model.attr("render_queue");
  ctx.gui_group = ctx.camera_model.attr("gui_group");

  // Native references for the C++-direct calls. The walker calls
  // `drawTileEffects(terrain_ref, world_view_ref, *render_queue_ref, …)`
  // per non-empty terrain tile, bypassing the nanobind dispatch layer
  // that the equivalent `aetherion.draw_tile_effects(…)` Python call
  // would go through.
  //
  // The cast is wrapped in try/catch because parity-test fixtures
  // substitute a Python `RecordingRenderQueue` stub for the real
  // `aetherion.RenderQueue` (the bundled font isn't reachable in test
  // env), and `nb::cast<RenderQueue&>` on a non-bound type throws
  // `std::bad_cast`. When the cast fails, `render_queue_ref` stays
  // `nullptr` and the terrain branch skips `drawTileEffects` — the
  // test world has no terrain anyway, so this preserves parity-test
  // semantics without forcing the test to construct a real
  // `RenderQueue`. In production the cast always succeeds.
  try {
    ctx.render_queue_ref = &nb::cast<RenderQueue &>(ctx.render_queue);
  } catch (const std::exception &) {
    ctx.render_queue_ref = nullptr;
  }
  ctx.gui_group_str = nb::cast<std::string>(ctx.gui_group);

  nb::object views = camera.attr("views");
  ctx.terrain_views = views.attr("__getitem__")(nb::str("terrains"));
  ctx.entity_views = views.attr("__getitem__")(nb::str("entities"));

  // Lookup of WATER terrain view + gradient_arrow camera-ui view + lifebar
  // camera-ui view mirrors lines 365–375 + 282–285 of the Python
  // `draw_player_perspective_layer` / `non_terrain_entity_handler`. All
  // three are loop-invariant; resolve once. We use `dict.get(key)` (which
  // returns None for missing keys) rather than `__contains__` +
  // `__getitem__` — `nb::object` does not expose `.contains()` directly
  // (that lives on `nb::dict`), and `.get()` is enough because the views
  // dict does not store explicit `None` values for present keys.
  nb::object terrain_enum_water =
      ctx.aetherion_module.attr("TerrainEnum_WATER");
  ctx.water_view = ctx.terrain_views.attr("get")(terrain_enum_water);
  nb::object camera_ui_views = views.attr("__getitem__")(nb::str("camera-ui"));
  ctx.gradient_view_object =
      camera_ui_views.attr("get")(nb::str("gradient_arrow"));
  ctx.lifebar_view = camera_ui_views.attr("get")(nb::str("lifebar"));

  // Classification ctor + beast-enum int are needed per non-empty entity
  // tile. The `MOVING_COMPONENT` component-flag check is *not* cached
  // here — the walker now calls `entity_ref.hasComponent(
  // ComponentFlag::MOVING_COMPONENT)` directly in C++, so no Python
  // attr lookup needed for the flag value.
  nb::object entities_base = nb::module_::import_("aetherion.entities.base");
  ctx.classification_ctor = entities_base.attr("Classification");
  ctx.beast_enum_value =
      nb::cast<int>(ctx.aetherion_module.attr("EntityEnum_BEAST"));

  ctx.tile_size = nb::cast<int>(ctx.settings.attr("tile_size_on_screen"));
  ctx.empty_tile_debugging =
      nb::cast<bool>(ctx.settings.attr("empty_tile_debugging"));

  // Native draw-terrain context. Only populate when the consumer leaves
  // `terrain_handler=None` — that's when the walker dispatches to the
  // C++ native handler instead of crossing into Python. Group names are
  // read off the camera_model so they always reflect the Camera's bound
  // config (set in `_build_camera_model`).
  if (ctx.terrain_handler_is_none) {
    const std::string terrain_group_0 =
        nb::cast<std::string>(ctx.camera_model.attr("terrain_group_0"));
    const std::string terrain_group_1 =
        nb::cast<std::string>(ctx.camera_model.attr("terrain_group_1"));
    const std::string effect_group =
        nb::cast<std::string>(ctx.camera_model.attr("effect_group"));
    ctx.draw_terrain_ctx = build_draw_terrain_context(
        camera, terrain_group_0, terrain_group_1, effect_group,
        ctx.gui_group_str, ctx.tile_size);
  }

  return ctx;
}

} // namespace

nb::object dimetric_tile_walker(
    nb::object camera, nb::object world_view_obj, int z, nb::tuple top_left,
    int blocks_width, int blocks_height, int screen_x_offset,
    int screen_y_offset, nb::object entity_hovered, nb::object selected_entity,
    int layer_index, nb::object mouse_state, nb::object player, float sun_light,
    bool water_camera_stats, bool terrain_gradient_camera_stats,
    bool iterate_right_to_left, bool iterate_bottom_to_top) {
  // Suppress unused-parameter warnings for the two camera-stats flags —
  // they are part of the public signature (consumers may pass them) but
  // the Phase-2 walker body does not branch on them. Phase 3+ may.
  (void)water_camera_stats;
  (void)terrain_gradient_camera_stats;

  WalkerContext ctx = build_context(camera);

  WorldView &world_view = nb::cast<WorldView &>(world_view_obj);
  const int world_shape_x = world_view.width;
  const int world_shape_y = world_view.height;

  // `initial_light_intensity` is `camera_model.light_intensities[layer_index]`
  // in the Python `default_terrain_handler`. Cached once per walker call
  // (layer_index is a function argument and constant for the duration of
  // a single walker invocation). Only used by the native terrain path.
  float initial_light_intensity = 0.0f;
  if (ctx.terrain_handler_is_none) {
    nb::object light_intensities = ctx.camera_model.attr("light_intensities");
    initial_light_intensity = nb::cast<float>(light_intensities[layer_index]);
  }

  int screen_y_initial;
  if (iterate_bottom_to_top) {
    screen_y_initial = (blocks_height - 1) * ctx.tile_size + screen_y_offset;
  } else {
    screen_y_initial = screen_y_offset;
  }

  const int world_layer_x_start = nb::cast<int>(top_left[0]);
  const int world_layer_y_start = nb::cast<int>(top_left[1]);

  int screen_y = screen_y_initial;

  for (int j = 0; j < blocks_height; ++j) {
    int screen_x;
    int world_layer_x;
    if (iterate_right_to_left) {
      screen_x = (blocks_width - 1) * ctx.tile_size - screen_x_offset;
      world_layer_x = world_layer_x_start + blocks_width - 1;
    } else {
      screen_x = -screen_x_offset;
      world_layer_x = world_layer_x_start;
    }

    int world_layer_y;
    if (iterate_bottom_to_top) {
      world_layer_y = world_layer_y_start + blocks_height - 1 - j;
    } else {
      world_layer_y = world_layer_y_start + j;
    }

    for (int i = 0; i < blocks_width; ++i) {
      const bool in_bounds =
          (world_layer_x >= 0 && world_layer_x < world_shape_x &&
           world_layer_y >= 0 && world_layer_y < world_shape_y);

      // Terrain fetch — native C++ accessor on WorldView.
      nb::object terrain = nb::none();
      const bool terrain_exists =
          in_bounds &&
          world_view.checkIfTerrainExist(world_layer_x, world_layer_y, z);
      if (terrain_exists) {
        terrain = world_view.getTerrain(world_layer_x, world_layer_y, z);
      }

      // Entity branch — Phase 3 + direct-C++ refactor.
      // Per non-empty entity tile the Python crossings are now only:
      //   1) `Classification(main, sub)` ctor (Python @dataclass)
      //   2) `views["entities"].get(cls)` dict lookup
      //   3) the beast or plant handler call (consumer contract)
      //   4) optional lifebar.set_health + set_position_draw (BaseView is
      //      a Python class — can't be bypassed)
      // The entity type / has_component / health-component accesses now
      // go through native `EntityInterface::getComponent<T>()` /
      // `hasComponent(ComponentFlag::…)` instead of `entity.attr(...)`.
      if (in_bounds &&
          world_view.checkIfEntityExist(world_layer_x, world_layer_y, z)) {
        nb::object entity =
            world_view.getEntity(world_layer_x, world_layer_y, z);
        EntityInterface &entity_ref = nb::cast<EntityInterface &>(entity);

        // Read main/sub type from the C++ component, not via attr access.
        const EntityTypeComponent &entity_type =
            entity_ref.getComponent<EntityTypeComponent>();
        const int main_type = entity_type.mainType;
        const int sub_type0 = entity_type.subType0;

        // Classification ctor + dict.get stay Python — Classification is
        // a `@dataclass(frozen=True)` and `views["entities"]` is a plain
        // Python dict. (Phase 6 candidate: mirror the dict in C++.)
        nb::object classification =
            ctx.classification_ctor(main_type, sub_type0);
        nb::object view_object = ctx.entity_views.attr("get")(classification);

        if (!view_object.is_none()) {
          // Beast vs plant dispatch — native is-moving check.
          const bool is_moving =
              entity_ref.hasComponent(ComponentFlag::MOVING_COMPONENT);
          const bool dispatch_beast =
              is_moving && main_type == ctx.beast_enum_value;
          nb::object handler =
              dispatch_beast ? ctx.beast_handler : ctx.plant_handler;

          nb::object handler_result =
              handler(ctx.camera_model, ctx.settings, world_view_obj, entity,
                      view_object, screen_x, screen_y, layer_index, mouse_state,
                      selected_entity, entity_hovered, sun_light);

          // Handler contract returns `(_screen_x, _screen_y,
          // entity_hovered)`.
          nb::tuple result_tuple = nb::cast<nb::tuple>(handler_result);
          nb::object new_screen_x = result_tuple[0];
          nb::object new_screen_y = result_tuple[1];
          entity_hovered = result_tuple[2];

          // Lifebar overlay — read health from the C++ component, then
          // call the Python BaseView methods. The two `lifebar.*` calls
          // remain Python because lifebar is a consumer-supplied
          // `BaseView` subclass with Python method bodies.
          if (!ctx.lifebar_view.is_none()) {
            const HealthComponent &health =
                entity_ref.getComponent<HealthComponent>();
            ctx.lifebar_view.attr("set_health")(health.healthLevel,
                                                health.maxHealth);
            ctx.lifebar_view.attr("set_position_draw")(
                ctx.render_queue, new_screen_x, new_screen_y,
                nb::arg("layer_index") = layer_index,
                nb::arg("group") = ctx.gui_group);
          }
        }
      }

      // Terrain branch — Phase 2 + direct-C++ refactor.
      // `shouldDrawTerrain`, `isTerrainAnEmptyWater`, and `drawTileEffects`
      // are all native C++ free functions in CameraUtils.cpp — calling
      // them via `aetherion.draw_tile_effects(...)` from Python would go
      // through the nanobind dispatch layer; calling them directly with
      // `EntityInterface&` / `WorldView&` / `RenderQueue&` references is
      // a plain C++ call. Per non-empty terrain tile that's three fewer
      // Python crossings.
      if (in_bounds && !terrain.is_none()) {
        EntityInterface &terrain_ref = nb::cast<EntityInterface &>(terrain);

        nb::object terrain_view_object = nb::none();
        if (shouldDrawTerrain(terrain_ref, ctx.empty_tile_debugging)) {
          // sub_type0 of the terrain's entity type is the index into
          // views["terrains"]. Read via the C++ component access, then
          // do the Python dict lookup (latter is a Phase 6 mirror
          // candidate).
          const int sub_type0 =
              terrain_ref.getComponent<EntityTypeComponent>().subType0;
          nb::object maybe = ctx.terrain_views.attr("get")(sub_type0);
          if (!maybe.is_none()) {
            terrain_view_object = maybe;
          }
        }

        // `render_queue_ref` is nullptr only in the parity-test setup
        // (Python `RecordingRenderQueue` substituted for the bound C++
        // `RenderQueue`). The test world has no terrain, so this branch
        // never fires in tests; in production the pointer is always set.
        if (ctx.render_queue_ref != nullptr) {
          drawTileEffects(terrain_ref, world_view, *ctx.render_queue_ref,
                          layer_index, ctx.gui_group_str, screen_x, screen_y,
                          ctx.tile_size);
        }

        if (!terrain_view_object.is_none()) {
          const bool empty_water = isTerrainAnEmptyWater(terrain_ref);
          if (!empty_water) {
            bool tile_became_hovered = false;
            if (ctx.terrain_handler_is_none &&
                ctx.render_queue_ref != nullptr) {
              // Native default path — no Python crossing into the
              // handler wrapper. `draw_terrain_native` returns a bool
              // directly (truthy = this tile became the hovered one).
              tile_became_hovered = draw_terrain_native(
                  ctx.draw_terrain_ctx, terrain_ref, terrain,
                  terrain_view_object, ctx.settings, selected_entity,
                  world_view, world_view_obj, mouse_state, screen_x, screen_y,
                  *ctx.render_queue_ref, ctx.render_queue, layer_index,
                  initial_light_intensity, sun_light, player,
                  ctx.gradient_view_object, terrain_gradient_camera_stats,
                  ctx.water_view, water_camera_stats, entity_hovered);
            } else {
              // Consumer-supplied callback path. Calls the Python
              // `terrain_handler` with the 15-arg shape (see
              // aetherion/src/aetherion/camera/entities_handlers.py:
              // default_terrain_handler).
              nb::object current_entity_hovered = ctx.terrain_handler(
                  ctx.camera_model, ctx.settings, terrain, terrain_view_object,
                  selected_entity, world_view_obj, mouse_state, screen_x,
                  screen_y, layer_index, entity_hovered, sun_light, player,
                  ctx.gradient_view_object, ctx.water_view);
              tile_became_hovered = !current_entity_hovered.is_none();
            }

            if (tile_became_hovered) {
              entity_hovered = terrain;
            }
          }
        }
      }

      // Position increment.
      if (iterate_right_to_left) {
        world_layer_x -= 1;
        screen_x -= ctx.tile_size;
      } else {
        world_layer_x += 1;
        screen_x += ctx.tile_size;
      }
    }

    if (iterate_bottom_to_top) {
      screen_y -= ctx.tile_size;
    } else {
      screen_y += ctx.tile_size;
    }
  }

  return entity_hovered;
}

} // namespace aetherion::render
