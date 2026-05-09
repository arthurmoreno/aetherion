"""Lifecycle tests for WorldInterface — dispose() / context manager / safety net.

These cover the Python<->C++ ref-cycle fix described in the
.claude/docs/epics-plans/2026-05-09-python-cpp-lifecycle-standard.md
plan: WorldInterface owns the C++ World for the duration of a session and
must release the C++ Python state before being dropped.

Together with the existing `tests/reference/test_nanobind_leak_isolation.py`
suite, these tests close the loop:
  - T7 (existing): proves `World.release_python_state()` itself works.
  - T(this file): proves the WorldInterface bridge wires it correctly.
"""

from __future__ import annotations

import sys
from pathlib import Path

# Reuse the subprocess leak-harness from tests/reference so we get a fresh
# interpreter per assertion — pytest's shared interpreter masks Py_Finalize
# behaviour.
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "reference"))
from _nanobind_leak_harness import run_snippet  # noqa: E402

# ─── In-process behavioural tests ─────────────────────────────────────


def _make_world_interface():
    from aetherion import World
    from aetherion.world.constants import WorldInstanceTypes
    from aetherion.world.interface import WorldInterface

    world = World(4, 4, 4)
    return WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)


def test_dispose_drops_world_ref():
    wi = _make_world_interface()
    assert wi.world is not None
    wi.dispose()
    assert wi.world is None


def test_dispose_is_idempotent():
    wi = _make_world_interface()
    wi.dispose()
    # Second call must not raise.
    wi.dispose()
    assert wi.world is None


def test_context_manager_disposes_on_exit():
    from aetherion import World
    from aetherion.world.constants import WorldInstanceTypes
    from aetherion.world.interface import WorldInterface

    world = World(4, 4, 4)
    with WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world) as wi:
        assert wi.world is not None
    assert wi.world is None


def test_dispose_after_close_still_releases_world():
    """`close()` only stops the executor today; `dispose()` must still
    release the C++ state even when called after `close()`."""
    wi = _make_world_interface()
    wi.close()
    assert wi.world is not None  # close() does not touch self.world
    wi.dispose()
    assert wi.world is None


def test_unload_world_disposes_and_drops():
    """End-to-end: WorldManager.unload_world must dispose the
    WorldInterface, breaking the ref cycle, before dropping it."""
    # Reuse the same FakeEventBus the existing manager-lifecycle tests
    # use — keeps the test in pure Python without spinning up a real
    # event subscription graph.
    sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
    from conftest import FakeEventBus  # noqa: E402

    from aetherion.world.manager import WorldManager

    manager = WorldManager(event_bus=FakeEventBus(), default_event_handlers={})
    wi = _make_world_interface()
    manager.worlds["x"] = wi

    manager.unload_world("x")
    assert "x" not in manager.worlds
    assert wi.world is None  # dispose() ran


# ─── Subprocess leak-harness test (the regression gate) ────────────────


def test_world_interface_dispose_zeroes_the_leak_for_session_with_systems():
    """The bridge-layer equivalent of T7: a session that registered a
    Python system via WorldInterface must leak zero instances after
    dispose() runs.

    This is the regression gate for the `python-cpp-lifecycle-standard`
    plan. If this turns red, someone added a new Python-ref accumulator
    on World without wiring it into release_python_state(), or
    WorldInterface.dispose() stopped calling release_python_state().
    """
    result = run_snippet(
        """
        from aetherion import World
        from aetherion.world.constants import WorldInstanceTypes
        from aetherion.world.interface import WorldInterface

        class S:
            def __init__(self, w):
                self.world = w
            def update(self):
                pass

        world = World(4, 4, 4)
        wi = WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world)
        wi.world.add_python_system(S(wi.world))
        del world  # WorldInterface holds the only ref to the C++ World
        wi.dispose()
        del wi
        """
    )
    assert result.is_clean, (
        f"WorldInterface.dispose() did not zero the leak: {result.summary()}\nstderr:\n{result.stderr}"
    )


def test_world_interface_context_manager_zeroes_the_leak():
    """The `with WorldInterface(...) as wi:` pattern must leak zero
    instances even when a Python system is registered inside the
    block — proves __exit__ wires through dispose() correctly."""
    result = run_snippet(
        """
        from aetherion import World
        from aetherion.world.constants import WorldInstanceTypes
        from aetherion.world.interface import WorldInterface

        class S:
            def __init__(self, w):
                self.world = w
            def update(self):
                pass

        world = World(4, 4, 4)
        with WorldInterface(WorldInstanceTypes.SYNCHRONOUS, world) as wi:
            wi.world.add_python_system(S(wi.world))
            del world
        del wi
        """
    )
    assert result.is_clean, (
        f"`with WorldInterface(...)` did not zero the leak: {result.summary()}\nstderr:\n{result.stderr}"
    )
