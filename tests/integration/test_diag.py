"""Tests for the aetherion::diag unified diagnostic module.

These exercise the Python-facing surface defined in src/aetherion.cpp
(which is a thin wrapper over the C++ Registry in src/diag/Diag.cpp).
The Registry is a process-wide singleton, so each test calls
`reset_for_testing()` first.
"""

from __future__ import annotations

import pytest

from aetherion._aetherion import diag


@pytest.fixture(autouse=True)
def fresh_registry():
    diag.reset_for_testing()
    yield
    diag.reset_for_testing()


def test_counter_inc_increments_accumulator():
    c = diag.counter("test.counter.basic")
    assert c.enabled()
    c.inc()
    c.inc(4)
    assert diag.peek_counter("test.counter.basic") == 5


def test_tick_resets_counter_after_flush_window():
    c = diag.counter("test.counter.flush", flush_every_ms=0)
    c.inc(7)
    assert diag.peek_counter("test.counter.flush") == 7
    diag.tick()
    # After tick, accumulator is reset to 0 even though the GameDB sink is
    # absent (Registry was never initialised with a GameDBHandler in this
    # test process — samples drop, but reset still happens).
    assert diag.peek_counter("test.counter.flush") == 0


def test_double_registration_raises():
    diag.counter("test.counter.dup")
    with pytest.raises(RuntimeError, match="duplicate"):
        diag.counter("test.counter.dup")


def test_disable_glob_silences_matching_counters():
    a = diag.counter("water_sim.tick_phases")
    b = diag.counter("water_sim.evap_condense")
    diag.disable("water_sim.tick_phases")
    assert not a.enabled()
    assert b.enabled()
    a.inc()
    b.inc(3)
    assert diag.peek_counter("water_sim.tick_phases") == 0
    assert diag.peek_counter("water_sim.evap_condense") == 3


def test_disable_glob_with_trailing_star():
    # `water_sim.*` should silence both subchannels but leave unrelated
    # counters alone.
    a = diag.counter("water_sim.tick_phases")
    b = diag.counter("water_sim.evap_condense")
    c = diag.counter("physics.move_solid_entity")
    diag.disable("water_sim.*")
    assert not a.enabled()
    assert not b.enabled()
    assert c.enabled()


def test_disable_then_register_late_handle_starts_disabled():
    diag.disable("late.*")
    c = diag.counter("late.counter")
    assert not c.enabled()


def test_enable_undoes_disable():
    c = diag.counter("test.toggle")
    diag.disable("test.toggle")
    assert not c.enabled()
    diag.enable("test.toggle")
    assert c.enabled()
    c.inc()
    assert diag.peek_counter("test.toggle") == 1


def test_gauge_set_and_flush():
    g = diag.gauge("test.gauge.last", flush_every_ms=0)
    g.set(5.0)
    g.set(7.5)
    diag.tick()  # snapshot-and-reset; AggFn::Last → emits 7.5 (no peek API)
    # No exception means the flush ran cleanly. The GameDB sink is absent
    # so we can't observe the value here — full end-to-end coverage runs
    # via the World fixture in test_diag_world.py.
    assert g.enabled()


def test_event_log_accepts_dict_payload():
    e = diag.event("test.events.basic")
    # Round-trip through json.dumps; if the conversion fails this raises.
    e.log({"phase": "begin", "tick": 42, "values": [1, 2, 3]})


def test_event_log_validates_required_keys_but_still_emits():
    e = diag.event("test.events.required", required_keys=["phase"])
    # Missing required key — should warn (once) but not raise.
    e.log({"tick": 1})
    e.log({"phase": "ok", "tick": 2})


def test_event_disable_makes_log_a_noop():
    e = diag.event("test.events.disable")
    diag.disable("test.events.disable")
    assert not e.enabled()
    # Should not raise; should not emit. Smoke test only.
    e.log({"x": 1})


def test_peek_unknown_counter_returns_zero():
    assert diag.peek_counter("nonexistent.counter") == 0


def test_is_enabled_unknown_returns_false():
    assert not diag.is_enabled("nonexistent.counter")
