from __future__ import annotations

from typing import TYPE_CHECKING

from aetherion import (
    GameEvent,
    GameEventType,
    PhysicsSettings,
    WorldInterfaceMetadata,
)
from aetherion.events.handlers.types import (
    GameEngineEventHandlersMap,
)
from aetherion.logger import logger
from aetherion.networking.admin_connection import ServerAdminConnection
from aetherion.paths import resolve_path
from aetherion.world.constants import WorldInstanceTypes

if TYPE_CHECKING:
    from aetherion.engine.models import GameEngineProtocol


def on_world_connected(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    world_name = event.data.get("world_name")
    world_key = event.data.get("world_key", world_name.lower().replace(" ", "_"))

    world_metadata: WorldInterfaceMetadata | None = game_engine.world_manager.worlds_metadata.get(world_key)
    if not world_metadata:
        logger.error(f"World metadata for '{world_key}' not found.")
        raise RuntimeError(f"World metadata for '{world_key}' not found.")

    if world_metadata.type == WorldInstanceTypes.SYNCHRONOUS:
        physics_settings: PhysicsSettings = PhysicsSettings()
        physics_settings.set_gravity(world_metadata.gravity)
        physics_settings.set_friction(world_metadata.friction)
        physics_settings.set_metabolism_cost_to_apply_force(world_metadata.metabolism_cost_to_apply_force)
        physics_settings.set_evaporation_coefficient(world_metadata.evaporation_coefficient)
        physics_settings.set_heat_to_water_evaporation(world_metadata.heat_to_water_evaporation)
        physics_settings.set_water_minimum_units(world_metadata.water_minimum_units)
        game_engine.shared_state.connected_world = world_key
        logger.info(f"Connected to world: {world_key} with physics settings: {physics_settings}")
    elif world_metadata.type == WorldInstanceTypes.SERVER:
        # For SERVER worlds, we just set the connected world without physics settings
        game_engine.server_admin_connection = ServerAdminConnection()
        game_engine.server_admin_connection.connect(connection_type=world_metadata.type)
        game_engine.shared_state.connected_world = world_key


def _pause_world(game_engine: GameEngineProtocol, world_key: str) -> None:
    """Pause a world using the WorldManager."""
    game_engine.world_manager.pause_world(world_key)


def on_change_desired_fps(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    desired_fps = event.data.get("desired_fps")
    if desired_fps is not None:
        game_engine.shared_state.desired_fps = desired_fps
        logger.info(f"Desired FPS changed to: {desired_fps}")


def on_world_disconnect_requested(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    world_key: str | None = game_engine.shared_state.connected_world
    if world_key is not None:
        _pause_world(game_engine, world_key)

    game_engine.shared_state.connected_world = None
    game_engine.shared_state.connected_beast = None
    game_engine.player_connection = None
    game_engine.world_interface = None
    logger.info(f"Disconnected from world: {world_key}")


def on_world_stop_requested(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    world_key: str | None = game_engine.shared_state.connected_world
    if not world_key:
        logger.error("No world is connected. Cannot stop world.")
        raise RuntimeError("No world is connected.")

    _pause_world(game_engine, world_key)


def on_world_play_requested(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    world_key: str | None = game_engine.shared_state.connected_world
    if not world_key:
        logger.error("No world is connected. Cannot play world.")
        raise RuntimeError("No world is connected.")

    game_engine.world_manager.play_world(world_key)


def on_create_beast_requested(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    raise NotImplementedError("Creating a beast is not implemented. Implement this in your game-specific code.")


def on_connect_beast_requested(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    raise NotImplementedError("Beast connection handling is not implemented. Provide a game-specific implementation.")


def on_audio_sound_effect_play(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    sound_name = event.data.get("name")
    file_path = event.data.get("file_path")
    loops = event.data.get("loops", 0)
    volume = event.data.get("volume", 128)

    if not sound_name or not file_path:
        logger.error("Missing name or file_path for sound effect.")
        return

    resolved_path = str(resolve_path(file_path))
    try:
        game_engine.sound_effect_manager.load_sound(sound_name, resolved_path)
        game_engine.sound_effect_manager.play_sound(sound_name, loops=loops, volume=volume)
    except Exception as e:
        logger.error(f"Failed to play sound effect {sound_name}: {e}")


def on_audio_music_play(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    file_path = event.data.get("file_path")
    loops = event.data.get("loops", -1)
    fade_ms = event.data.get("fade_ms", 0)
    volume = event.data.get("volume", 128)

    if not file_path:
        logger.error("Missing file_path for music.")
        return

    resolved_path = str(resolve_path(file_path))
    try:
        game_engine.audio_manager.load_music(resolved_path)
        game_engine.audio_manager.set_music_volume(volume)
        if fade_ms > 0:
            game_engine.audio_manager.fade_in_music(loops=loops, fade_ms=fade_ms)
        else:
            game_engine.audio_manager.play_music(loops=loops)
    except Exception as e:
        logger.error(f"Failed to play music {file_path}: {e}")


def on_audio_music_stop(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    fade_ms = event.data.get("fade_ms", 0)
    if fade_ms > 0:
        game_engine.audio_manager.fade_out_music(fade_ms=fade_ms)
    else:
        game_engine.audio_manager.stop_music()


def on_audio_music_pause(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    game_engine.audio_manager.pause_music()


def on_audio_music_resume(game_engine: GameEngineProtocol, event: GameEvent[GameEventType]) -> None:
    game_engine.audio_manager.resume_music()


gameengine_event_handlers: GameEngineEventHandlersMap | None = {
    GameEventType.CHANGE_DESIRED_FPS: on_change_desired_fps,
    GameEventType.WORLD_CONNECTED: on_world_connected,
    GameEventType.CREATE_BEAST_REQUESTED: on_create_beast_requested,
    GameEventType.BEAST_CONNECT_REQUESTED: on_connect_beast_requested,
    GameEventType.WORLD_DISCONNECT_REQUESTED: on_world_disconnect_requested,
    GameEventType.WORLD_PLAY_REQUESTED: on_world_play_requested,
    GameEventType.WORLD_STOP_REQUESTED: on_world_stop_requested,
}


audiomanager_event_handlers: GameEngineEventHandlersMap | None = {
    GameEventType.AUDIO_SOUND_EFFECT_PLAY: on_audio_sound_effect_play,
    GameEventType.AUDIO_MUSIC_PLAY: on_audio_music_play,
    GameEventType.AUDIO_MUSIC_STOP: on_audio_music_stop,
    GameEventType.AUDIO_MUSIC_PAUSE: on_audio_music_pause,
    GameEventType.AUDIO_MUSIC_RESUME: on_audio_music_resume,
}
