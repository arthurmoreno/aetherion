from __future__ import annotations

import queue
import threading
import time
from abc import ABC, abstractmethod
from concurrent.futures import Future, ThreadPoolExecutor
from typing import TYPE_CHECKING, Any

try:
    from typing import override
except ImportError:
    from typing_extensions import override

import lz4.frame
import msgpack
from logger import logger
from websockets.sync.client import ClientConnection, connect

from aetherion.entities.beasts import BeastEntity
from aetherion.networking.exceptions import AuthenticationError
from aetherion.networking.jwt import MockJWTProvider
from aetherion.networking.receiver import WebSocketReceiver
from aetherion.world.constants import WorldInstanceTypes

if TYPE_CHECKING:
    from aetherion.world.interface import WorldInterface

from aetherion import BaseEntity, EntityInterface, GameClock, PerceptionResponseFlatB, SharedState


class BeastConnection(ABC):
    """Abstract base class for connections to a world interface."""

    def __init__(self, name: str | None = None, *args: Any, **kwargs: Any) -> None:
        self.name: str | None = name
        self.connected: bool = False
        self.last_state_response: PerceptionResponseFlatB | None = None
        self.last_state_thread: threading.Thread | None = None
        self.last_state_response_event: threading.Event = threading.Event()
        self.entity: EntityInterface | None = None

    @abstractmethod
    def connect(
        self, connection_type: WorldInstanceTypes, world_instance: WorldInterface | None = None, pipe: Any = None
    ) -> bool:
        """Connect to the world interface."""
        pass

    @property
    @abstractmethod
    def ticks(self) -> int | None:
        """Get the current world ticks."""
        pass

    @property
    @abstractmethod
    def server_online(self) -> bool:
        """Check if the server is online."""
        pass

    @property
    @abstractmethod
    def ready_to_render_world(self) -> bool:
        """Check if the server is online."""
        pass

    @abstractmethod
    def request_world_state(self) -> None:
        """Execute a request to get the next world state."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    def wait_for_next_world_state(self, shared_state: SharedState | None = None) -> PerceptionResponseFlatB | None:
        """Wait for the next world state."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    # Direction change methods (currently no-op in both implementations)
    def change_direction_left(self):
        pass

    def change_direction_up(self):
        pass

    def change_direction_right(self):
        pass

    def change_direction_down(self):
        pass

    # Abstract action methods
    @abstractmethod
    def walk_left(self):
        pass

    @abstractmethod
    def walk_up(self):
        pass

    @abstractmethod
    def walk_right(self):
        pass

    @abstractmethod
    def walk_down(self):
        pass

    @abstractmethod
    def jump(self):
        pass

    @abstractmethod
    def walk_left_up(self):
        pass

    @abstractmethod
    def walk_left_down(self):
        pass

    @abstractmethod
    def walk_right_up(self):
        pass

    @abstractmethod
    def walk_right_down(self):
        pass

    @abstractmethod
    def walk_left_jump(self):
        pass

    @abstractmethod
    def walk_right_jump(self):
        pass

    @abstractmethod
    def walk_up_jump(self):
        pass

    @abstractmethod
    def walk_down_jump(self):
        pass

    @abstractmethod
    def make_entity_take_item(self, shared_state: dict[str, Any] | None = None):
        pass

    @abstractmethod
    def make_entity_use_item(self, item_slot: int = 0, shared_state: dict[str, Any] | None = None):
        pass

    @abstractmethod
    def eat(self):
        pass

    @abstractmethod
    def set_entity_to_debug(self, entity_id: int):
        pass


class SynchronousBeastConnection(BeastConnection):
    """Handles synchronous connections using direct world instance calls."""

    def __init__(self, name: str | None = None, *args, **kwargs) -> None:
        super().__init__(name, *args, **kwargs)
        self.world_instance: WorldInterface | None = None

    def connect(
        self,
        connection_type: WorldInstanceTypes,
        world_instance: WorldInterface | None = None,
        entity: BaseEntity | None = None,
        pipe: Any = None,
    ) -> bool:
        if connection_type != WorldInstanceTypes.SYNCHRONOUS:
            raise ValueError("SynchronousBeastConnection only supports SYNCHRONOUS connection type")
        if world_instance is None:
            raise ValueError("for synchronous connection you need to provide world_instance parameter")
        self.world_instance = world_instance
        self.entity = entity
        self.connected = True
        return self.connected

    @property
    @override
    def ticks(self) -> int | None:
        if self.world_instance is None:
            return None
        return self.world_instance.ticks

    @property
    @override
    def server_online(self) -> bool:
        if self.world_instance is None:
            return False
        return self.world_instance.ready or False

    @property
    @override
    def ready_to_render_world(self) -> bool:
        """Check if the server is online."""
        world_ticks = self.ticks
        if world_ticks is None:
            return False
        return self.world_instance.world is not None and int(world_ticks) >= 3

    @override
    def request_world_state(self) -> None:
        """Execute a request to get the next world state."""
        pass

    @override
    def wait_for_next_world_state(self, shared_state: SharedState | None = None) -> PerceptionResponseFlatB | None:
        if self.world_instance is None:
            return None

        optional_queries = {}
        optional_queries[self.name] = [command for command in shared_state.commands if "query" in command.get("type")]

        for command in shared_state.commands:
            if command.get("type") == "get_ai_statistics":
                optional_queries[self.name].append(command)

        for command in shared_state.commands:
            if command.get("type") == "get_ai_statistics":
                optional_queries[self.name].append(command)

        entities_ids_with_queries = {}
        entities_ids_connection_names = {}
        if isinstance(self.entity, BeastEntity):
            connected_entity_id: int | None = self.entity.entity_id
        elif isinstance(self.entity, EntityInterface):
            connected_entity_id = self.entity.get_entity_id()
        else:
            raise ValueError("Unsupported entity type")

        entities_ids_with_queries[connected_entity_id] = optional_queries[self.name]
        entities_ids_connection_names[connected_entity_id] = self.name

        self.last_state_response = msgpack.unpackb(
            self.world_instance.get_perception_responses(entities_ids_with_queries, entities_ids_connection_names),
            raw=False,
            strict_map_key=False,
        )
        beast_response = self.last_state_response.get(self.name)
        if beast_response:
            flatb_accessor = PerceptionResponseFlatB(beast_response)
            self.entity = flatb_accessor.getEntity()
            world_ticks = flatb_accessor.get_ticks()
            if shared_state.game_clock is None:
                shared_state.game_clock = GameClock()

            shared_state.game_clock.set_ticks(world_ticks)
            return flatb_accessor

    def walk_left(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_left_entity(self.entity)

    def walk_up(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_up_entity(self.entity)

    def walk_right(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_right_entity(self.entity)

    def walk_down(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_down_entity(self.entity)

    def jump(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.jump_entity(self.entity)

    def walk_left_up(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_left_up_entity(self.entity)

    def walk_left_down(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_left_down_entity(self.entity)

    def walk_right_up(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_right_up_entity(self.entity)

    def walk_right_down(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_right_down_entity(self.entity)

    def walk_left_jump(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_left_jump_entity(self.entity)

    def walk_right_jump(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_right_jump_entity(self.entity)

    def walk_up_jump(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_up_jump_entity(self.entity)

    def walk_down_jump(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.walk_down_jump_entity(self.entity)

    def make_entity_take_item(self, shared_state: dict = {}):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.make_entity_take_item(
                self.entity, shared_state.get("hovered_entity"), shared_state.get("selected_entity")
            )

    def make_entity_use_item(self, item_slot: int = 0, shared_state: SharedState | None = None):
        if self.world_instance and self.entity and shared_state:
            self.last_state_response = self.world_instance.make_entity_use_item(
                self.entity, item_slot, shared_state.hovered_entity, shared_state.selected_entity
            )

    def eat(self):
        if self.world_instance and self.entity:
            self.last_state_response = self.world_instance.make_entity_eat(self.entity)

    def set_entity_to_debug(self, entity_id):
        if self.world_instance:
            self.world_instance.set_entity_to_debug(entity_id)


class ServerBeastConnection(BeastConnection):
    """Handles server connections using WebSocket communication.

    Adds a low-latency push-based stream (`subscribe_state`) and keeps polling as fallback.
    """

    last_state_response: PerceptionResponseFlatB | None = None
    _executor: ThreadPoolExecutor | None = None
    _last_state_future: Future | None = None

    def __init__(self, name: str | None = None, *args, **kwargs) -> None:
        super().__init__(name, *args, **kwargs)
        self._server_online = False
        self.world_ticks: int | None = None
        self.websocket = None
        self.jwt_provider: MockJWTProvider = MockJWTProvider()
        self.response_lock = threading.Lock()
        self.recv_lock = threading.Lock()
        self._executor = ThreadPoolExecutor(max_workers=16, thread_name_prefix="ws-world-state")
        self._last_state_future = None
        # Queues consumed by the unified receiver
        self.state_queue: queue.Queue[bytes] = queue.Queue(maxsize=1)
        self.control_queue: queue.Queue[dict] = queue.Queue(maxsize=32)
        self.error_queue: queue.Queue[dict] = queue.Queue(maxsize=32)
        self._receiver: WebSocketReceiver | None = None
        # Streaming flag
        self.streaming_enabled: bool = False

    @override
    def connect(
        self, connection_type: WorldInstanceTypes, world_host: str = "localhost", world_port: str = "8765"
    ) -> bool:
        if connection_type != WorldInstanceTypes.SERVER:
            raise ValueError("ServerBeastConnection only supports SERVER connection type")
        uri = f"ws://{world_host}:{world_port}"
        try:
            self.websocket: ClientConnection = connect(uri)
            self.connected = True
            print(f"Connected to the WebSocket server at {uri}")
            # Start unified receiver thread to own all recv() calls
            self._receiver = WebSocketReceiver(
                websocket=self.websocket,
                state_queue=self.state_queue,
                control_queue=self.control_queue,
                error_queue=self.error_queue,
            )
            self._receiver.start()
            return self.connected
        except Exception as e:
            print(f"Failed to connect to WebSocket server: {e}")
            self.connected = False
            return self.connected

    @property
    @override
    def ticks(self) -> int | None:
        return self.world_ticks

    @property
    @override
    def server_online(self) -> bool:
        if not self._server_online and self.websocket is not None:
            ping_message = {
                "token": self.jwt_provider.get_user_token(),
                "type": "ping",
                "client_id": self.name,
            }
            self.websocket.send(msgpack.packb(ping_message))
            deadline = time.time() + 0.5
            while time.time() < deadline:
                try:
                    msg = self.control_queue.get(timeout=max(0.0, deadline - time.time()))
                except queue.Empty:
                    break
                if (
                    isinstance(msg, dict)
                    and msg.get("type") == "error"
                    and msg.get("error_type") == "authentication_error"
                ):
                    raise AuthenticationError(msg.get("message", "Authentication error occurred."))
                if isinstance(msg, dict) and msg.get("type") == "pong":
                    world_ready = msg.get("world_ready", False)
                    logger.debug(f"World ready status: {world_ready}")
                    self._server_online = world_ready
                    return world_ready
            return False
        return self._server_online

    @property
    @override
    def ready_to_render_world(self) -> bool:
        world_ticks: int | None = self.ticks
        if world_ticks is None:
            return False
        return self.server_online and int(world_ticks) >= 3

    def subscribe_to_entities(self, entities: list[int]) -> None:
        if not self.websocket or not self.connected:
            return
        request_message = {
            "token": self.jwt_provider.get_user_token(),
            "type": "subscribe_entities",
            "client_type": self.name,
            "client_id": self.name,
            "entities": entities,
        }
        self.websocket.send(msgpack.packb(request_message))
        # Optionally wait briefly for ack (non-blocking to render loop)
        deadline = time.time() + 0.25
        while time.time() < deadline:
            try:
                msg = self.control_queue.get(timeout=max(0.0, deadline - time.time()))
            except queue.Empty:
                break
            if isinstance(msg, dict) and msg.get("type") == "subscribe_entities_ack":
                break

    def enable_state_stream(self, fps: int | float | None = 20) -> None:
        if not self.websocket or not self.connected or self.streaming_enabled:
            return
        request_message: dict[str, Any] = {
            "token": self.jwt_provider.get_user_token(),
            "type": "subscribe_state",
            "client_type": self.name,
            "client_id": self.name,
        }
        if isinstance(fps, (int, float)) and fps > 0:
            request_message["fps"] = float(fps)
        self.websocket.send(msgpack.packb(request_message))
        self.streaming_enabled = True

    def set_stream_rate(self, fps: int | float | None = None, interval_ms: int | float | None = None) -> None:
        if not self.websocket or not self.connected:
            return
        req: dict[str, Any] = {
            "token": self.jwt_provider.get_user_token(),
            "type": "set_stream_rate",
            "client_type": self.name,
            "client_id": self.name,
        }
        if isinstance(fps, (int, float)) and fps > 0:
            req["fps"] = float(fps)
        if isinstance(interval_ms, (int, float)) and interval_ms >= 0:
            req["interval_ms"] = float(interval_ms)
        self.websocket.send(msgpack.packb(req))

    @override
    def request_world_state(self):
        if not self.websocket or not self.connected:
            return
        request_message = {
            "token": self.jwt_provider.get_user_token(),
            "type": "action_and_state_request",
            "client_type": self.name,
            "client_id": self.name,
            "actions": None,
        }
        self.websocket.send(msgpack.packb(request_message))

    @override
    def wait_for_next_world_state(self, shared_state: dict | None = None) -> PerceptionResponseFlatB | None:
        # Non-blocking: consume latest state if present, else keep last
        try:
            comp_buf: bytes = self.state_queue.get_nowait()
        except queue.Empty:
            if not self.streaming_enabled:
                self.request_world_state()
            return self.last_state_response
        try:
            decomp = lz4.frame.decompress(comp_buf)
            full_response = msgpack.unpackb(decomp, raw=False, strict_map_key=False)
            payload = full_response.get(self.name) if isinstance(full_response, dict) else None
            if isinstance(payload, (bytes, bytearray)):
                with self.response_lock:
                    flatb_accessor: PerceptionResponseFlatB = PerceptionResponseFlatB(payload)
                    self.last_state_response = flatb_accessor
                    ent = flatb_accessor.getEntity()
                    if ent is not None:
                        self.entity = ent
                    self.world_ticks = flatb_accessor.get_ticks()
                self.last_state_response_event.set()
        except Exception as e:
            logger.error(f"Error decoding state buffer: {e}")
        return self.last_state_response

    def _send_action_message(self, action_name: str):
        if not self.websocket or not self.entity:
            return
        request_message = {
            "token": self.jwt_provider.get_user_token(),
            "type": "action",
            "client_type": self.name,
            "client_id": self.name,
            "action_name": action_name,
            "entity_id": self.entity.get_entity_id(),
        }
        self.websocket.send(msgpack.packb(request_message))

    def walk_left(self):
        self._send_action_message("walk_left_entity")

    def walk_up(self):
        self._send_action_message("walk_up_entity")

    def walk_right(self):
        self._send_action_message("walk_right_entity")

    def walk_down(self):
        self._send_action_message("walk_down_entity")

    def jump(self):
        self._send_action_message("jump_entity")

    def walk_left_up(self):
        self._send_action_message("walk_left_up_entity")

    def walk_left_down(self):
        self._send_action_message("walk_left_down_entity")

    def walk_right_up(self):
        self._send_action_message("walk_right_up_entity")

    def walk_right_down(self):
        self._send_action_message("walk_right_down_entity")

    def walk_left_jump(self):
        self._send_action_message("walk_left_jump_entity")

    def walk_right_jump(self):
        self._send_action_message("walk_right_jump_entity")

    def walk_up_jump(self):
        self._send_action_message("walk_up_jump_entity")

    def walk_down_jump(self):
        self._send_action_message("walk_down_jump_entity")

    def make_entity_take_item(self, shared_state: dict = {}):
        raise NotImplementedError()

    def make_entity_use_item(self, item_slot: int = 0, shared_state: dict = {}):
        raise NotImplementedError()

    def eat(self):
        self._send_action_message("make_entity_eat")

    def set_entity_to_debug(self, entity_id):
        pass
