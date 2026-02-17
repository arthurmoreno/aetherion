"""World state recording and replay functionality.

This module provides the WorldRecorder class for capturing and managing
simulation state snapshots for debugging and analysis purposes.
"""

import os
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

from aetherion.logger import logger

from aetherion import EntityEnum, World
from aetherion.entities.beasts import BeastEnum
from aetherion.world.interface import WorldInterface


class WorldRecorder:
    """Records and replays world simulation states.

    This class is responsible for capturing the state of the world at various points in time
    and allowing for replaying those states for debugging or analysis purposes.

    The state should be a flexible data structure that can represent the entire world state, but with numpy arrays for specific checks.

    For instance, during the initialization of the WorldRecorder,
    you might define the state structure of which fields will be recorded. and how they should be transformed into the final state representation.
    """

    def __init__(self, recording_config: Optional[dict[str, Any]] = None):
        """Initialize the WorldRecorder with optional configuration.

        Args:
            recording_config: Configuration dictionary specifying what to record:
                - 'record_entities': bool - Record entity states (default: True)
                - 'record_terrain': bool - Record terrain voxel grid (default: True)
                - 'record_physics': bool - Record physics state (default: True)
                - 'record_ai_metadata': bool - Record AI metadata (default: False)
                - 'max_snapshots': int - Maximum number of snapshots to store (default: 1000)
                - 'compression': bool - Use compression for snapshots (default: True)
        """
        self.recording_config: dict[str, Any] = recording_config or {}

        # Default configuration
        self.record_entities: bool = self.recording_config.get("record_entities", True)
        self.record_terrain: bool = self.recording_config.get("record_terrain", True)
        self.record_physics: bool = self.recording_config.get("record_physics", True)
        self.record_ai_metadata: bool = self.recording_config.get("record_ai_metadata", False)
        self.max_snapshots: int = self.recording_config.get("max_snapshots", 1000)
        self.use_compression: bool = self.recording_config.get("compression", True)

        # Storage for recorded states
        self.snapshots: list[dict[str, Any]] = []
        self.current_snapshot_index: int = -1
        self.is_recording: bool = False
        self.is_replaying: bool = False

        logger.info(f"WorldRecorder initialized with config: {self.recording_config}")

    def start_recording(self) -> None:
        """Start recording world states."""
        self.is_recording = True
        self.is_replaying = False
        logger.info("Started recording world states")

    def stop_recording(self) -> None:
        """Stop recording world states."""
        self.is_recording = False
        logger.info(f"Stopped recording. Total snapshots: {len(self.snapshots)}")

    def clear_snapshots(self) -> None:
        """Clear all recorded snapshots."""
        self.snapshots.clear()
        self.current_snapshot_index = -1
        logger.info("Cleared all recorded snapshots")

    def record_snapshot(self, world_interface: WorldInterface, tick: int) -> None:
        """Record a snapshot of the current world state.

        Args:
            world_interface: The WorldInterface instance to record from
            tick: Current simulation tick
        """
        if not self.is_recording:
            return

        try:
            snapshot: dict[str, Any] = {"tick": tick, "timestamp": time.time()}

            world = world_interface.world

            # Record entity states
            if self.record_entities:
                snapshot["entities"] = self._capture_entities(world)

            # Record terrain state
            if self.record_terrain:
                snapshot["terrain"] = self._capture_terrain(world)

            # Record physics state
            if self.record_physics:
                snapshot["physics"] = self._capture_physics(world)

            # Record AI metadata
            if self.record_ai_metadata:
                snapshot["ai_metadata"] = self._capture_ai_metadata(world_interface)

            # Compress if enabled
            if self.use_compression:
                snapshot = self._compress_snapshot(snapshot)

            # Add snapshot and manage max size
            self.snapshots.append(snapshot)
            if len(self.snapshots) > self.max_snapshots:
                self.snapshots.pop(0)

            self.current_snapshot_index = len(self.snapshots) - 1

            logger.debug(f"Recorded snapshot at tick {tick}")

        except Exception as e:
            logger.error(f"Failed to record snapshot at tick {tick}: {e}")

    def _capture_entities(self, world: World) -> dict[str, Any]:
        """Capture entity states from the world.

        Args:
            world: The World instance

        Returns:
            Dictionary containing entity data
        """
        import numpy as np

        entities_data: dict[str, Any] = {}

        # Get all entity types we want to track
        entity_types = [
            (EntityEnum.BEAST.value, BeastEnum.SQUIRREL.value),
            (EntityEnum.PLANT.value, 0),
        ]

        for main_type, sub_type in entity_types:
            try:
                entities = world.get_entities_by_type(main_type, sub_type)
                if not entities:
                    continue

                type_key = f"{main_type}_{sub_type}"
                entities_data[type_key] = []

                for entity_id, entity_interface in entities.items():
                    entity_state = {"id": entity_id, "components": {}}

                    # Capture position
                    if entity_interface.has_component("Position"):
                        pos = entity_interface.get_component("Position")
                        entity_state["components"]["position"] = np.array([pos.x, pos.y, pos.z], dtype=np.float32)

                    # Capture velocity
                    if entity_interface.has_component("Velocity"):
                        vel = entity_interface.get_component("Velocity")
                        entity_state["components"]["velocity"] = np.array([vel.dx, vel.dy, vel.dz], dtype=np.float32)

                    # Capture health
                    if entity_interface.has_component("HealthComponent"):
                        health = entity_interface.get_component("HealthComponent")
                        entity_state["components"]["health"] = health.health_level

                    entities_data[type_key].append(entity_state)

            except Exception as e:
                logger.warning(f"Failed to capture entities of type {main_type}_{sub_type}: {e}")

        return entities_data

    def _capture_terrain(self, world: World) -> dict[str, Any]:
        """Capture terrain state from the voxel grid.

        Args:
            world: The World instance

        Returns:
            Dictionary containing terrain data
        """
        import numpy as np

        terrain_data: dict[str, Any] = {
            "width": world.width,
            "height": world.height,
            "depth": world.depth,
        }

        # Sample terrain at key points rather than storing the entire grid
        # For full recording, you would iterate through the entire voxel grid
        sample_points: list[tuple[int, int, int]] = []

        # Sample strategy: record terrain at regular intervals
        step = max(1, world.width // 10)
        for x in range(0, world.width, step):
            for y in range(0, world.height, step):
                for z in range(0, world.depth, step):
                    sample_points.append((x, y, z))

        terrain_samples: list[dict[str, Any]] = []
        for x, y, z in sample_points:
            try:
                terrain_id = world.get_terrain(x, y, z)
                if terrain_id != -1:
                    terrain_samples.append({"pos": np.array([x, y, z], dtype=np.int32), "terrain_id": terrain_id})
            except Exception:
                pass

        terrain_data["samples"] = terrain_samples
        return terrain_data

    def _capture_physics(self, world: World) -> dict[str, Any]:
        """Capture physics state from the world.

        Args:
            world: The World instance

        Returns:
            Dictionary containing physics data
        """
        physics_data: dict[str, Any] = {
            "tick": world.game_clock.get_ticks(),
        }

        return physics_data

    def _capture_ai_metadata(self, world_interface: WorldInterface) -> dict[str, Any]:
        """Capture AI metadata from the world interface.

        Args:
            world_interface: The WorldInterface instance

        Returns:
            Dictionary containing AI metadata
        """
        ai_data: dict[str, Any] = {}

        try:
            ai_metadata = world_interface.get_ai_metadata()
            if ai_metadata:
                for entity_id, entity_metadata in ai_metadata.ai_metadata.items():
                    ai_data[str(entity_id)] = {"brain_created": entity_metadata.brain_created}
        except Exception as e:
            logger.warning(f"Failed to capture AI metadata: {e}")

        return ai_data

    def _compress_snapshot(self, snapshot: dict[str, Any]) -> dict[str, Any]:
        """Compress a snapshot using msgpack and lz4.

        Args:
            snapshot: The snapshot to compress

        Returns:
            Compressed snapshot dictionary
        """
        try:
            import lz4.frame
            import msgpack

            # Convert numpy arrays to lists for msgpack serialization
            serializable_snapshot = self._make_serializable(snapshot)

            # Serialize with msgpack
            packed = msgpack.packb(serializable_snapshot)

            # Compress with lz4
            compressed = lz4.frame.compress(packed)

            return {
                "compressed": True,
                "data": compressed,
                "tick": snapshot["tick"],
                "timestamp": snapshot["timestamp"],
            }
        except Exception as e:
            logger.warning(f"Failed to compress snapshot: {e}. Using uncompressed.")
            return snapshot

    def _make_serializable(self, obj: Any) -> Any:
        """Convert numpy arrays and other non-serializable objects to serializable formats.

        Args:
            obj: Object to make serializable

        Returns:
            Serializable version of the object
        """
        import numpy as np

        if isinstance(obj, np.ndarray):
            return obj.tolist()
        elif isinstance(obj, dict):
            return {k: self._make_serializable(v) for k, v in obj.items()}
        elif isinstance(obj, list):
            return [self._make_serializable(item) for item in obj]
        else:
            return obj

    def _decompress_snapshot(self, snapshot: dict[str, Any]) -> dict[str, Any]:
        """Decompress a compressed snapshot.

        Args:
            snapshot: The compressed snapshot

        Returns:
            Decompressed snapshot dictionary
        """
        if not snapshot.get("compressed", False):
            return snapshot

        try:
            import lz4.frame
            import msgpack

            # Decompress with lz4
            decompressed = lz4.frame.decompress(snapshot["data"])

            # Deserialize with msgpack
            unpacked = msgpack.unpackb(decompressed)

            return unpacked
        except Exception as e:
            logger.error(f"Failed to decompress snapshot: {e}")
            return snapshot

    def get_snapshot(self, index: int) -> Optional[dict[str, Any]]:
        """Get a snapshot by index.

        Args:
            index: Index of the snapshot to retrieve

        Returns:
            The snapshot dictionary, or None if not found
        """
        if 0 <= index < len(self.snapshots):
            snapshot = self.snapshots[index]
            if snapshot.get("compressed", False):
                return self._decompress_snapshot(snapshot)
            return snapshot
        return None

    def get_snapshot_by_tick(self, tick: int) -> Optional[dict[str, Any]]:
        """Get a snapshot by tick number.

        Args:
            tick: Tick number to find

        Returns:
            The snapshot dictionary, or None if not found
        """
        for snapshot in self.snapshots:
            if snapshot["tick"] == tick:
                if snapshot.get("compressed", False):
                    return self._decompress_snapshot(snapshot)
                return snapshot
        return None

    def get_snapshot_count(self) -> int:
        """Get the total number of recorded snapshots.

        Returns:
            Number of snapshots
        """
        return len(self.snapshots)

    def save_to_file(self, filepath: str) -> None:
        """Save all recorded snapshots to a file.

        Args:
            filepath: Path to save the snapshots
        """
        try:
            import pickle

            with open(filepath, "wb") as f:
                pickle.dump({"config": self.recording_config, "snapshots": self.snapshots}, f)

            logger.info(f"Saved {len(self.snapshots)} snapshots to {filepath}")
        except Exception as e:
            logger.error(f"Failed to save snapshots to {filepath}: {e}")

    def load_from_file(self, filepath: str) -> None:
        """Load recorded snapshots from a file.

        Args:
            filepath: Path to load the snapshots from
        """
        try:
            import pickle

            with open(filepath, "rb") as f:
                data = pickle.load(f)

            self.recording_config = data.get("config", {})
            self.snapshots = data.get("snapshots", [])
            self.current_snapshot_index = len(self.snapshots) - 1

            logger.info(f"Loaded {len(self.snapshots)} snapshots from {filepath}")
        except Exception as e:
            logger.error(f"Failed to load snapshots from {filepath}: {e}")


class WorldRecorderManager:
    """Manages multiple world recordings and provides a simple interface for recording operations.

    This manager handles the lifecycle of recordings, including starting, stopping,
    saving, and listing recordings. It manages a single active recorder at a time.
    """

    def __init__(self, recordings_dir: str = "recordings"):
        """Initialize the WorldRecorderManager.

        Args:
            recordings_dir: Directory where recordings will be saved (default: "recordings")
        """
        self.recordings_dir_ = Path(recordings_dir)
        self.recordings_dir_.mkdir(parents=True, exist_ok=True)

        self.active_recorder_: Optional[WorldRecorder] = None
        self.current_recording_name_: Optional[str] = None

        logger.info(f"WorldRecorderManager initialized with recordings directory: {self.recordings_dir_}")

    def start_recording(self, name: Optional[str] = None, config: Optional[dict[str, Any]] = None) -> str:
        """Start a new recording session.

        Args:
            name: Optional name for the recording. If not provided, generates timestamp-based name
            config: Optional configuration for the WorldRecorder

        Returns:
            The name of the started recording

        Raises:
            RuntimeError: If a recording is already in progress
        """
        if self.active_recorder_ is not None and self.active_recorder_.is_recording:
            raise RuntimeError(
                f"Cannot start new recording: '{self.current_recording_name_}' is already in progress. "
                "Stop it first with stop_recording()."
            )

        # Generate name if not provided
        if name is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            name = f"recording_{timestamp}"

        # Create new recorder
        self.active_recorder_ = WorldRecorder(recording_config=config)
        self.current_recording_name_ = name
        self.active_recorder_.start_recording()

        logger.info(f"Started recording: {name}")
        return name

    def stop_recording(self) -> Optional[str]:
        """Stop the current recording session.

        Returns:
            The name of the stopped recording, or None if no recording was active
        """
        if self.active_recorder_ is None or not self.active_recorder_.is_recording:
            logger.warning("No active recording to stop")
            return None

        self.active_recorder_.stop_recording()
        recording_name = self.current_recording_name_

        logger.info(f"Stopped recording: {recording_name} ({self.active_recorder_.get_snapshot_count()} snapshots)")

        return recording_name

    def save_recording(self, name: Optional[str] = None, filepath: Optional[str] = None) -> str:
        """Save the current or specified recording to disk.

        Args:
            name: Name to use when saving. If None, uses current recording name
            filepath: Custom filepath. If None, saves to recordings_dir with .pkl extension

        Returns:
            The filepath where the recording was saved

        Raises:
            RuntimeError: If no active recorder exists
        """
        if self.active_recorder_ is None:
            raise RuntimeError("No active recorder to save")

        # Determine the save name
        save_name = name or self.current_recording_name_
        if save_name is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            save_name = f"recording_{timestamp}"

        # Determine filepath
        if filepath is None:
            filepath = str(self.recordings_dir_ / f"{save_name}.pkl")

        # Save the recording
        self.active_recorder_.save_to_file(filepath)

        logger.info(f"Saved recording to: {filepath}")
        return filepath

    def load_recording(self, name_or_path: str) -> None:
        """Load a recording from disk.

        Args:
            name_or_path: Either a recording name (will look in recordings_dir)
                         or an absolute filepath to a recording file
        """
        # Check if it's a full path
        if os.path.isabs(name_or_path) and os.path.exists(name_or_path):
            filepath = name_or_path
            recording_name = Path(name_or_path).stem
        else:
            # Treat as a name in recordings_dir
            filepath = str(self.recordings_dir_ / f"{name_or_path}.pkl")
            recording_name = name_or_path

        if not os.path.exists(filepath):
            raise FileNotFoundError(f"Recording not found: {filepath}")

        # Create new recorder and load
        self.active_recorder_ = WorldRecorder()
        self.active_recorder_.load_from_file(filepath)
        self.current_recording_name_ = recording_name

        logger.info(f"Loaded recording: {recording_name} ({self.active_recorder_.get_snapshot_count()} snapshots)")

    def list_recordings(self) -> list[dict[str, Any]]:
        """List all available recordings in the recordings directory.

        Returns:
            List of dictionaries containing recording information:
                - name: Recording name (without extension)
                - filepath: Full path to the recording file
                - size_bytes: File size in bytes
                - modified: Last modified timestamp
        """
        recordings: list[dict[str, Any]] = []

        if not self.recordings_dir_.exists():
            return recordings

        for filepath in self.recordings_dir_.glob("*.pkl"):
            try:
                stat = filepath.stat()
                recordings.append(
                    {
                        "name": filepath.stem,
                        "filepath": str(filepath),
                        "size_bytes": stat.st_size,
                        "modified": datetime.fromtimestamp(stat.st_mtime).isoformat(),
                    }
                )
            except Exception as e:
                logger.warning(f"Failed to get info for {filepath}: {e}")

        # Sort by modified time (newest first)
        recordings.sort(key=lambda x: x["modified"], reverse=True)

        return recordings

    def get_active_recorder(self) -> Optional[WorldRecorder]:
        """Get the currently active WorldRecorder instance.

        Returns:
            The active WorldRecorder, or None if no recorder is active
        """
        return self.active_recorder_

    def is_recording(self) -> bool:
        """Check if a recording is currently in progress.

        Returns:
            True if recording is active, False otherwise
        """
        return self.active_recorder_ is not None and self.active_recorder_.is_recording

    def get_current_recording_name(self) -> Optional[str]:
        """Get the name of the current recording.

        Returns:
            The current recording name, or None if no recording is active
        """
        return self.current_recording_name_

    def delete_recording(self, name: str) -> bool:
        """Delete a recording from disk.

        Args:
            name: Name of the recording to delete

        Returns:
            True if deletion was successful, False otherwise
        """
        filepath = self.recordings_dir_ / f"{name}.pkl"

        try:
            if filepath.exists():
                filepath.unlink()
                logger.info(f"Deleted recording: {name}")
                return True
            else:
                logger.warning(f"Recording not found: {name}")
                return False
        except Exception as e:
            logger.error(f"Failed to delete recording {name}: {e}")
            return False
