"""End-to-end test: diag::Counter values land in GameDB after World::update().

This validates the wiring from Task 7 (Registry::tick from World loop) and
Task 9 (PhysicsEngine migration to diag::Counter handles).
"""

from __future__ import annotations

import gc
import time

from aetherion import World
from aetherion._aetherion import diag


def test_world_update_calls_diag_tick_without_error():
    world = World(3, 3, 3)
    try:
        # If Registry::tick wiring is wrong this raises or aborts.
        world.update()
    finally:
        world.release_python_state()
        del world
        gc.collect()


def test_world_update_flushes_due_counters():
    """End-to-end proof that World::update() runs Registry::tick(): a
    counter registered with flush_every_ms=0 has its accumulator reset
    after the next World::update() call. The reset only happens inside
    Sink-routing tick() — `inc()` itself never resets — so this transition
    is unambiguous evidence the wiring works.

    Verifying the GameDB row landed end-to-end requires a Python binding
    for query_time_series that returns iterable rows; deferred to v2.
    Until then, the GameDBSink path is covered by the C++ binding test
    that confirms `putTimeSeries` is called.
    """
    world = World(3, 3, 3)
    try:
        c = diag.counter("test.diag_world.flush", flush_every_ms=0)
        c.inc(11)
        assert diag.peek_counter("test.diag_world.flush") == 11
        # Two updates: the first sets next_flush in the future on
        # registration; the second is past it. Loop a few times in case
        # of clock granularity.
        for _ in range(5):
            world.update()
            if diag.peek_counter("test.diag_world.flush") == 0:
                break
            time.sleep(0.01)
        assert diag.peek_counter("test.diag_world.flush") == 0
    finally:
        world.release_python_state()
        del world
        gc.collect()


def test_physics_metric_names_unchanged():
    """Regression: post-migration counter names match the legacy GameDB
    series names, so existing dashboards and historical data continue to
    work."""
    expected = {
        "physics_move_gas_entity",
        "physics_move_solid_entity",
        "physics_evaporate_water_entity",
        "physics_condense_water_entity",
        "physics_water_fall_entity",
        "physics_water_spread",
        "physics_water_gravity_flow",
        "physics_terrain_phase_conversion",
        "physics_vapor_creation",
        "physics_water_creation",
        "physics_vapor_merge_up",
        "physics_vapor_merge_sideways",
        "physics_add_vapor_to_tile_above",
        "physics_delete_or_convert_terrain",
        "physics_invalid_terrain_found",
        "physics_plant_water_uptake",
    }
    world = World(3, 3, 3)
    try:
        for name in expected:
            assert diag.is_enabled(name), (
                f"physics counter '{name}' was not registered by PhysicsEngine::registerDiagCounters()"
            )
    finally:
        world.release_python_state()
        del world
        gc.collect()
