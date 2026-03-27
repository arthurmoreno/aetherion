from __future__ import annotations

from types import SimpleNamespace

from aetherion.events import GameEvent, PubSubTopicBroker
from aetherion.events.action_event import InputEventActionType
from aetherion.input.action_processor import InputActionProcessor
from aetherion.input.event_action_handler import InputEventActionHandler


def test_process_inputs_ignores_unknown_events():
    broker: PubSubTopicBroker[InputEventActionType] = PubSubTopicBroker()
    processor = InputActionProcessor(broker, command_specs={})
    broker.publish_event("input_action_queue", GameEvent(event_type=InputEventActionType.JUMP))

    # No command specs available should be a no-op.
    processor.process_inputs(SimpleNamespace())


def test_process_inputs_passes_shared_state_and_fixed_kwargs():
    broker: PubSubTopicBroker[InputEventActionType] = PubSubTopicBroker()
    calls: list[dict[str, object]] = []
    shared_state = SimpleNamespace(name="state")

    def _handler(**kwargs):
        calls.append(kwargs)

    spec = InputEventActionHandler(
        handler=_handler,
        kwargs={"speed": 2.5},
        processor_kwargs={"shared_state"},
    )
    processor = InputActionProcessor(broker, command_specs={InputEventActionType.WALK_LEFT: spec})
    broker.publish_event("input_action_queue", GameEvent(event_type=InputEventActionType.WALK_LEFT))

    processor.process_inputs(shared_state)

    assert len(calls) == 1
    assert calls[0]["shared_state"] is shared_state
    assert calls[0]["speed"] == 2.5
