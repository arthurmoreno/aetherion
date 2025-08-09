import collections
import threading
import time
from collections import defaultdict, deque
from enum import StrEnum
from typing import Any, Callable, Generic, TypeVar

from lifesim.logger import logger
from pydantic import BaseModel, Field


class GameEventType(StrEnum):
    """Enumeration of all game level events."""

    # World Management Events
    WORLD_CREATE_REQUESTED = "world_create_requested"
    WORLD_CREATED = "world_created"
    WORLD_LOAD_REQUESTED = "world_load_requested"
    WORLD_LOADED = "world_loaded"
    WORLD_SAVE_REQUESTED = "world_save_requested"
    WORLD_SAVED = "world_saved"

    WORLD_CONNECT_REQUESTED = "world_connect_requested"
    WORLD_CONNECTED = "world_connected"
    WORLD_DISCONNECT_REQUESTED = "world_disconnect_requested"
    WORLD_DISCONNECTED = "world_disconnected"

    # Scene Management Events
    SCENE_CHANGE_REQUESTED = "scene_change_requested"
    SCENE_CHANGED = "scene_changed"

    # Game State Events
    GAME_PAUSE_REQUESTED = "game_pause_requested"
    GAME_RESUME_REQUESTED = "game_resume_requested"
    GAME_STATE_CHANGED = "game_state_changed"

    # Beast Events
    CREATE_BEAST_REQUESTED = "create_beast_requested"
    BEAST_CONNECT_REQUESTED = "beast_connect_requested"
    BEAST_CONNECTED = "beast_connected"
    BEAST_DISCONNECT_REQUESTED = "beast_disconnect_requested"
    BEAST_DISCONNECTED = "beast_disconnected"

    # Player Connection Events
    PLAYER_CONNECTED = "player_connected"
    PLAYER_DISCONNECTED = "player_disconnected"


E = TypeVar("E", bound=StrEnum)


class GameEvent(BaseModel, Generic[E]):
    """
    Represents an event that flows through event bus systems.

    Encapsulates game event information including type, data, source, and timestamp.
    Used by both ObserverEventBus and DrainableTopicBus systems.

    Attributes:
        event_type: Enum value identifying the event type
        data: Optional event-specific data dictionary
        source: Optional source identifier for debugging
        timestamp: Auto-generated if not provided
    """

    event_type: E
    data: dict[str, Any] = Field(default_factory=dict)
    source: str | None = None
    timestamp: float = Field(default_factory=lambda: time.time())

    class Config:
        # Keep the enum members as their actual Enum type (not auto-coerced to value)
        use_enum_values: bool = False
        frozen: bool = True  # optional: make events immutable if that fits your semantics


class EventBus(Generic[E]):
    """Push-based event system using observer pattern with callback registration.

    Events are queued and dispatched to registered callbacks synchronously.
    Ideal for real-time game systems requiring immediate event response.

    Key features:
        - Push-based: Immediate callback execution
        - Type-safe: Enum-based event types
        - Queued processing: Batch event handling
        - Error-resilient: Callback exceptions don't block others

    Note: Not thread-safe. Use in single-threaded game loops.
    """

    def __init__(self) -> None:
        self._listeners: dict[E, list[Callable[[GameEvent[E]], None]]] = defaultdict(list)
        self._event_queue: deque[GameEvent[E]] = deque()

    # ------------------------------------------------------------------
    # Subscription management
    # ------------------------------------------------------------------
    def subscribe(self, event_type: E, callback: Callable[[GameEvent[E]], None]) -> None:
        """Register a callback to be invoked when the specified event type occurs.

        Args:
            event_type: The type of event to listen for (e.g., GameEventType.WORLD_CREATED)
            callback: Function to call when the event occurs. Must accept a GameEvent parameter.

        Note:
            Duplicate callbacks for the same event type are automatically prevented.
            The callback will be executed synchronously when the event is processed.
        """
        if callback not in self._listeners[event_type]:
            self._listeners[event_type].append(callback)

    def unsubscribe(self, event_type: E, callback: Callable[[GameEvent[E]], None]) -> None:
        """Remove a callback from event type notifications.

        Args:
            event_type: The event type to stop listening to
            callback: The specific callback function to remove

        Note:
            Silently ignores attempts to remove non-existent callbacks.
        """
        try:
            self._listeners[event_type].remove(callback)
        except (KeyError, ValueError):
            pass

    # ------------------------------------------------------------------
    # Event emission
    # ------------------------------------------------------------------
    def emit(self, event_type: E, data: dict[str, Any] = {}, source: str | None = None) -> GameEvent[E]:
        """Create an event and queue it for later processing via process_events().

        Args:
            event_type: The type of event to emit (e.g., GameEventType.WORLD_CREATED)
            data: Optional dictionary containing event-specific data
            source: Optional identifier of the event source for debugging/tracing

        Returns:
            The created GameEvent instance (with auto-generated timestamp)

        Note:
            Events are queued, not immediately dispatched. Call process_events() to deliver
            them to subscribers. For immediate delivery, use emit_immediate() instead.
        """
        event: GameEvent[E] = GameEvent(event_type=event_type, data=data, source=source)
        self._event_queue.append(event)
        return event

    def emit_immediate(self, event: GameEvent[E]) -> None:
        """Dispatch an event to all registered listeners immediately.

        Args:
            event: The GameEvent to dispatch

        Note:
            Callbacks are executed synchronously in registration order. If a callback
            raises an exception, it's logged but doesn't prevent other callbacks from running.
        """
        for callback in list(self._listeners.get(event.event_type, [])):
            try:
                callback(event)
            except Exception as exc:  # pragma: no cover - defensive
                logger.error(f"Error in event callback: {exc}")

    # ------------------------------------------------------------------
    # Event processing
    # ------------------------------------------------------------------
    def process_events(self) -> None:
        """Dispatch all queued events to their registered listeners in FIFO order.

        This is typically called once per game loop iteration to process all events
        that were emitted since the last call. Events are processed in the order
        they were emitted (first-in, first-out).

        Note:
            After processing, the event queue is empty. If a callback during processing
            emits new events, they will be queued for the next process_events() call.
        """
        while self._event_queue:
            event: GameEvent[E] = self._event_queue.popleft()
            self.emit_immediate(event)

    def clear(self) -> None:
        """Remove all pending events from the queue without dispatching them.

        Useful for resetting the event system state, such as when transitioning
        between game scenes or during error recovery.
        """
        self._event_queue.clear()


# Global bus instance used by the engine
event_bus = EventBus()


class PubSubTopicBroker(Generic[E]):
    """Pull-based publish-subscribe event system with topic-based routing.

    Events are published to topics and consumers pull events at their own pace.
    Decouples producers from consumers with individual event queues per consumer.

    Key features:
        - Pull-based: Consumers retrieve events when ready
        - Topic-based: String topic routing with filtering
        - Thread-safe: Concurrent access support
        - Buffered: Individual consumer event queues

    Use cases: Multi-threaded systems, audit logging, analytics, distributed communication.
    """

    def __init__(self):
        self._consumers: list["TopicReader[E]"] = []
        self._lock = threading.RLock()

    def register_consumer(self, consumer: "TopicReader[E]") -> None:
        """Register a consumer to receive published events.

        Args:
            consumer: The Consumer instance to register

        Note:
            Duplicate registration attempts are ignored. Thread-safe operation.
        """
        with self._lock:
            if consumer not in self._consumers:
                self._consumers.append(consumer)

    def unregister_consumer(self, consumer: "TopicReader[E]") -> None:
        """Remove a consumer from receiving published events.

        Args:
            consumer: The Consumer instance to unregister

        Note:
            Silently ignores attempts to unregister non-existent consumers. Thread-safe operation.
        """
        with self._lock:
            if consumer in self._consumers:
                self._consumers.remove(consumer)

    def publish_event(self, topic: str, event: GameEvent[E]) -> None:
        """Publish an event to a topic, delivering it to all relevant consumers.

        Args:
            topic: The topic string to publish to (e.g., "world_events", "player_actions")
            event: The GameEvent to publish

        Note:
            Events are delivered to consumers that either subscribe to the specific topic
            or subscribe to all topics (topics=None). Thread-safe operation.
        """
        with self._lock:
            # Fan-out to all consumers (each enqueues independently)
            for c in list(self._consumers):
                if c.topics is None or topic in c.topics:
                    c.enqueue(event)


class TopicReader(Generic[E]):
    """Pull-based event consumer for PubSubTopicBroker with topic filtering.

    Maintains individual event queue and retrieves events via drain() method.
    Supports topic filtering and provides thread-safe operations.

    Key features:
        - Independent queuing: Own event buffer per consumer
        - Topic filtering: Subscribe to specific topics or all topics
        - Thread-safe: Protected operations with locks
        - Context manager: Automatic cleanup support
        - Non-blocking: peek() for inspection without consumption

    Args:
        bus: PubSubTopicBroker instance to consume from
        topics: Optional topic filter list (None = all topics)
    """

    def __init__(self, bus: PubSubTopicBroker[E], topics: list[str] | None = None):
        self._queue: collections.deque[GameEvent[E]] = collections.deque()
        self._lock = threading.RLock()
        self.topics = set(topics) if topics is not None else None
        self._bus = bus
        self._bus.register_consumer(self)

    def enqueue(self, event: GameEvent[E]) -> None:
        """Internal method called by PubSubEventBus to add events to this consumer's queue.

        Args:
            event: The GameEvent to add to the queue

        Note:
            This method is typically called automatically by the bus. Thread-safe operation.
        """
        with self._lock:
            self._queue.append(event)

    def drain(self) -> list[GameEvent[E]]:
        """Retrieve and remove all queued events.

        Returns:
            List of GameEvent objects in the order they were received.
            Empty list if no events are queued.

        Note:
            This operation clears the consumer's queue. Events are returned in FIFO order.
            Thread-safe operation.
        """
        with self._lock:
            items: list[GameEvent[E]] = list(self._queue)
            self._queue.clear()
        return items

    def peek(self, n: int = 10) -> list[GameEvent[E]]:
        """Inspect queued events without removing them.

        Args:
            n: Maximum number of events to return (default: 10)

        Returns:
            List of up to n GameEvent objects from the front of the queue.

        Note:
            Events remain in the queue after peeking. Thread-safe operation.
        """
        with self._lock:
            return list(self._queue)[:n]

    def close(self) -> None:
        """Unregister this consumer from the bus and clean up resources.

        Note:
            After calling close(), this consumer will no longer receive events.
            Any events still in the queue remain accessible until drained.
        """
        self._bus.unregister_consumer(self)

    def __enter__(self):
        """Context manager entry - returns self for use in 'with' statements."""
        return self

    def __exit__(self, exc_type: type[BaseException] | None, exc_value: BaseException | None, tb: object) -> None:
        """Context manager exit - automatically calls close() for cleanup."""
        self.close()
