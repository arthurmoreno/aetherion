// DimetricTileWalker — C++ replacement for the per-tile inner loop of
// `aetherion.camera.dimetric.Camera.draw_player_perspective_layer`.
//
// Migration plan:
// .claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md
//
// Scope (Phase 2 — current):
//   * The doubly-nested per-tile loop iterates in C++.
//   * Bounds check, terrain existence + fetch, screen-coordinate math,
//     terrain-view lookup, draw_tile_effects invocation, terrain-handler
//     dispatch all run in the C++ walker body.
//   * The entity branch is still delegated to a Python helper
//     (`Camera._entity_tile_step`) — Phase 3 will migrate it natively.
//
// Public contract:
//   * Signature matches `draw_player_perspective_layer` (17 args + the
//     `Camera` instance for callback access).
//   * Handler-callback signatures (`beast_entity_handler`,
//     `plant_entity_handler`, `terrain_handler`) are not touched — this
//     module invokes the callables the consumer already registered with
//     the `Camera` at construction time.

#pragma once

#include <nanobind/nanobind.h>

namespace aetherion::render {

// Phase 2 entry point. Iterates the visible window in C++, performs the
// terrain branch natively, and delegates the entity branch to the Python
// `Camera._entity_tile_step` helper. Returns the (possibly updated)
// `entity_hovered` — same semantics as the Python implementation.
//
// `camera` is the Python `Camera` instance; the walker reads
// `camera.settings`, `camera.views`, `camera._camera_model`,
// `camera._terrain_handler`, and `camera._entity_tile_step` to mirror
// the Python loop body. All other state is passed by argument so this
// function stays stateless and side-effect-free outside the handler
// callbacks and the C++ accessors it makes on the `WorldView`.
nanobind::object dimetric_tile_walker(
    nanobind::object camera, nanobind::object world_view, int z,
    nanobind::tuple top_left, int blocks_width, int blocks_height,
    int screen_x_offset, int screen_y_offset, nanobind::object entity_hovered,
    nanobind::object selected_entity, int layer_index,
    nanobind::object mouse_state, nanobind::object player, float sun_light,
    bool water_camera_stats, bool terrain_gradient_camera_stats,
    bool iterate_right_to_left, bool iterate_bottom_to_top);

} // namespace aetherion::render
