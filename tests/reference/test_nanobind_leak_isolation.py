"""Hypothesis bisection tests for the nanobind reference leak.

Each test runs a self-contained Python snippet in a fresh interpreter
(via `_nanobind_leak_harness.run_snippet`) and asserts on the leak /
abort outcome. See
`.claude/docs/epics-plans/2026-05-06-nanobind-leak-bisection.md` for the
hypothesis tracking table this file feeds.

Test naming: `test_t<N>_<short>` matches the plan's T<N> identifiers.
"""

from __future__ import annotations

from _nanobind_leak_harness import run_snippet


def test_t1_bare_world_is_clean():
    """H1: bare `World()` create + drop leaks zero objects.

    Confirms the leak is *not* triggered by mere instantiation — it
    requires Python state to be attached. If this test ever fails,
    there is an unaccounted holder we have not enumerated.
    """
    result = run_snippet(
        """
        from aetherion._aetherion import World
        w = World(4, 4, 4)
        del w
        """
    )
    assert result.is_clean, f"bare World leaked: {result.summary()}\nstderr:\n{result.stderr}"


def test_t3_system_with_back_ref_leaks():
    """H3: `add_python_system(s)` where `s.world = world` produces a
    Python<->C++ cycle (s.world keeps the World wrapper alive; C++
    `pythonSystems` keeps s alive via `nb::object`). `~World` never
    runs, so the leak persists at module shutdown.

    NOTE: this test asserts that the bug is present *today*. Once the
    cleanup-plan fix lands (`release_python_state()` clearing
    `pythonSystems`), this assertion must be inverted to `is_clean` —
    the test breaking is the signal that the fix worked on this path.
    """
    result = run_snippet(
        """
        from aetherion._aetherion import World
        class S:
            def __init__(self, w):
                self.world = w
            def update(self):
                pass
        w = World(4, 4, 4)
        w.add_python_system(S(w))
        del w
        """
    )
    assert result.leaked_instances > 0, (
        f"expected baseline leak from Python<->C++ cycle, but ran clean: {result.summary()}\nstderr:\n{result.stderr}"
    )


def test_t7_release_python_state_zeroes_leak():
    """H7: calling `world.release_python_state()` between T3's setup
    (system with cycle) and `del w` zeroes the leak.

    This is the regression gate for the cleanup plan's Task 1. Before
    the fix landed: this test was RED (the binding did not exist). After
    the fix: this test is GREEN — and any future change that re-introduces
    a long-lived Python-ref accumulator on `World` without clearing it
    in `releasePythonState()` will turn it RED again.
    """
    result = run_snippet(
        """
        from aetherion._aetherion import World
        class S:
            def __init__(self, w):
                self.world = w
            def update(self):
                pass
        w = World(4, 4, 4)
        w.add_python_system(S(w))
        w.release_python_state()  # break the cycle from the C++ side
        del w
        """
    )
    assert result.is_clean, (
        f"release_python_state() did not zero the leak: {result.summary()}\nstderr:\n{result.stderr}"
    )


def test_t8_breaking_cycle_from_python_zeroes_leak():
    """H8: if the cycle is broken from the Python side before `del w`,
    `~World` runs, `pythonSystems.~vector` destroys the held
    `nb::object`, and the leak goes to zero.

    If this test fails (i.e. still leaks), the cycle is not the only
    path — the C++ container is retaining refs even after the back-ref
    is broken. That would mean a Python-callable
    `release_python_state()` (clearing the container directly) is
    *required*, not just a nice-to-have for forgetful callers.
    """
    result = run_snippet(
        """
        from aetherion._aetherion import World
        class S:
            def __init__(self, w):
                self.world = w
            def update(self):
                pass
        w = World(4, 4, 4)
        s = S(w)
        w.add_python_system(s)
        s.world = None  # break the cycle from Python's side
        del s
        del w
        """
    )
    assert result.is_clean, (
        f"breaking the cycle from Python did not zero the leak: {result.summary()}\nstderr:\n{result.stderr}"
    )
