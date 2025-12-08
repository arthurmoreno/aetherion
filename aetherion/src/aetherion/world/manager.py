from threading import Lock, Thread
from typing import Any, Callable, Optional

from logger import logger
from aetherion.world.recorder import WorldRecorderManager

from aetherion import EventBus, GameEventType, SharedState, World, WorldInterfaceMetadata
from aetherion.networking.ai_manager import AIProcessManager
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.interface import WorldInterface


class WorldManager:
    """
    Manages world registration and lifecycle transitions.

    Provides comprehensive world state management including:
    - World creation and registration
    - World lifecycle (connect, disconnect, unload)
    - Simulation control (pause, play, step)
    - World status tracking (creating, ready, running, paused, stepping)

    Usage:
        # Pause the current world
        world_manager.pause_world()

        # Resume the current world
        world_manager.play_world()

        # Step simulation by 1 tick (useful for debugging)
        world_manager.step_simulation(steps=1)

        # Check world status
        status = world_manager.get_world_status()

        # Control a specific world by key
        world_manager.pause_world("my_world_key")
    """

    def __init__(
        self,
        event_bus: EventBus,
        ai_manager_factory: Callable[[Any, WorldInstanceTypes], AIProcessManager] | None = None,
        event_handlers: dict[GameEventType, Callable[[Any], None] | None] | None = None,
    ):
        self.ai_manager: AIProcessManager | None = None
        self.ai_manager_factory = ai_manager_factory
        self.world_factories: dict[str, Callable[[], World]] = {}
        self.worlds_metadata: dict[str, WorldInterfaceMetadata] = {}
        self.worlds: dict[str, WorldInterface] = {}

        self.current: WorldInterface | None = None
        self.current_metadata: WorldInterfaceMetadata | None = None
        self.current_key: str | None = None
        self.world_saves: dict[str, str] = {}  # world_name -> save_path mapping
        self.event_bus: EventBus = event_bus

        self.world_thread_lock: Lock = Lock()

        self.recorder_manager_: WorldRecorderManager = WorldRecorderManager(recordings_dir="recordings")

        # Snapshot storage: world_key -> list of snapshot names
        self.world_snapshots: dict[str, list[str]] = {}

        # Default handlers for critical events
        default_handlers: dict[GameEventType, Callable[[Any], None] | None] = {
            GameEventType.WORLD_CREATE_REQUESTED: self._handle_create_request,
            GameEventType.WORLD_CONNECT_REQUESTED: self._handle_connect_world,
            GameEventType.WORLD_RECORDER_START_REQUESTED: self._handle_recorder_start,
            GameEventType.WORLD_RECORDER_STOP_AND_SAVE_REQUESTED: self._handle_recorder_stop_and_save,
            GameEventType.WORLD_SNAPSHOT_TAKE_REQUESTED: self._handle_snapshot_take,
            GameEventType.WORLD_SNAPSHOT_DELETE_REQUESTED: self._handle_snapshot_delete,
        }

        # Merge user-provided handlers with defaults
        # User handlers override defaults if provided
        self.handlers = default_handlers.copy()
        if event_handlers:
            self.handlers.update(event_handlers)

        # Subscribe to events
        for event_type, handler in self.handlers.items():
            if handler:  # Allow disabling a handler by passing None
                self.event_bus.subscribe(event_type, handler)

    # TODO: Remove from this file, make events easilly customizable. CONSUMER: Event handlers
    def _handle_create_request(self, event):
        """Handle world creation requests"""
        world_name: str = event.data.get("world_name", "default_world")
        world_config = event.data.get("world_config", {})
        world_description: str = event.data.get("world_description", "")
        # world_instance_type = world_config["type"]

        world_key = world_name.lower().replace(" ", "_")

        try:
            if world_config["type"] == WorldInstanceTypes.SYNCHRONOUS:
                self.worlds_metadata[world_key] = WorldInterfaceMetadata(
                    key=world_key, name=world_name, description=world_description, status="creating"
                )
                self.world_thread = Thread(
                    target=self.load_world,
                    args=(),
                    kwargs={
                        "world_name": world_name,
                        "world_config": world_config,
                    },
                )
                self.world_thread.start()
            elif world_config["type"] == WorldInstanceTypes.SERVER:
                world_host: str = event.data.get("world_host", "168.119.102.52")
                world_port: str = event.data.get("world_port", "5202")
                self.worlds_metadata[world_key] = WorldInterfaceMetadata(
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
            self.event_bus.emit(
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
            self.event_bus.emit(
                event_type=GameEventType.WORLD_CREATED,
                data={"world_name": world_name, "world_key": world_key, "success": False, "error": str(e)},
                source="world_manager",
            )

    def load_world(self, world_name: str = "default", world_config: dict[str, Any] = {}) -> None:
        with self.world_thread_lock:
            factory_method = self.world_factories.get("default")
            if factory_method is None:
                raise RuntimeError("World factory is not set.")

            # Initialize the world and player in the thread
            world = factory_method(world_config)
            world_instance_type = world_config["type"]
            world_key: str = world_name.lower().replace(" ", "_")
            self.worlds[world_key] = WorldInterface(world_instance_type, world)

            self.worlds_metadata[world_key].gravity = world_config.get("gravity", 5.0)
            self.worlds_metadata[world_key].friction = world_config.get("friction", 1.0)
            self.worlds_metadata[world_key].allow_multi_direction = world_config.get("allow_multi_direction", True)
            self.worlds_metadata[world_key].evaporation_coefficient = world_config.get("evaporation_coefficient", 8.0)
            self.worlds_metadata[world_key].heat_to_water_evaporation = world_config.get(
                "heat_to_water_evaporation", 120.0
            )
            self.worlds_metadata[world_key].water_minimum_units = world_config.get("water_minimum_units", 120000)
            self.worlds_metadata[world_key].metabolism_cost_to_apply_force = world_config.get(
                "metabolism_cost_to_apply_force", 1.999999949504854e-06
            )
            self.worlds_metadata[world_key].status = "ready"

        # Mark the world as ready after loading
        # self._ready = True

    def start_ai_manager(self, world_instance, connection_type: WorldInstanceTypes = WorldInstanceTypes.SYNCHRONOUS):
        if self.ai_manager_factory:
            self.ai_manager = self.ai_manager_factory(connection_type)
            if not isinstance(self.ai_manager, AIProcessManager):
                raise RuntimeError("AI Manager factory did not return a valid AIProcessManager instance.")

            self.ai_manager.connect(connection_type=connection_type, world_instance=world_instance)
            self.ai_manager.request_ai_metadata()

    # TODO: Remove from this file, make events easilly customizable. CONSUMER: Event handlers
    def _handle_connect_world(self, event):
        """Handle world connection requests"""
        world_name = event.data.get("world_name")
        world_key = event.data.get("world_key", world_name.lower().replace(" ", "_"))
        world_instance_type = self.worlds_metadata[world_key].type

        self.current_metadata = self.worlds_metadata.get(world_key)
        self.current = self.worlds.get(world_key)
        self.current_key = world_key

        if self.current_metadata is not None and self.current is None:
            # virtual connection created. For world server type.
            self.event_bus.emit(
                event_type=GameEventType.WORLD_CONNECTED,
                data={"world_name": world_name, "success": True},
                source="world_manager",
            )
            return

        if self.current is None:
            raise ValueError(f"World '{world_key}' is not registered.")

        self.start_ai_manager(self.current, connection_type=world_instance_type)

        # Set initial status to running when connected
        if self.current_metadata and self.current_metadata.status in ["ready", "created"]:
            self.current_metadata.status = "running"

        self.event_bus.emit(
            event_type=GameEventType.WORLD_CONNECTED,
            data={"world_name": world_name, "success": True},
            source="world_manager",
        )

    # TODO: Remove from this file, make events easilly customizable.RECORDER: Event handlers
    def _handle_recorder_start(self, event):
        """Handle world recorder start requests.

        Expected event data:
            - recording_name: Optional[str] - Name for the recording
            - recording_config: Optional[dict] - Configuration for the recorder
        """
        try:
            recording_name = event.data.get("recording_name")
            recording_config = event.data.get("recording_config")

            # Start the recording
            actual_name = self.recorder_manager_.start_recording(name=recording_name, config=recording_config)

            logger.info(f"Started recording: {actual_name}")

            # Emit success event
            self.event_bus.emit(
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
            self.event_bus.emit(
                event_type=GameEventType.WORLD_CREATED,  # TODO: Create WORLD_RECORDER_STARTED event
                data={
                    "success": False,
                    "error": str(e),
                },
                source="world_manager",
            )

    def _handle_recorder_stop_and_save(self, event):
        """Handle world recorder stop and save requests.

        Expected event data:
            - save_name: Optional[str] - Name to use when saving
            - save_path: Optional[str] - Custom filepath for saving
        """
        try:
            save_name = event.data.get("save_name")
            save_path = event.data.get("save_path")

            # Stop the recording
            stopped_name = self.recorder_manager_.stop_recording()

            if stopped_name is None:
                logger.warning("No active recording to stop")
                self.event_bus.emit(
                    event_type=GameEventType.WORLD_SAVED,  # TODO: Create WORLD_RECORDER_SAVED event
                    data={
                        "success": False,
                        "error": "No active recording",
                    },
                    source="world_manager",
                )
                return

            # Save the recording
            saved_path = self.recorder_manager_.save_recording(name=save_name, filepath=save_path)

            logger.info(f"Stopped and saved recording to: {saved_path}")

            # Emit success event
            self.event_bus.emit(
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
            self.event_bus.emit(
                event_type=GameEventType.WORLD_SAVED,  # TODO: Create WORLD_RECORDER_SAVED event
                data={
                    "success": False,
                    "error": str(e),
                },
                source="world_manager",
            )

    # TODO: Remove from this file, make events easilly customizable.SNAPSHOT: Event handlers
    def _handle_snapshot_take(self, event):
        """Handle world snapshot take requests.

        Expected event data:
            - name: str - Name for the snapshot (with timestamp)
            - world_key: Optional[str] - World to snapshot (defaults to current)
        """
        try:
            snapshot_name: str = event.data.get("name")
            world_key: str = event.data.get("world_key", self.current_key)

            if not snapshot_name:
                raise ValueError("Snapshot name is required")

            if not world_key:
                raise ValueError("No world available to snapshot")

            # Initialize snapshot list for this world if needed
            if world_key not in self.world_snapshots:
                self.world_snapshots[world_key] = []

            # Get the world instance
            world_interface = self.worlds.get(world_key)
            if not world_interface:
                raise ValueError(f"World '{world_key}' not found")

            # TODO: Implement actual snapshot capture
            # For now, just store the snapshot name
            self.world_snapshots[world_key].append(snapshot_name)

            logger.info(f"Snapshot taken: {snapshot_name} for world '{world_key}'")

        except Exception as e:
            logger.error(f"Failed to take snapshot: {e}")

    def _handle_snapshot_delete(self, event):
        """Handle world snapshot deletion requests.

        Expected event data:
            - name: str - Name of the snapshot to delete
            - index: int - Index of the snapshot in the list
            - world_key: Optional[str] - World key (defaults to current)
        """
        try:
            snapshot_name = event.data.get("name")
            snapshot_index = event.data.get("index")
            world_key = event.data.get("world_key", self.current_key)

            if not snapshot_name:
                raise ValueError("Snapshot name is required")

            if not world_key:
                raise ValueError("No world key provided")

            # Verify snapshot exists
            if world_key not in self.world_snapshots:
                raise ValueError(f"No snapshots found for world '{world_key}'")

            if snapshot_name not in self.world_snapshots[world_key]:
                raise ValueError(f"Snapshot '{snapshot_name}' not found")

            # Remove the snapshot
            self.world_snapshots[world_key].remove(snapshot_name)

            logger.info(f"Deleted snapshot: {snapshot_name} from world '{world_key}'")

        except Exception as e:
            logger.error(f"Failed to delete snapshot: {e}")

    def update_shared_state_snapshots(self, shared_state: SharedState) -> None:
        """
        Update the shared_state with current snapshot data.

        Loads the snapshot list for the current world into shared_state
        so it can be accessed by the GUI layer.

        This method should be called before rendering the GUI to ensure
        the EditorDebuggerProgram displays the current snapshot list.

        Args:
            shared_state: SharedState object to update with snapshot data

        Usage:
            world_manager.update_shared_state_snapshots(shared_state)
            # Then shared_state.snapshots will be available in GUI
        """
        if not self.current_key:
            # No current world, set empty list
            shared_state.snapshots = []
            return

        # Get snapshots for the current world
        snapshots = self.world_snapshots.get(self.current_key, [])
        if snapshots:
            breakpoint()
            print("stop")

        # Update shared_state with the snapshot list
        shared_state.snapshots = snapshots

        logger.debug(f"Updated shared_state with {len(snapshots)} snapshot(s) for world '{self.current_key}'")

    # Simulation Control Methods
    def _validate_target_key(self, world_key: str | None = None) -> str | None:
        """
        Validate and return the target world key.

        Args:
            world_key: The key of the world. If None, uses the current world key.

        Returns:
            The validated world key, or None if validation fails.
        """
        target_key = world_key or self.current_key

        if target_key is None:
            logger.warning("No world key provided and no current world is active.")
            return None

        if target_key not in self.worlds_metadata:
            logger.error(f"World metadata for '{target_key}' not found.")
            return None

        return target_key

    def _pause_world(self, world_key: str | None = None) -> None:
        """
        Pause the specified world or the current world.

        Args:
            world_key: The key of the world to pause. If None, pauses the current world.
        """
        target_key = self._validate_target_key(world_key)
        if target_key is None:
            return

        metadata = self.worlds_metadata[target_key]

        if metadata.status == "paused":
            logger.info(f"World '{target_key}' is already paused.")
            return

        metadata.status = "paused"
        logger.info(f"World '{target_key}' has been paused.")

    def _play_world(self, world_key: str | None = None) -> None:
        """
        Resume/play the specified world or the current world.

        Args:
            world_key: The key of the world to play. If None, plays the current world.
        """
        target_key = self._validate_target_key(world_key)
        if target_key is None:
            return

        metadata = self.worlds_metadata[target_key]

        if metadata.status == "running":
            logger.info(f"World '{target_key}' is already running.")
            return

        metadata.status = "running"
        logger.info(f"World '{target_key}' is now running.")

    def _step_simulation(self, world_key: str | None = None, steps: int = 1) -> None:
        """
        Step the simulation forward by a specified number of ticks.
        Useful for debugging and controlled simulation.

        Args:
            world_key: The key of the world to step. If None, steps the current world.
            steps: Number of simulation steps to perform (default: 1).
        """
        target_key = self._validate_target_key(world_key)
        if target_key is None:
            return

        world_interface = self.worlds.get(target_key)

        if world_interface is None:
            logger.error(f"World instance for '{target_key}' not found.")
            return

        metadata = self.worlds_metadata[target_key]
        previous_status = metadata.status

        # Temporarily mark as stepping
        metadata.status = "stepping"

        logger.info(f"Stepping world '{target_key}' by {steps} tick(s).")

        # Perform the simulation steps
        for step in range(steps):
            world_interface.update_world()
            logger.debug(f"World '{target_key}' - Step {step + 1}/{steps} completed.")

        # Restore previous status (or set to paused if it was stepping)
        if previous_status != "stepping":
            metadata.status = previous_status
        else:
            metadata.status = "paused"

    def _get_world_status(self, world_key: str | None = None) -> str | None:
        """
        Get the status of the specified world or the current world.

        Args:
            world_key: The key of the world. If None, gets the current world status.

        Returns:
            The status string or None if the world is not found.
        """
        target_key = self._validate_target_key(world_key)
        if target_key is None:
            return None

        return self.worlds_metadata[target_key].status

    # Public API for simulation control
    def pause_world(self, world_key: str | None = None) -> None:
        """
        Public method to pause a world.

        Args:
            world_key: The key of the world to pause. If None, pauses the current world.
        """
        self._pause_world(world_key)

    def play_world(self, world_key: str | None = None) -> None:
        """
        Public method to play/resume a world.

        Args:
            world_key: The key of the world to play. If None, plays the current world.
        """
        self._play_world(world_key)

    def step_simulation(self, world_key: str | None = None, steps: int = 1) -> None:
        """
        Public method to step the simulation.

        Args:
            world_key: The key of the world to step. If None, steps the current world.
            steps: Number of simulation steps to perform (default: 1).
        """
        self._step_simulation(world_key, steps)

    def get_world_status(self, world_key: str | None = None) -> str | None:
        """
        Public method to get world status.

        Args:
            world_key: The key of the world. If None, gets the current world status.

        Returns:
            The status string or None if the world is not found.
        """
        return self._get_world_status(world_key)

    def register_factory(self, name: str, factory: Callable[[], World]) -> None:
        """Register a world factory under a name."""
        if name in self.world_factories:
            raise ValueError(f"Factory with name '{name}' is already registered.")

        # if not issubclass(factory, WorldFactory):
        #     raise TypeError(f"{factory} must inherit from WorldFactory")

        self.world_factories[name] = factory

    def register(self, name: str, world: WorldInterface) -> WorldInterface:
        """Register a world instance under a name."""
        if name in self.worlds:
            raise ValueError(f"World with name '{name}' is already registered.")

        if not isinstance(world, WorldInterface):
            raise TypeError(f"{world} must inherit from WorldInterface")

        self.worlds[name] = world

        return world

    def create_world(self, name: str, world_config: dict[str, Any], world_class=None) -> WorldInterface:
        """Create and register a new world with given configuration."""
        if world_class is None:
            world_class = World

        world_instance = world_class(name, world_config)
        return self.register(name, world_instance)

    def get_world(self, world_name: str) -> WorldInterface:
        """Get a registered world by name."""
        world_key: str = world_name.lower().replace(" ", "_")
        return self.worlds[world_key]

    def change(self, name: str):
        """Switch to a registered world by name."""
        if self.current:
            self.current.on_exit()
        self.current = self.worlds.get(name)
        if self.current:
            self.current.on_enter()

    def process_ai_decisions(self) -> None:
        """
        Process AI decisions if the conditions are met.
        """
        if self.current and isinstance(self.ai_manager, AIProcessManager) and self.ai_manager.server_online:
            self.ai_manager.process_ai_decisions()

    def update(self):
        """Update the current world; handle world simulation."""
        if not self.current:
            return

        # Check if the current world is in a runnable state
        if self.current_metadata:
            status = self.current_metadata.status

            # Only update if the world is running
            if status == "running":
                self.current.update_world()
            elif status == "paused":
                # World is paused, don't update
                logger.debug(f"World '{self.current_key}' is paused. Skipping update.")
                return
            elif status == "stepping":
                # Stepping is handled by _step_simulation, don't update here
                return
            else:
                # For other statuses (creating, ready, etc.), don't update
                logger.debug(f"World '{self.current_key}' has status '{status}'. Skipping update.")
                return
        else:
            # If no metadata, update anyway (backward compatibility)
            self.current.update_world()

    def save_current_world(self, save_path: str):
        """Save the current world to a file."""
        if self.current:
            self.current.save(save_path)
            # Store the save path for this world
            for name, world in self.worlds.items():
                if world == self.current:
                    self.world_saves[name] = save_path
                    break

    def load_world_from_save(self, world_name: str, save_path: str, world_class=None) -> WorldInterface:
        """Load a world from a save file."""
        if world_class is None:
            world_class = World

        world_instance = world_class(world_name, {})
        world_instance.load_from_save(save_path)

        # Register the loaded world
        self.register(world_name, world_instance)
        self.world_saves[world_name] = save_path

        return world_instance

    def unload_world(self, world_name: str):
        """Unload a world and free its resources."""
        if world_name in self.worlds:
            world = self.worlds[world_name]
            if world == self.current:
                world.on_exit()
                self.current = None
            world.on_unload()
            del self.worlds[world_name]
            if world_name in self.world_saves:
                del self.world_saves[world_name]

    def list_worlds(self) -> list[str]:
        """Get a list of all registered world names."""
        return list(self.worlds.keys())

    def get_current_world_name(self) -> Optional[str]:
        """Get the name of the currently active world."""
        if not self.current:
            return None
        for name, world in self.worlds.items():
            if world == self.current:
                return name
        return None
