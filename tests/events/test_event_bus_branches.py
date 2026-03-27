from __future__ import annotations

from aetherion.events import EventBus, GameEvent, GameEventType
from aetherion.events import logger as events_logger


def test_subscribe_prevents_duplicate_callbacks():
    bus: EventBus[GameEventType] = EventBus()

    def _handler(_event):
        return None

    bus.subscribe(GameEventType.WORLD_CREATED, _handler)
    bus.subscribe(GameEventType.WORLD_CREATED, _handler)

    assert len(bus._listeners[GameEventType.WORLD_CREATED]) == 1


def test_unsubscribe_missing_callback_is_noop():
    bus: EventBus[GameEventType] = EventBus()

    def _handler(_event):
        return None

    bus.unsubscribe(GameEventType.WORLD_CREATED, _handler)
    assert bus._listeners[GameEventType.WORLD_CREATED] == []


def test_process_events_dispatches_in_order_and_clears_queue():
    bus: EventBus[GameEventType] = EventBus()
    received: list[str] = []

    bus.subscribe(GameEventType.WORLD_CREATED, lambda _e: received.append("created"))
    bus.subscribe(GameEventType.WORLD_SAVED, lambda _e: received.append("saved"))

    bus.emit(GameEventType.WORLD_CREATED, {"k": 1}, "tests")
    bus.emit(GameEventType.WORLD_SAVED, {"k": 2}, "tests")
    bus.process_events()

    assert received == ["created", "saved"]
    assert len(bus._event_queue) == 0


def test_emit_immediate_isolates_exceptions_and_logs(monkeypatch):
    bus: EventBus[GameEventType] = EventBus()
    called = {"ok": 0, "logged": 0}

    def _bad(_event):
        raise RuntimeError("boom")

    def _good(_event):
        called["ok"] += 1

    monkeypatch.setattr(events_logger, "error", lambda _msg: called.__setitem__("logged", called["logged"] + 1))
    bus.subscribe(GameEventType.WORLD_CREATED, _bad)
    bus.subscribe(GameEventType.WORLD_CREATED, _good)

    bus.emit_immediate(GameEvent(event_type=GameEventType.WORLD_CREATED))

    assert called["ok"] == 1
    assert called["logged"] == 1
