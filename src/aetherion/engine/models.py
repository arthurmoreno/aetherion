from typing import Any, Dict, Optional, Protocol


class GameEngineProtocol(Protocol):
    """Protocol describing the minimal GameEngine surface used by handlers.

    Attributes are intentionally permissive (Any) to avoid tight coupling with
    concrete engine classes. Replace with concrete types from
    `aetherion.engine` if stronger typing is preferred.
    """

    event_bus: Any

    # Core subsystems
    world_manager: Any
    shared_state: Any

    # Optional connection objects
    server_admin_connection: Optional[Any]
    player_connection: Optional[Any]

    # Current player and world interface
    player: Optional[Any]
    world_interface: Optional[Any]

    # Metadata and managers
    beast_connection_metadata: Dict[str, Any]
    sound_effect_manager: Any
    audio_manager: Any

    # Engine helper methods used by handlers
    def set_input_action_processor(self) -> None: ...
