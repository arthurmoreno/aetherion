from __future__ import annotations

from threading import Thread
from typing import Any, Callable

from aetherion import (
    GameEvent,
    GameEventType,
    WorldInterfaceMetadata,
)
from aetherion.logger import logger
from aetherion.world.constants import WorldInstanceTypes
from aetherion.events.handlers.types import WorldEventHandlersMap
from aetherion.world.models import WorldManagerProtocol


# CONSUMER: Event handlers
def on_world_create_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world creation requests"""
    world_name: str = event.data.get("world_name", "default_world")
    world_factory_name: str = event.data.get("world_factory_name", "default")
    world_config: dict[str, Any] = event.data.get("world_config", {})
    world_description: str = event.data.get("world_description", "")
    # world_instance_type = world_config["type"]

    world_key: str = world_name.lower().replace(" ", "_")

    try:
        if world_config["type"] == WorldInstanceTypes.SYNCHRONOUS:
            world_manager.worlds_metadata[world_key] = WorldInterfaceMetadata(
                key=world_key, name=world_name, description=world_description, status="creating"
            )
            world_manager.world_thread = Thread(
                target=world_manager.load_world,
                args=(),
                kwargs={
                    "world_name": world_name,
                    "world_factory_name": world_factory_name,
                    "world_config": world_config,
                },
            )
            world_manager.world_thread.start()
        elif world_config["type"] == WorldInstanceTypes.SERVER:
            world_host: str = event.data.get("world_host", "168.119.102.52")
            world_port: str = event.data.get("world_port", "5202")
            world_manager.worlds_metadata[world_key] = WorldInterfaceMetadata(
                key=world_key,
                name=world_name,
                description=world_description,
                type=WorldInstanceTypes.SERVER,
                status="ready",
                host=world_host,
                port=world_port,
            )
        else:
            raise ValueError(f"Unsupported world instance type: {world_config['type']}")

        # CLIENT: Notify success
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_CREATED,
            data={
                "world_name": world_name,
                "world_key": world_key,
                "success": True,
            },
            source="world_manager",
        )

    except Exception as e:
        # CLIENT: Notify failure
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_CREATED,
            data={"world_name": world_name, "world_key": world_key, "success": False, "error": str(e)},
            source="world_manager",
        )


def on_world_connect_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world connection requests"""
    world_name: str = event.data.get("world_name")
    world_key: str = event.data.get("world_key", world_name.lower().replace(" ", "_"))

    if world_key not in world_manager.worlds_metadata:
        raise ValueError(f"World '{world_key}' is not registered.")

    world_instance_type: WorldInstanceTypes = world_manager.worlds_metadata[world_key].type

    world_manager.current_metadata = world_manager.worlds_metadata.get(world_key)
    world_manager.current = world_manager.worlds.get(world_key)
    world_manager.current_key = world_key

    if world_manager.current_metadata is not None and world_manager.current_metadata.status not in ["ready", "created"]:
        logger.error(f"World '{world_key}' is not ready for connection. Current status: {world_manager.current_metadata.status}")
        raise RuntimeError(f"World '{world_key}' is not ready for connection. Current status: {world_manager.current_metadata.status}")

    if world_manager.current_metadata is not None and world_manager.current is None:
        # virtual connection created. For world server type.
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_CONNECTED,
            data={"world_name": world_name, "success": True},
            source="world_manager",
        )
        return

    if world_manager.current is None:
        raise ValueError(f"World '{world_key}' is not registered.")

    world_manager.start_ai_manager(world_manager.current, connection_type=world_instance_type)

    # Set initial status to running when connected
    if world_manager.current_metadata and world_manager.current_metadata.status in ["ready", "created"]:
        world_manager.current_metadata.status = "running"
    world_manager.event_bus.emit(
        event_type=GameEventType.WORLD_CONNECTED,
        data={"world_name": world_name, "success": True},
        source="world_manager",
    )


# RECORDER: Event handlers
def on_recorder_start_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world recorder start requests.

    Expected event data:
        - recording_name: Optional[str] - Name for the recording
        - recording_config: Optional[dict] - Configuration for the recorder
    """
    try:
        recording_name: str | None = event.data.get("recording_name")
        recording_config: dict[str, Any] | None = event.data.get("recording_config")

        # Start the recording
        actual_name = world_manager.recorder_manager_.start_recording(name=recording_name, config=recording_config)

        logger.info(f"Started recording: {actual_name}")

        # Emit success event
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_CREATED,  # TODO: Create WORLD_RECORDER_STARTED event
            data={
                "recording_name": actual_name,
                "success": True,
            },
            source="world_manager",
        )

    except Exception as e:
        logger.error(f"Failed to start recording: {e}")

        # Emit failure event
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_CREATED,  # TODO: Create WORLD_RECORDER_STARTED event
            data={
                "success": False,
                "error": str(e),
            },
            source="world_manager",
        )


def on_recorder_stop_and_save_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world recorder stop and save requests.

    Expected event data:
        - save_name: Optional[str] - Name to use when saving
        - save_path: Optional[str] - Custom filepath for saving
    """
    try:
        save_name: str | None = event.data.get("save_name")
        save_path: str | None = event.data.get("save_path")

        # Stop the recording
        stopped_name = world_manager.recorder_manager_.stop_recording()

        if stopped_name is None:
            logger.warning("No active recording to stop")
            world_manager.event_bus.emit(
                event_type=GameEventType.WORLD_SAVED,  # TODO: Create WORLD_RECORDER_SAVED event
                data={
                    "success": False,
                    "error": "No active recording",
                },
                source="world_manager",
            )
            return

        # Save the recording
        saved_path = world_manager.recorder_manager_.save_recording(name=save_name, filepath=save_path)

        logger.info(f"Stopped and saved recording to: {saved_path}")

        # Emit success event
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_SAVED,  # TODO: Create WORLD_RECORDER_SAVED event
            data={
                "recording_name": stopped_name,
                "save_path": saved_path,
                "success": True,
            },
            source="world_manager",
        )

    except Exception as e:
        logger.error(f"Failed to stop and save recording: {e}")

        # Emit failure event
        world_manager.event_bus.emit(
            event_type=GameEventType.WORLD_SAVED,  # TODO: Create WORLD_RECORDER_SAVED event
            data={
                "success": False,
                "error": str(e),
            },
            source="world_manager",
        )


# SNAPSHOT: Event handlers
def on_snapshot_take_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world snapshot take requests.

    Expected event data:
        - name: str - Name for the snapshot (with timestamp)
        - world_key: Optional[str] - World to snapshot (defaults to current)
    """
    try:
        snapshot_name: str = event.data.get("name")
        world_key: str = event.data.get("world_key", world_manager.current_key)

        if not snapshot_name:
            raise ValueError("Snapshot name is required")

        if not world_key:
            raise ValueError("No world available to snapshot")

        # Initialize snapshot list for this world if needed
        if world_key not in world_manager.world_snapshots:
            world_manager.world_snapshots[world_key] = []

        # Get the world instance
        world_interface = world_manager.worlds.get(world_key)
        if not world_interface:
            raise ValueError(f"World '{world_key}' not found")

        # TODO: Implement actual snapshot capture
        # For now, just store the snapshot name
        world_manager.world_snapshots[world_key].append(snapshot_name)

        logger.info(f"Snapshot taken: {snapshot_name} for world '{world_key}'")

    except Exception as e:
        logger.error(f"Failed to take snapshot: {e}")


def on_snapshot_delete_requested(world_manager: WorldManagerProtocol, event: GameEvent[GameEventType]) -> None:
    """Handle world snapshot deletion requests.

    Expected event data:
        - name: str - Name of the snapshot to delete
        - index: int - Index of the snapshot in the list
        - world_key: Optional[str] - World key (defaults to current)
    """
    try:
        snapshot_name: str = event.data.get("name")
        # snapshot_index = event.data.get("index")
        world_key: str = event.data.get("world_key", world_manager.current_key)

        if not snapshot_name:
            raise ValueError("Snapshot name is required")

        if not world_key:
            raise ValueError("No world key provided")

        # Verify snapshot exists
        if world_key not in world_manager.world_snapshots:
            raise ValueError(f"No snapshots found for world '{world_key}'")

        if snapshot_name not in world_manager.world_snapshots[world_key]:
            raise ValueError(f"Snapshot '{snapshot_name}' not found")

        # Remove the snapshot
        world_manager.world_snapshots[world_key].remove(snapshot_name)
        logger.info(f"Deleted snapshot: {snapshot_name} from world '{world_key}'")

    except Exception as e:
        logger.error(f"Failed to delete snapshot: {e}")


worldmanager_event_handlers: WorldEventHandlersMap | None = {
    GameEventType.WORLD_CREATE_REQUESTED: on_world_create_requested,
    GameEventType.WORLD_CONNECT_REQUESTED: on_world_connect_requested,
    GameEventType.WORLD_RECORDER_START_REQUESTED: on_recorder_start_requested,
    GameEventType.WORLD_RECORDER_STOP_AND_SAVE_REQUESTED: on_recorder_stop_and_save_requested,
    GameEventType.WORLD_SNAPSHOT_TAKE_REQUESTED: on_snapshot_take_requested,
    GameEventType.WORLD_SNAPSHOT_DELETE_REQUESTED: on_snapshot_delete_requested,
}
