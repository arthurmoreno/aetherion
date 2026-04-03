from __future__ import annotations

import asyncio
from typing import Any

import lz4.frame
import msgpack
import websockets
from websockets import State
from websockets.asyncio.server import ServerConnection

from aetherion import EntityEnum, EntityInterface
from aetherion.entities.beasts import BeastEntity, BeastEnum
from aetherion.logger import logger
from aetherion.networking.exceptions import AuthenticationError
from aetherion.networking.jwt_auth import JWTAuthenticator
from aetherion.networking.models import WebsocketActionHandler
from aetherion.world.protocols import WorldInterfaceProtocol

FPS = 30  # Default frames per second for the server


def make_ai_metadata_serializable(
    ai_metadata: dict[int, dict[str, EntityInterface | bool]],
) -> dict[int, dict[str, bytes | bool]]:
    """Convert AI metadata to a serializable format."""
    serialized_metadata = {}
    for entity_id, entity_dict in ai_metadata.items():
        encoded_entity_info = {}
        for key, value in entity_dict.items():
            if isinstance(value, EntityInterface):
                serialized_value: bytes = value.serialize()
            elif isinstance(value, bool):
                serialized_value = value
            else:
                raise TypeError(f"Unsupported type in AI metadata: {type(value)}")
            encoded_entity_info[key] = serialized_value
        serialized_metadata[entity_id] = encoded_entity_info
    return serialized_metadata


class WebSocketGameServer:
    def __init__(
        self,
        world_interface: WorldInterfaceProtocol,
        host: str = "localhost",
        port: int = 8765,
        fps: int = FPS,
        action_handlers: dict[str, WebsocketActionHandler] | None = None,
    ) -> None:
        self.host: str = host
        self.port: int = port
        self.fps: int = fps
        self.world_interface: WorldInterfaceProtocol = world_interface
        self.clients: set[ServerConnection] = set()
        self.clients_entities_map: dict[ServerConnection, list[int]] = {}
        # Streaming state
        self._client_state_queues: dict[ServerConnection, asyncio.Queue[bytes]] = {}
        self._client_stream_tasks: dict[ServerConnection, tuple[asyncio.Task, asyncio.Task]] = {}
        # Per-connection stream configuration (poll interval seconds)
        self._client_stream_cfg: dict[ServerConnection, dict[str, float]] = {}
        self._action_handlers: dict[str, WebsocketActionHandler] = {
            **self._default_action_handlers(),
            **(action_handlers or {}),
        }

    def get_entity_from_request(self, request_message: dict[str, Any]) -> EntityInterface:
        entity_id: Any | None = request_message.get("entity_id")
        if not isinstance(entity_id, int):
            raise ValueError("No entity ID found in the request message.")
        entity = self.world_interface.world.get_entity_by_id(entity_id)
        if not entity:
            raise ValueError(f"Entity with ID {entity_id} not found.")
        return entity

    def _default_action_handlers(self) -> dict[str, WebsocketActionHandler]:
        """Default action handlers shipped with the engine.

        Users are expected to inject/override handlers via the `action_handlers` constructor argument. The
        defaults are intentionally small and generic (movement, jump, etc.) and can be replaced if your game’s
        websocket protocol differs.
        """

        async def walk_left_entity(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.walk_left_entity(entity)

        async def walk_up_entity(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.walk_up_entity(entity)

        async def walk_right_entity(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.walk_right_entity(entity)

        async def walk_down_entity(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.walk_down_entity(entity)

        async def jump_entity(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.jump_entity(entity)

        async def make_entity_eat(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity = self.get_entity_from_request(request_message)
            self.world_interface.make_entity_eat(entity)

        async def ai_decisions(request_message: dict[str, Any], _ws: ServerConnection) -> None:
            entity_action_map = request_message.get("entity_action_map", {})
            statistics = request_message.get("statistics", {})
            self.world_interface.set_entity_action_map(entity_action_map, statistics)
            self.world_interface.process_nn_actions()

        async def create_player(request_message: dict[str, Any], websocket: ServerConnection) -> None:
            # Example default: many games will want to override this.
            player_start_x: int = request_message.get("player_start_x", 50)
            player_start_y: int = request_message.get("player_start_y", 50)
            player_start_z: int = request_message.get("player_start_z", 9)
            mass: int = request_message.get("mass", 80)
            max_health: int = request_message.get("max_health", 500)
            health_level: int = request_message.get("health_level", 500)
            max_speed: int = request_message.get("max_speed", 20)
            perception_area: int = request_message.get("perception_area", 20)
            z_perception_area: int = request_message.get("z_perception_area", 10)

            player: BeastEntity = BeastEntity(
                player_start_x,
                player_start_y,
                player_start_z,
                mass=mass,
                max_health=max_health,
                health_level=health_level,
                max_speed=max_speed,
                perception_area=perception_area,
                z_perception_area=z_perception_area,
            )
            entity_created = self.world_interface.world.create_entity(player)
            self.add_client_entity(websocket, entity_created.get_id())
            try:
                self.world_interface.register_connected_entity(entity_created.get_id())
            except Exception:
                pass
            response_message = {"type": "player_created", "entity_id": entity_created.get_id(), "success": True}
            await websocket.send(msgpack.packb(response_message))

        return {
            "walk_left_entity": walk_left_entity,
            "walk_up_entity": walk_up_entity,
            "walk_right_entity": walk_right_entity,
            "walk_down_entity": walk_down_entity,
            "jump_entity": jump_entity,
            "make_entity_eat": make_entity_eat,
            "ai_decisions": ai_decisions,
            "create_player": create_player,
        }

    async def process_action(self, request_message: dict[str, Any], websocket: ServerConnection) -> None:
        """Dispatch an incoming `type='action'` message to an action handler.

        The engine ships a small set of defaults for common movement-like actions, but games should treat the
        websocket action protocol as *user-owned*. Inject handlers via ``action_handlers`` on
        ``WebSocketGameServer`` / ``AuthenticatedWebSocketServer``, or pass them through
        ``WorldInterface.start_server(action_handlers=...)`` when the server is created from the world layer.
        """

        action_name_raw = request_message.get("action_name")
        action_name = str(action_name_raw) if action_name_raw is not None else None
        logger.info(f"Processing action: {action_name} for entity")

        if not action_name:
            await websocket.send(msgpack.packb({"type": "error", "message": "Missing action_name"}))
            return

        handler = self._action_handlers.get(action_name)
        if handler is None:
            await websocket.send(msgpack.packb({"type": "error", "message": f"Unknown action: {action_name}"}))
            return

        await handler(request_message, websocket)

    def add_client(self, websocket: ServerConnection) -> None:
        self.clients.add(websocket)
        if websocket not in self.clients_entities_map:
            self.clients_entities_map[websocket] = []
        logger.info(f"Client {getattr(websocket, 'remote_address', '<unknown>')} connected")

    def remove_client(self, websocket: ServerConnection) -> None:
        self.clients.discard(websocket)
        if websocket in self.clients_entities_map:
            del self.clients_entities_map[websocket]
        logger.info(f"Client {getattr(websocket, 'remote_address', '<unknown>')} disconnected")

    def add_client_entity(self, websocket: ServerConnection, entity_id: int) -> None:
        if websocket not in self.clients_entities_map:
            self.clients_entities_map[websocket] = []
        if entity_id not in self.clients_entities_map[websocket]:
            self.clients_entities_map[websocket].append(entity_id)
            logger.info(f"Added entity {entity_id} to client {getattr(websocket, 'remote_address', '<unknown>')}")

    def remove_client_entity(self, websocket: ServerConnection, entity_id: int) -> None:
        if websocket in self.clients_entities_map:
            try:
                self.clients_entities_map[websocket].remove(entity_id)
                logger.info(
                    f"Removed entity {entity_id} from client {getattr(websocket, 'remote_address', '<unknown>')}"
                )
            except ValueError:
                logger.warning(
                    f"Entity {entity_id} not found for client {getattr(websocket, 'remote_address', '<unknown>')}"
                )

    def get_client_entities(self, websocket: ServerConnection) -> list[int]:
        return self.clients_entities_map.get(websocket, [])

    async def process_ping_request(self, websocket: ServerConnection) -> None:
        await websocket.send(msgpack.packb({"type": "pong", "world_ready": True}))

    async def process_state_request(self, websocket: ServerConnection) -> None:
        if self.world_interface.last_state_response:
            await websocket.send(lz4.frame.compress(self.world_interface.last_state_response))
        else:
            await websocket.send(msgpack.packb({"type": "error", "message": "No state available"}))

    async def process_ai_metadata_request(self, websocket: ServerConnection) -> None:
        ai_metadata: dict[int, dict[str, EntityInterface | bool]] = self.world_interface.get_ai_metadata()
        encoded_ai_metadata: dict[int, dict[str, bytes | bool]] = make_ai_metadata_serializable(ai_metadata)
        ai_metadata_bytes = msgpack.packb(encoded_ai_metadata)
        await websocket.send(lz4.frame.compress(ai_metadata_bytes))

    async def process_subscribe_entities(self, request_message: dict[str, Any], websocket: ServerConnection) -> None:
        entities = request_message.get("entities", [])
        registered: list[int] = []
        try:
            if isinstance(entities, list):
                for eid in entities:
                    if isinstance(eid, int):
                        self.add_client_entity(websocket, eid)
                        registered.append(eid)
        except Exception:
            pass
        # Register with the world so run_game_loop includes player_<id> snapshots
        if registered:
            try:
                self.world_interface.register_list_of_connected_entities(registered)
            except Exception:
                pass
        response_message = {
            "type": "subscribe_entities_ack",
            "success": True,
            "count": len(self.get_client_entities(websocket)),
        }
        await websocket.send(msgpack.packb(response_message))

    async def _subscribe_state(
        self, websocket: ServerConnection, request_message: dict[str, Any] | None = None
    ) -> None:
        if websocket in self._client_stream_tasks:
            await websocket.send(
                msgpack.packb({"type": "subscribe_state_ack", "success": True, "status": "already_active"})
            )
            return

        q: asyncio.Queue[bytes] = asyncio.Queue(maxsize=1)
        self._client_state_queues[websocket] = q
        # Compute initial poll interval from request (fps or interval_ms), fallback to settings.FPS
        req_fps = None
        req_dt_ms = None
        if request_message and isinstance(request_message, dict):
            v = request_message.get("fps")
            if isinstance(v, (int, float)) and v > 0:
                req_fps = float(v)
            v = request_message.get("interval_ms")
            if isinstance(v, (int, float)) and v >= 0:
                req_dt_ms = float(v)
        poll_dt = (
            max(0.001, 1.0 / max(1.0, req_fps))
            if req_fps
            else (
                max(0.001, (req_dt_ms or 0.0) / 1000.0)
                if req_dt_ms is not None
                else max(0.001, 1.0 / max(1, int(self.fps)))
            )
        )
        self._client_stream_cfg[websocket] = {"dt": float(poll_dt)}
        last_seen_id: int | None = None

        entities_ids_with_queries = {}
        entities_ids_connection_names = {}
        for connected_entity_id in self.clients_entities_map[websocket]:
            # TODO: Fix optional queries.
            entities_ids_with_queries[connected_entity_id] = []
            entities_ids_connection_names[connected_entity_id] = f"player_{connected_entity_id}"

        async def producer() -> None:
            nonlocal last_seen_id
            try:
                while True:
                    # Offload perception gathering/encoding to thread pool to avoid blocking the event loop
                    compressed_data = await self.world_interface.get_perception_responses_async(
                        entities_ids_with_queries, entities_ids_connection_names
                    )
                    if compressed_data is not None:
                        cur_id = id(compressed_data)
                        if cur_id != last_seen_id:
                            last_seen_id = cur_id
                            try:
                                while True:
                                    q.get_nowait()
                            except asyncio.QueueEmpty:
                                pass
                            await q.put(compressed_data)
                    await asyncio.sleep(self._client_stream_cfg.get(websocket, {}).get("dt", poll_dt))
            except asyncio.CancelledError:
                return

        async def sender() -> None:
            try:
                while True:
                    buf = await q.get()
                    await websocket.send(buf)
            except asyncio.CancelledError:
                return
            except Exception:
                return

        logger.info(
            f"Subscribing {getattr(websocket, 'remote_address', '<unknown>')} "
            f"to state updates with dt={poll_dt} ({req_fps})"
        )
        task_prod = asyncio.create_task(producer())
        task_send = asyncio.create_task(sender())
        self._client_stream_tasks[websocket] = (task_prod, task_send)

        await websocket.send(
            msgpack.packb(
                {
                    "type": "subscribe_state_ack",
                    "success": True,
                    "dt": self._client_stream_cfg.get(websocket, {}).get("dt", poll_dt),
                }
            )
        )

    async def _unsubscribe_state(self, websocket: ServerConnection) -> None:
        logger.info(f"Unsubscribing {getattr(websocket, 'remote_address', '<unknown>')} from state updates")
        for entity_id in self.clients_entities_map[websocket]:
            self.world_interface.unregister_connected_entity(entity_id)
        if websocket in self._client_stream_tasks:
            for t in self._client_stream_tasks.pop(websocket):
                t.cancel()
        self._client_state_queues.pop(websocket, None)
        if websocket.state == State.CLOSED:
            return

        await websocket.send(msgpack.packb({"type": "unsubscribe_state_ack", "success": True}))

    # TODO: This must be implemented by the user, not the game engine.
    async def process_message(self, request_message: dict[str, Any], websocket: ServerConnection) -> None:
        msg_type = request_message.get("type")
        if msg_type == "ping":
            await self.process_ping_request(websocket)
        elif msg_type == "action_and_state_request":
            client_id = request_message.get("client_id")
            if client_id == "Aeolus":
                # Find Aeolus entity
                aeolus_entities = self.world_interface.world.get_entities_by_type(
                    EntityEnum.BEAST.value, BeastEnum.AEOLUS.value
                )
                if aeolus_entities:
                    aeolus_id = list(aeolus_entities.keys())[0]
                    self.add_client_entity(websocket, aeolus_id)
                    self.world_interface.register_connected_entity(aeolus_id, name="Aeolus")
            await self.process_state_request(websocket)
        elif msg_type == "ai_metadata":
            await self.process_ai_metadata_request(websocket)
        elif msg_type == "subscribe_entities":
            await self.process_subscribe_entities(request_message, websocket)
        elif msg_type == "subscribe_state":
            await self._subscribe_state(websocket, request_message)
        elif msg_type == "unsubscribe_state":
            await self._unsubscribe_state(websocket)
        elif msg_type == "set_stream_rate":
            # Update per-connection poll interval while streaming
            fps = request_message.get("fps")
            interval_ms = request_message.get("interval_ms")
            if websocket not in self._client_stream_cfg:
                self._client_stream_cfg[websocket] = {}
            new_dt = None
            if isinstance(fps, (int, float)) and fps > 0:
                new_dt = 1.0 / float(fps)
            elif isinstance(interval_ms, (int, float)) and interval_ms >= 0:
                new_dt = float(interval_ms) / 1000.0
            if new_dt is not None:
                self._client_stream_cfg[websocket]["dt"] = max(0.001, new_dt)
            await websocket.send(
                msgpack.packb(
                    {
                        "type": "set_stream_rate_ack",
                        "success": True,
                        "dt": self._client_stream_cfg[websocket].get("dt"),
                    }
                )
            )
        elif msg_type == "action":
            await self.process_action(request_message, websocket)
        else:
            await websocket.send(msgpack.packb({"type": "error", "message": f"Unknown type: {msg_type}"}))

    async def handle_client(self, websocket: ServerConnection) -> None:
        self.add_client(websocket)
        try:
            async for data in websocket:
                try:
                    request_message = msgpack.unpackb(data, raw=False, strict_map_key=False)
                    logger.debug(
                        f"Received message: {request_message} from {getattr(websocket, 'remote_address', '<unknown>')}"
                    )
                    await self.process_message(request_message, websocket)
                except (msgpack.exceptions.ExtraData, msgpack.exceptions.UnpackException) as e:
                    logger.error(
                        f"Failed to unpack message from {getattr(websocket, 'remote_address', '<unknown>')}: {e}"
                    )
                except Exception as e:
                    logger.error(
                        f"Error processing message from {getattr(websocket, 'remote_address', '<unknown>')}: {e}"
                    )
        except websockets.ConnectionClosed:
            logger.info(f"Client {getattr(websocket, 'remote_address', '<unknown>')} disconnected")
        except Exception as e:
            logger.error(f"Unexpected error handling client {getattr(websocket, 'remote_address', '<unknown>')}: {e}")
        finally:
            if websocket in self._client_stream_tasks:
                for t in self._client_stream_tasks.pop(websocket):
                    t.cancel()
            self._client_state_queues.pop(websocket, None)
            self._client_stream_cfg.pop(websocket, None)
            await self._unsubscribe_state(websocket)
            self.remove_client(websocket)

    async def start(self) -> None:
        logger.info(f"Starting server on {self.host}:{self.port}")
        async with websockets.serve(self.handle_client, self.host, self.port):
            await asyncio.Future()  # Run forever


class AuthenticatedWebSocketServer(WebSocketGameServer):
    """Extended WebSocket server with JWT authentication."""

    def __init__(
        self,
        world_interface: WorldInterfaceProtocol,
        host: str = "localhost",
        port: int = 8765,
        jwt_secret: str = "dev-secret-key",
        fps: int = FPS,
        action_handlers: dict[str, WebsocketActionHandler] | None = None,
    ) -> None:
        super().__init__(world_interface, host, port, fps=fps, action_handlers=action_handlers)
        self.authenticator: JWTAuthenticator = JWTAuthenticator(jwt_secret)

        # Store authenticated user sessions
        self.authenticated_sessions: dict[Any, dict[str, str | int | float]] = {}

    async def send_error_response(
        self, websocket: Any, error_message: str, error_type: str = "authentication_error"
    ) -> None:
        response_message = {"type": "error", "error_type": error_type, "message": error_message, "success": False}
        response_data = msgpack.packb(response_message)
        await websocket.send(response_data)

    async def authenticate_request(
        self, request_message: dict[str, Any], websocket: Any
    ) -> dict[str, str | int | float] | None:
        token = request_message.get("token")
        if not token:
            logger.error("No authentication token provided in the request. From: {websocket.remote_address}")
            await self.send_error_response(websocket, "No authentication token provided", "authentication_error")
            return None

        try:
            user_claims = self.authenticator.validate_token(token)
            return user_claims
        except AuthenticationError as e:
            await self.send_error_response(websocket, str(e))
            return None

    async def check_action_authorization(
        self, action_name: str, user_claims: dict[str, str | int | float], websocket: Any
    ) -> bool:
        permission_level = self.authenticator.get_user_permission_level(user_claims)
        if not self.authenticator.check_action_permission(action_name, permission_level):
            logger.error(f"User {user_claims.get('user_id')} is not authorized to perform action '{action_name}'.")
            await self.send_error_response(
                websocket,
                f"Insufficient permissions for action '{action_name}'. Required: {permission_level.value}",
                "authorization_error",
            )
            return False
        return True

    async def process_authenticated_message(self, request_message: dict[str, Any], websocket: Any) -> None:
        user_claims = await self.authenticate_request(request_message, websocket)
        if user_claims is None:
            return

        self.authenticated_sessions[websocket] = user_claims

        message_type = request_message.get("type")
        action_name_raw = request_message.get("action_name", message_type)
        action_name = str(action_name_raw) if action_name_raw is not None else str(message_type)

        if not await self.check_action_authorization(action_name, user_claims, websocket):
            return

        user_id = user_claims.get("user_id", "unknown")
        role = user_claims.get("role", "unknown")
        logger.debug(f"Authenticated user {user_id} with role {role} performing action: {action_name}")

        await super().process_message(request_message, websocket)

    async def process_message(self, request_message: dict[str, Any], websocket: Any) -> None:
        await self.process_authenticated_message(request_message, websocket)

    def remove_client(self, websocket: Any) -> None:
        super().remove_client(websocket)
        self.authenticated_sessions.pop(websocket, None)

    def get_authenticated_user(self, websocket: Any) -> dict[str, str | int | float] | None:
        return self.authenticated_sessions.get(websocket)
