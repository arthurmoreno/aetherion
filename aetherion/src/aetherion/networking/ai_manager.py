import os
import shutil
from abc import ABC, abstractmethod
from aetherion.world.constants import WorldInstanceTypes

ASYNC_WORKER_OFF = True
LLM_CALLS_DISABLED = True


def check_remove_and_create_directory(dir_path: str):
    """
    Check if a directory with the given path exists.
    If it does, remove the directory and its contents.
    Then, create a new empty directory with the same name.

    Parameters:
    - dir_path (str): The path of the directory to check, remove, and create.
    """
    # Check if the directory exists
    if os.path.exists(dir_path) and os.path.isdir(dir_path):
        # Remove the directory and its contents
        shutil.rmtree(dir_path)
        print(f"Directory '{dir_path}' and its contents have been removed.")

    # Create the new directory
    os.makedirs(dir_path)
    print(f"Directory '{dir_path}' has been created.")


class AIProcessManager(ABC):
    """Abstract base class for AI process management with different connection types."""

    name: str = "ai_manager"
    _server_online: bool = False

    @abstractmethod
    def connect(self, connection_type: WorldInstanceTypes | None = None, world_instance=None, pipe=None) -> bool:
        """Connect to the world interface."""
        pass

    @property
    @abstractmethod
    def server_online(self) -> bool:
        """Check if the server is online."""
        pass

    @abstractmethod
    def request_ai_metadata(self):
        """Request AI metadata from the world."""
        pass

    @abstractmethod
    def send_ai_decision(self, entity_action_map: dict, statistics: dict | None = None):
        """Send AI decisions to the world."""
        pass

    @abstractmethod
    def wait_for_next_world_state(self) -> bytes | None:
        """Wait for the next world state."""
        pass

    @abstractmethod
    def unsubscribe_entities(self, entity_ids: list[int]) -> None:
        """Unsubscribe entities from the world."""
        pass

    @abstractmethod
    def process_ai_decisions(self) -> None:
        """Process AI decisions."""
        pass

    @abstractmethod
    def terminate(self):
        """Terminate all populations and cleanup resources."""
        pass

    @abstractmethod
    def report_metrics(self):
        """Print metrics for debugging or monitoring."""
        pass
