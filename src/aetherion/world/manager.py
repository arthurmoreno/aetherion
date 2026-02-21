from functools import partial
from threading import Lock
from typing import Any, Callable, Optional

from aetherion import EventBus, GameEventType, SharedState, World, WorldInterfaceMetadata
from aetherion.events.handlers.types import WorldEventHandlersMap
from aetherion.logger import logger
from aetherion.networking.ai_manager import AIProcessManager
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.interface import WorldInterface
from aetherion.world.recorder import WorldRecorderManager


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
        event_bus: EventBus[GameEventType],
        ai_manager_factory: Callable[[Any, WorldInstanceTypes], AIProcessManager] | None = None,
        event_handlers: WorldEventHandlersMap | None = None,
        default_event_handlers: WorldEventHandlersMap | None = None,
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
        self.event_bus: EventBus[GameEventType] = event_bus

        self.world_thread_lock: Lock = Lock()

        self.recorder_manager_: WorldRecorderManager = WorldRecorderManager(recordings_dir="recordings")

        # Snapshot storage: world_key -> list of snapshot names
        self.world_snapshots: dict[str, list[str]] = {}

        if default_event_handlers is None and event_handlers is None:
            raise ValueError("At least one of default_event_handlers or event_handlers must be provided.")

        # Merge user-provided handlers with defaults
        # User handlers override defaults if provided
        self.handlers: WorldEventHandlersMap = {}
        if event_handlers:
            self.handlers = event_handlers.copy()
            self.handlers.update(event_handlers)
        elif default_event_handlers:
            self.handlers = default_event_handlers.copy()

        # Subscribe to events
        for event_type, handler in self.handlers.items():
            if handler:  # Allow disabling a handler by passing None
                self.event_bus.subscribe(event_type, partial(handler, self))

    def load_world(
        self, world_name: str = "default", world_factory_name: str = "default", world_config: dict[str, Any] = {}
    ) -> None:
        with self.world_thread_lock:
            factory_method = self.world_factories.get(world_factory_name)
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
            # self.worlds_metadata[world_key].water_minimum_units = world_config.get("water_minimum_units", 120000)
            self.worlds_metadata[world_key].water_minimum_units = world_config.get("water_minimum_units", 30000)
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
