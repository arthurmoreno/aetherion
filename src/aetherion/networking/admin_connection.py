from time import sleep

import msgpack
from websockets.sync.client import connect

from aetherion.logger import logger
from aetherion.networking.jwt import MockJWTProvider
from aetherion.world.constants import WorldInstanceTypes


class ServerAdminConnection:
    """Handles server AI process management using WebSocket communication."""

    def __init__(self, name: str | None = None, model_name: str = "gpt2", *args, **kwargs) -> None:
        self.name = name if name else "ServerAdminConnection"
        self.websocket = None
        self.connected = False
        self.connection_type = None
        self._server_online = False
        self.ai_metadata = {}

        self.jwt_provider: MockJWTProvider = MockJWTProvider()

    def connect(self, connection_type: WorldInstanceTypes | None = None, world_instance=None, pipe=None) -> bool:
        self.connection_type = connection_type
        if self.connection_type != WorldInstanceTypes.SERVER:
            raise ValueError("ServerAdminConnection only supports SERVER connection type")

        uri = "ws://168.119.102.52:5202"
        while not self.connected:
            try:
                self.websocket = connect(uri)
                self.connected = True
                print(f"Connected to the WebSocket server at {uri}")
            except Exception as e:
                print(f"Failed to connect to WebSocket server: {e}")
                sleep(2)
                self.connected = False
        return self.connected

    @property
    def server_online(self) -> bool:
        if not self._server_online and self.websocket is not None:
            # Create a "ping" message
            ping_message = {
                "type": "ping",
                "client_id": self.name,  # Your name or client identifier
            }
            packed_message = msgpack.packb(ping_message)

            self.websocket.send(packed_message)
            message = self.websocket.recv()
            response = msgpack.unpackb(message, raw=False, strict_map_key=False)

            if response.get("type") == "pong":
                world_ready = response.get("world_ready", False)
                logger.debug(f"World ready status: {world_ready}")
                self._server_online = world_ready
                return world_ready
            return False
        else:
            return self._server_online

    def create_entity(self):
        if self.websocket is None:
            return
        metadata_message = {
            "token": self.jwt_provider.create_mock_token("admin", "admin"),
            "type": "action",
            "action_name": "create_player",
            "player_start_x": 50,
            "player_start_y": 50,
            "player_start_z": 9,
            "mass": 80,
            "max_health": 500,
            "health_level": 500,
            "max_speed": 20,
            "perception_area": 20,
            "z_perception_area": 10,
            "client_id": self.name,
        }
        packed_message = msgpack.packb(metadata_message)
        self.websocket.send(packed_message)
        packed_encoded_response = self.websocket.recv()
        server_response: dict[str, str | int | bool] = msgpack.unpackb(packed_encoded_response)

        return server_response
