from __future__ import annotations

import asyncio
import os
import time
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor

# from threading import Thread
from time import perf_counter
from typing import TYPE_CHECKING, cast

import lz4.frame
import msgpack

from aetherion.entities.beasts import BeastEnum
from aetherion.logger import logger
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.models import AIEntityMetadataResponse, AIMetadataResponse
from aetherion.world.state_manager import create_perception_multithread

if TYPE_CHECKING:
    from aetherion.networking.websocket_server import AuthenticatedWebSocketServer

from aetherion import BaseEntity, DirectionEnum, EntityEnum, EntityInterface, World


class WorldInterface:
    """Manages different ways to instantiate and manage a world instance."""

    world: World
    server: AuthenticatedWebSocketServer | None = None
    server_task: asyncio.Task[None] | None = None
    # player: BaseEntity | None = None
    _ready: bool = False
    _connected_entities: set[int] = set()
    _connected_entity_names: dict[int, str] = {}
    _entities_optional_queries: dict[int, list[str]] = {}
    _executor: ThreadPoolExecutor | None = None
    entity_action_map: dict[int, object]
    entity_last_action_map: dict[int, float]
    last_state_response: bytes | None

    _start_world: Callable[[], tuple[World, BaseEntity]] | None = None

    def __init__(self, world_instance_type: WorldInstanceTypes, world: World, fps: int = 30) -> None:
        self.connection_type: WorldInstanceTypes = world_instance_type
        self.world = world
        self.fps: int = fps
        # if self.connection_type == WorldInstanceTypes.SYNCHRONOUS:
        # Create and start the thread, targetting the load_world method

        # To debug the load_world method:
        # self.load_world()
        # self._start_world = start_world
        # self.world_thread = Thread(target=self.load_world)
        # self.world_thread.start()

        # if self.connection_type == WorldInstanceTypes.SERVER:
        #     self.load_world()
        self.ai_metadata_response: AIMetadataResponse | None = None
        self.entity_action_map = {}
        self.entity_last_action_map = {}

        self.last_state_response = None
        # Thread pool to offload CPU-heavy perception encoding without blocking the event loop
        # Size heuristic: cores + 2, but at least 2
        max_workers: int = max(2, (os.cpu_count() or 1) + 2)
        self._executor = ThreadPoolExecutor(
            max_workers=max_workers,
            thread_name_prefix="lifesim-perception",
        )

    @property
    def ticks(self) -> int:
        """Getter for the number of ticks."""
        if self.world:
            return self.world.game_clock.get_ticks()
        return 0

    @property
    def ready(self) -> bool:
        # Simplified readiness check; adapt if thread-based start is reintroduced
        if self.connection_type == WorldInstanceTypes.SERVER:
            return self._ready
        return True

    def check_world(self) -> None:
        if self.world is None:
            raise RuntimeError("World is not initialized.")

    def get_ai_metadata(self) -> AIMetadataResponse | None:
        if self.world:
            beast_entities: dict[int, EntityInterface] = cast(
                dict[int, EntityInterface],
                self.world.get_entities_by_type(EntityEnum.BEAST.value, BeastEnum.SQUIRREL.value),
            )
            god_entities = self.world.get_entities_by_type(EntityEnum.BEAST.value, BeastEnum.AEOLUS.value)

            # If the gods are added, the process get's stuck...
            beast_entities.update(god_entities)

            dict_ai_metadata: dict[int, AIEntityMetadataResponse] = {}
            for ai_entity_id, ai_entity in beast_entities.items():
                if self.ai_metadata_response and ai_entity_id not in self.ai_metadata_response.ai_metadata:
                    dict_ai_metadata[ai_entity_id] = AIEntityMetadataResponse(
                        entity_interface=ai_entity, brain_created=False
                    )
                elif self.ai_metadata_response:
                    dict_ai_metadata[ai_entity_id] = AIEntityMetadataResponse(
                        entity_interface=ai_entity,
                        brain_created=self.ai_metadata_response.ai_metadata[ai_entity_id].brain_created,
                    )
                else:
                    dict_ai_metadata[ai_entity_id] = AIEntityMetadataResponse(
                        entity_interface=ai_entity, brain_created=False
                    )
            ai_metadata: AIMetadataResponse = AIMetadataResponse(ai_metadata=dict_ai_metadata)
            self.ai_metadata_response = ai_metadata
            return ai_metadata

        return None

    def set_entity_action_map(self, entity_action_map: dict[int, object], statistics: dict[str, float]):
        # TODO: Make this resilient in the case that the entity_action_map are not being set for many ticks
        self.entity_action_map = entity_action_map
        float_timestamp: float = time.time()
        timestamp = int(float_timestamp)

        if not self.entity_last_action_map:
            self.entity_last_action_map = {entity_id: float_timestamp for entity_id in entity_action_map.keys()}
        else:
            for entity_id in entity_action_map.keys():
                if entity_id not in self.entity_last_action_map:
                    self.entity_last_action_map[entity_id] = float_timestamp
                else:
                    self.entity_last_action_map[entity_id] = max(
                        self.entity_last_action_map[entity_id], float_timestamp
                    )

        # self.put_time_series(timestamp, len(entity_action_map), series_name="entity_action_map_length")
        for statistic, value in statistics.items():
            logger.debug(f"==================>>>>>>> {statistic}: {value}")
            self.put_time_series(timestamp, value, series_name=statistic)

    # TODO: This is potentially API user facing implementation. Remove to a a Lifesim implementation!
    def process_nn_actions(self):
        """Process the actions received from the neural network for each entity."""
        entities_to_remove: list[int] = []
        for entity_id, nn_action in self.entity_action_map.items():
            try:
                entity: EntityInterface = self.world.get_entity_by_id(entity_id)
            except RuntimeError as exc:
                logger.error(f"[WorldInterface] Entity with ID {entity_id} not found in the world. Exception: {exc}")
                entities_to_remove.append(entity_id)
                continue

            if entity.get_entity_type().main_type == 2:
                nn_action = int(nn_action)
                if nn_action == 0:
                    pass
                elif nn_action >= 1 and nn_action <= 6:
                    try:
                        if nn_action == 1:
                            self.walk_up_entity(entity)
                        if nn_action == 2:
                            self.walk_right_entity(entity)
                        if nn_action == 3:
                            self.walk_down_entity(entity)
                        if nn_action == 4:
                            self.walk_left_entity(entity)
                        if nn_action == 5:
                            # TODO: Multiple input actions (from AI) are not supported yet (Setting default values)
                            self.make_entity_take_item(entity, None, None)
                        if nn_action == 6:
                            # TODO: Multiple input actions (from AI) are not supported yet (Setting default values)
                            self.make_entity_use_item(entity, 0, None, None)
                    except RuntimeError as exc:
                        logger.error(
                            f"[WorldInterface] Error processing action {nn_action} for entity ID {entity_id}. Exception: {exc}"
                        )
                        entities_to_remove.append(entity_id)

        for entity_id in entities_to_remove:
            if entity_id in self.entity_action_map:
                del self.entity_action_map[entity_id]

        current_time: float = time.time()
        if self.ai_metadata_response is not None:
            for entity_id, nn_action in self.ai_metadata_response.ai_metadata.items():
                if (
                    entity_id in self.entity_last_action_map
                    and current_time - self.entity_last_action_map[entity_id] > 30
                ):
                    log_message: str = (
                        f"[WorldInterface] Entity {entity_id} has not performed an action in the last 30 seconds. "
                        f"Last action time: {self.entity_last_action_map[entity_id]}"
                    )
                    logger.debug(log_message)

    def start_server(self):
        self.server = AuthenticatedWebSocketServer(world_interface=self, host="0.0.0.0", port=8765, fps=self.fps)

        # Start the WebSocket server as a background task
        self.server_task = asyncio.create_task(self.server.start())

    def update_world(self):
        """
        To update the world and get creatures perceptions.
        """
        self.check_world()
        self.world.update()

    # def get_next_world_state(self, optional_queries={}):
    #     self.check_world()
    #     # TODO: Now this needs to be a list of ids to gather the perception responses for.
    #     response = update_world(self, self.player, optional_queries)

    #     encoded_state = msgpack.packb(response)
    #     self.last_state_response = encoded_state

    #     return encoded_state

    def register_connected_entity(self, entity_id: int, name: str | None = None):
        self._connected_entities.add(entity_id)
        if name:
            self._connected_entity_names[entity_id] = name

    def register_list_of_connected_entities(self, entity_ids: list[int]):
        self._connected_entities.update(entity_ids)

    def unregister_connected_entity(self, entity_id: int):
        self._connected_entities.discard(entity_id)
        self._connected_entity_names.pop(entity_id, None)

    def unregister_list_of_connected_entities(self, entity_ids: list[int]):
        self._connected_entities.difference_update(set(entity_ids))

    def get_perception_responses(self, entities_ids_with_queries, entities_ids_connection_names):
        self.check_world()

        response = create_perception_multithread(self.world, entities_ids_with_queries, entities_ids_connection_names)

        encoded_state = msgpack.packb(response)
        # TODO: This maybe this should be moved from the responsability of the world interface.
        if self.connection_type == WorldInstanceTypes.SYNCHRONOUS:
            has_empty_value = any([True for key, value in response.items() if len(value) == 0]) if response else False
            if has_empty_value:
                logger.error("Perception response has empty values")
            self.last_state_response = encoded_state
            return encoded_state
        else:
            return lz4.frame.compress(encoded_state)

    async def get_perception_responses_async(self, entities_ids_with_queries, entities_ids_connection_names):
        """Run get_perception_responses in the dedicated thread pool.

        Returns the encoded state bytes and updates self.last_state_response.
        """
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(
            self._executor,
            self.get_perception_responses,
            entities_ids_with_queries,
            entities_ids_connection_names,
        )

    async def run_game_loop(self):
        frame_time = 1 / self.fps  # Target time per frame (30 FPS)

        while True:
            logger.debug("Beginning of game loop")
            # Start the timer
            start_time = perf_counter()

            # Execute your game logic
            self.update_world()
            # TODO: Needs support receiving a list of connected entities to get their perceptions.

            entities_ids_with_queries = {}
            entities_ids_connection_names = {}
            for connected_entity_id in self._connected_entities:
                # TODO: Fix optional queries.
                entities_ids_with_queries[connected_entity_id] = []
                name = self._connected_entity_names.get(connected_entity_id, f"player_{connected_entity_id}")
                entities_ids_connection_names[connected_entity_id] = name

            # Add all beast for gathering state
            beast_ids = self.world.get_entity_ids_by_type(EntityEnum.BEAST.value, BeastEnum.SQUIRREL.value)
            for b_id in beast_ids:
                entities_ids_with_queries[b_id] = []
                entities_ids_connection_names[b_id] = f"beast_brain_{b_id}"

            # Offload perception gathering/encoding to thread pool to avoid blocking the event loop
            _ = await self.get_perception_responses_async(entities_ids_with_queries, entities_ids_connection_names)

            execution_time = perf_counter() - start_time

            # Calculate the remaining time to sleep to maintain 30 FPS
            sleep_time = frame_time - execution_time
            if sleep_time > 0:
                await asyncio.sleep(sleep_time)

            logger.debug(f"execution_time: {execution_time}")

    def walk_left_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.LEFT]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_up_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.UP]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_right_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.RIGHT]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_down_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.DOWN]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def jump_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.UPWARD]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_left_up_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.LEFT, DirectionEnum.UP]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_left_down_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.LEFT, DirectionEnum.DOWN]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_right_up_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.RIGHT, DirectionEnum.UP]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_right_down_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.RIGHT, DirectionEnum.DOWN]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_left_jump_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.LEFT, DirectionEnum.UPWARD]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_right_jump_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.RIGHT, DirectionEnum.UPWARD]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_up_jump_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.UP, DirectionEnum.UPWARD]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def walk_down_jump_entity(self, entity_interface: EntityInterface):
        self.check_world()
        directions: list[DirectionEnum] = [DirectionEnum.DOWN, DirectionEnum.UPWARD]
        self.world.dispatch_move_entity_event_by_id(entity_interface.get_entity_id(), directions)

    def make_entity_take_item(
        self, entity_interface: EntityInterface, hovered_entity: int | None, selected_entity: int | None
    ):
        self.check_world()
        if hovered_entity is None:
            hovered_entity = -1
        if selected_entity is None:
            selected_entity = -1
        self.world.dispatch_take_item_event_by_id(entity_interface.get_entity_id(), hovered_entity, selected_entity)

    def make_entity_use_item(self, entity_interface, item_slot, hovered_entity, selected_entity):
        self.check_world()
        if hovered_entity is None:
            hovered_entity = -1
        if selected_entity is None:
            selected_entity = -1
        self.world.dispatch_use_item_event_by_id(
            entity_interface.get_entity_id(), item_slot, hovered_entity, selected_entity
        )

    def set_entity_to_debug(self, entity_id: int):
        self.check_world()
        self.world.dispatch_set_entity_to_debug(entity_id)

    def set_entity_statistics_map(self, entity_statistics_map: dict[int, dict[str, float]]):
        self.check_world()
        self.world.dispatch_set_entity_statistics_map(entity_statistics_map)

    def put_time_series(self, timestamp: int, value: float, series_name: str = "performance_metrics") -> None:
        """
        Record a time series value in the C++ world database.

        Args:
            timestamp: Unix timestamp
            value: The value to record
            series_name: Name of the time series (default: "performance_metrics")
        """
        self.check_world()
        self.world.put_time_series(series_name, timestamp, value)

    def query_time_series(self, start: int, end: int, series_name: str = "performance_metrics"):
        """
        Query time series data from the C++ world database.
        Returns a list of (timestamp, value) pairs.

        Args:
            start: Start timestamp
            end: End timestamp
            series_name: Name of the time series to query (default: "performance_metrics")
        """
        self.check_world()
        return self.world.query_time_series(series_name, start, end)

    def execute_sql(self, sql: str) -> None:
        """
        Execute a raw SQL command in the C++ world database.
        """
        self.check_world()
        self.world.execute_sql(sql)

    def close(self) -> None:
        """Gracefully shutdown background executors."""
        try:
            if self._executor is not None:
                self._executor.shutdown(wait=False, cancel_futures=True)  # type: ignore[arg-type]
        except Exception:
            pass
