from collections import defaultdict, deque
from dataclasses import dataclass
from enum import Enum
from typing import Any, Callable

from lifesim.logger import logger


class GameEventType(Enum):
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


@dataclass
class GameEvent:
    """Represents an event that flows through the :class:`EventBus`."""

    event_type: Enum
    data: dict[str, Any] | None = None
    source: str | None = None
    timestamp: float | None = None

    def __post_init__(self) -> None:
        if self.timestamp is None:
            import time

            self.timestamp = time.time()
        if self.data is None:
            self.data = {}


class EventBus:
    """Central hub for dispatching and receiving game events."""

    def __init__(self) -> None:
        self._listeners: dict[Enum, list[Callable[[GameEvent], None]]] = defaultdict(list)
        self._event_queue: deque[GameEvent] = deque()

    # ------------------------------------------------------------------
    # Subscription management
    # ------------------------------------------------------------------
    def subscribe(self, event_type: Enum, callback: Callable[[GameEvent], None]) -> None:
        """Register ``callback`` to be invoked when ``event_type`` occurs."""

        if callback not in self._listeners[event_type]:
            self._listeners[event_type].append(callback)

    def unsubscribe(self, event_type: Enum, callback: Callable[[GameEvent], None]) -> None:
        """Remove ``callback`` from ``event_type`` notifications."""

        try:
            self._listeners[event_type].remove(callback)
        except (KeyError, ValueError):
            pass

    # ------------------------------------------------------------------
    # Event emission
    # ------------------------------------------------------------------
    def emit(self, event_type: Enum, data: dict[str, Any] | None = None, source: str | None = None) -> GameEvent:
        """Create an event and queue it for later processing."""

        event = GameEvent(event_type=event_type, data=data, source=source)
        self._event_queue.append(event)
        return event

    def emit_immediate(self, event: GameEvent) -> None:
        """Dispatch ``event`` to listeners immediately."""

        for callback in list(self._listeners.get(event.event_type, [])):
            try:
                callback(event)
            except Exception as exc:  # pragma: no cover - defensive
                logger.error(f"Error in event callback: {exc}")

    # ------------------------------------------------------------------
    # Event processing
    # ------------------------------------------------------------------
    def process_events(self) -> None:
        """Dispatch all queued events in the order they were emitted."""

        while self._event_queue:
            event = self._event_queue.popleft()
            self.emit_immediate(event)

    def clear(self) -> None:
        """Remove all pending events without dispatching them."""

        self._event_queue.clear()


# Global bus instance used by the engine
event_bus = EventBus()
