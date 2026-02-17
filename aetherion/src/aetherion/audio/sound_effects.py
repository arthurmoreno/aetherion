"""Audio module for SDL2-based music and sound effects management.

This module provides classes and functions for initializing SDL2 audio,
managing background music playback, and handling sound effects using SDL2_mixer.
"""

from ctypes import c_void_p

import sdl2.sdlmixer as mixer
from aetherion.logger import logger

from aetherion.audio.base_manager import BaseAudioManager
from aetherion.audio.exceptions import AudioLoadError, AudioPlaybackError


class SoundEffectManager(BaseAudioManager):
    """Manages sound effects playback using SDL2_mixer channels.

    Handles loading and playing multiple simultaneous sound effects with
    configurable channel allocation and priority-based playback.
    """

    def __init__(self, num_channels: int = 16, channel_offset: int = 4) -> None:
        """Initialize the SoundEffectManager.

        Args:
            num_channels: Number of mixing channels to use (default: 16).
            channel_offset: Starting channel index for this manager (default: 4,
                          leaving 0-3 for ambient sounds).
        """
        self.sounds: dict[str, c_void_p] = {}
        self._num_channels = num_channels
        self._channel_offset = channel_offset
        self._channel_end = channel_offset + num_channels

        # Ensure enough channels are allocated globally
        current_channels = mixer.Mix_AllocateChannels(-1)  # pyright: ignore
        needed_channels = self._channel_end
        if current_channels < needed_channels:
            mixer.Mix_AllocateChannels(needed_channels)  # pyright: ignore
            logger.debug(f"Allocated {needed_channels} total channels")

        logger.info(f"SoundEffectManager initialized: {num_channels} channels (offset: {channel_offset})")

    def load_sound(self, name: str, file_path: str | bytes) -> None:
        """Load a sound effect file.

        Args:
            name: Identifier for the sound effect.
            file_path: Path to the sound file (WAV, OGG, etc.).

        Raises:
            AudioLoadError: If the sound file cannot be loaded.
        """
        # Free previous sound if it exists
        if name in self.sounds:
            mixer.Mix_FreeChunk(self.sounds[name])  # pyright: ignore

        path_bytes = self._to_path_bytes(file_path)
        path_str = self._to_path_str(file_path)

        sound = mixer.Mix_LoadWAV(path_bytes)  # pyright: ignore
        if not sound:
            error_msg = self._get_error()
            logger.error(f"Failed to load sound '{name}' from '{path_str}': {error_msg}")
            raise AudioLoadError(f"Failed to load sound '{name}' from '{path_str}': {error_msg}")

        self.sounds[name] = sound
        logger.info(f"Sound effect loaded: {name} ({path_str})")

    def play_sound(self, name: str, loops: int = 0, volume: int = 128) -> int:  # pyright: ignore[reportReturnType]
        """Play a loaded sound effect.

        Args:
            name: Identifier of the sound to play.
            loops: Number of additional loops (0 = play once).
            volume: Volume level for this sound (0-128).

        Returns:
            Channel number used for playback, or -1 if failed.

        Raises:
            AudioPlaybackError: If the sound is not loaded.
        """
        if name not in self.sounds:
            logger.error(f"Cannot play sound '{name}': not loaded")
            raise AudioPlaybackError(f"Sound '{name}' not loaded")

        # Set volume for this specific chunk
        volume = self._clamp_volume(volume)
        mixer.Mix_VolumeChunk(self.sounds[name], volume)  # pyright: ignore

        # Find available channel in our range, or use any (-1)
        channel = -1
        for ch in range(self._channel_offset, self._channel_end):
            if not mixer.Mix_Playing(ch):  # pyright: ignore
                channel = ch
                break

        # If no channel in our range is free, let SDL find any available
        if channel == -1:
            channel = -1  # SDL will find first available globally

        result = mixer.Mix_PlayChannel(channel, self.sounds[name], loops)  # pyright: ignore
        if result == -1:
            error_msg = self._get_error()
            logger.warning(f"Failed to play sound '{name}': {error_msg}")
        else:
            relative_channel = result - self._channel_offset if result >= self._channel_offset else result
            logger.debug(
                f"Playing sound '{name}' on channel {relative_channel} (global: {result}, volume: {volume}/128)"
            )

        return result  # pyright: ignore[reportReturnType]

    def stop_sound(self, channel: int) -> None:
        """Stop playback on a specific channel.

        Args:
            channel: Channel number to stop (global channel index).
        """
        mixer.Mix_HaltChannel(channel)  # pyright: ignore
        logger.debug(f"Stopped sound on channel {channel}")

    def stop_all_sounds(self) -> None:
        """Stop playback on all channels managed by this instance."""
        for channel in range(self._channel_offset, self._channel_end):
            mixer.Mix_HaltChannel(channel)  # pyright: ignore
        logger.debug(f"Stopped all sound effects (channels {self._channel_offset}-{self._channel_end - 1})")

    def set_sfx_volume(self, volume: int) -> None:
        """Set the volume for all sound effect channels.

        Args:
            volume: Volume level (0-128, where 128 is maximum).
        """
        volume = self._clamp_volume(volume)
        for channel in range(self._channel_offset, self._channel_end):
            mixer.Mix_Volume(channel, volume)  # pyright: ignore
        logger.debug(f"SFX volume set to {volume}/128 for channels {self._channel_offset}-{self._channel_end - 1}")

    def get_sfx_volume(self) -> int:
        """Get the current sound effects volume.

        Returns:
            Current volume level (0-128) from first channel in range.
        """
        return mixer.Mix_Volume(self._channel_offset, -1)  # pyright: ignore

    def cleanup(self) -> None:
        """Free all loaded sound effects."""
        for _, sound in self.sounds.items():
            mixer.Mix_FreeChunk(sound)  # pyright: ignore
        self.sounds.clear()
        logger.info("SoundEffectManager cleaned up")
