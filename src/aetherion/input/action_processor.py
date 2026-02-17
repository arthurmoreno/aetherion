from typing import Any, Callable

from aetherion import PubSubTopicBroker, TopicReader
from aetherion.events.action_event import InputEventActionType
from aetherion.game_state.state import SharedState
from aetherion.input.event_action_handler import InputEventActionHandler


class InputActionProcessor:
    # TODO: Receive logger
    def __init__(
        self,
        pubsub_broker: PubSubTopicBroker[InputEventActionType],
        command_specs: dict[InputEventActionType, InputEventActionHandler],
    ) -> None:
        """
        :param input_queue: A queue containing commands (each is an InputEventActionType).
        :param command_specs: A dict that maps each InputEventActionType to a spec,
        The "kwargs" dict can be empty if no arguments are needed for a certain command.
        """
        self.topic_name: str = "input_action_queue"
        self.topic_reader: TopicReader[InputEventActionType] = TopicReader(bus=pubsub_broker, topics=[self.topic_name])
        self.command_specs: dict[InputEventActionType, InputEventActionHandler] = command_specs

    def process_inputs(self, shared_state: SharedState) -> None:
        """
        Dequeue and process all pending commands, looking up their
        handler and default kwargs in 'command_specs'.
        """
        for event in self.topic_reader.drain():
            cmd_type: InputEventActionType = event.event_type
            spec: InputEventActionHandler | None = self.command_specs.get(cmd_type)
            if not spec:
                # logger.warning(f"[Processor] No spec found for command: {cmd_type.value}")
                continue

            handler: Callable[[], None] = spec.handler
            kwargs: dict[str, Any] = {}

            kwargs_set: set[str] = spec.processor_kwargs
            if "shared_state" in kwargs_set:
                kwargs["shared_state"] = shared_state

            fixed_kwargs = spec.kwargs
            kwargs = {**kwargs, **fixed_kwargs}

            # logger.info(f"[Processor] Processing command: {cmd_type.value} with kwargs: {kwargs}")
            handler(**kwargs)  # Call the handler with the stored kwargs
