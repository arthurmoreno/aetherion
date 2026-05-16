"""Leak gate for perception-path and component-extraction patterns.

The game leaked 791 EntityInterface instances + all types/functions at
shutdown. Root cause: C++/Python cycles (World ↔ Python event handlers)
were never broken because `release_python_state()` was never called.
The fix promotes the atexit cleanup from tests/conftest into
`aetherion/__init__.py` so every process gets it automatically.

These tests verify that the CORRECT cleanup patterns leave zero leaks:

  P1 — PerceptionResponse created and deleted before shutdown is clean.
  P2 — Component extraction wrappers released before shutdown is clean.
  P3 — WorldView.entities cleared before shutdown is clean.
  P4 — Full update + perception loop with proper cleanup is clean.

Note: plain Python objects (PerceptionResponse, WorldView) held as
locals are NOT a nanobind leak — Python destroys them via __main__
teardown. Only C++/Python reference cycles (the test_t3 pattern in
test_nanobind_leak_isolation.py) survive shutdown and produce leaks.
"""

from __future__ import annotations

from _nanobind_leak_harness import run_snippet


def test_p1_perception_response_released_before_shutdown_is_clean():
    """PerceptionResponse created and deleted before interpreter exits
    must not leak. Regression gate: if the FlatBuffer arena or the
    entity inside PerceptionResponse holds a ref that outlives `del pr`,
    this test fails."""
    result = run_snippet(
        """
        import gc
        from aetherion._aetherion import World, EntityInterface, WorldView
        w = World(3, 3, 3)
        entity = EntityInterface()
        entity.set_entity_id(1)
        wv = WorldView()
        wv.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)
        from aetherion._aetherion import PerceptionResponse
        pr = PerceptionResponse(entity, wv)
        del pr
        del wv
        del entity
        w.release_python_state()
        del w
        gc.collect()
        """
    )
    assert result.is_clean, f"PerceptionResponse leak: {result.summary()}\nstderr:\n{result.stderr}"


def test_p2_component_extraction_released_is_clean():
    """Components extracted via get_position / get_inventory / etc. are
    nanobind wrappers. Deleting them before shutdown must leave zero
    leaks."""
    result = run_snippet(
        """
        import gc
        from aetherion._aetherion import (
            World, EntityInterface,
            Position, Velocity, HealthComponent, Inventory,
        )
        w = World(3, 3, 3)
        e = EntityInterface()
        e.set_entity_id(7)
        pos = Position(); pos.x = 1; pos.y = 2; pos.z = 3
        e.set_position(pos)
        vel = Velocity(); vel.vx = 1.0
        e.set_velocity(vel)
        hp = HealthComponent(); hp.health_level = 50.0; hp.max_health = 100.0
        e.set_health(hp)

        # Simulate what game AI does: extract components, use, discard
        got_pos = e.get_position()
        got_vel = e.get_velocity()
        got_hp  = e.get_health()
        del got_pos, got_vel, got_hp
        del pos, vel, hp, e
        w.release_python_state()
        del w
        gc.collect()
        """
    )
    assert result.is_clean, f"component extraction leak: {result.summary()}\nstderr:\n{result.stderr}"


def test_p3_worldview_with_entities_released_is_clean():
    """WorldView.entities holds EntityInterface wrappers. Clearing the
    map before shutdown must leave zero leaks."""
    result = run_snippet(
        """
        import gc
        from aetherion._aetherion import World, EntityInterface, WorldView
        w = World(3, 3, 3)
        wv = WorldView()
        wv.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)
        for i in range(10):
            e = EntityInterface()
            e.set_entity_id(i)
            wv.entities[i] = e
            del e
        wv.entities.clear()
        del wv
        w.release_python_state()
        del w
        gc.collect()
        """
    )
    assert result.is_clean, f"WorldView entities leak: {result.summary()}\nstderr:\n{result.stderr}"


def test_p4_full_update_and_perception_loop_is_clean():
    """Full game-loop pattern: world.update() followed by
    create_perception_responses(), consume results, clear all refs, then
    release. Must leave zero leaks."""
    result = run_snippet(
        """
        import gc
        from aetherion._aetherion import World
        w = World(3, 3, 3)
        w.update()

        # Simulate game: collect perception for entity 0 (may return empty)
        try:
            prs = w.create_perception_responses([0])
        except Exception:
            prs = []

        # Game must clear perception results before shutdown
        del prs
        w.release_python_state()
        del w
        gc.collect()
        """,
        timeout=60,
    )
    assert result.is_clean, f"full-loop leak: {result.summary()}\nstderr:\n{result.stderr}"
