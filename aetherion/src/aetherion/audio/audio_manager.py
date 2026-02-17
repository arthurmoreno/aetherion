"""Audio module for SDL2-based music and sound effects management.

This module provides classes and functions for initializing SDL2 audio,
managing background music playback, and handling sound effects using SDL2_mixer.
"""

from ctypes import c_void_p
from typing import Optional

import sdl2
import sdl2.sdlmixer as mixer
from aetherion.logger import logger

from aetherion.audio.base_manager import BaseAudioManager
from aetherion.audio.exceptions import AudioLoadError, AudioPlaybackError


class MusicAndAmbienceManager(BaseAudioManager):
    """Manages background music and ambient sound loops using SDL2_mixer.

    Handles:
    - Single background music track (using dedicated music channel)
    - Multiple ambient sound loops (using reserved SFX channels)

    Music uses SDL2's dedicated music channel, while ambient sounds use
    regular sound effect channels for simultaneous playback.
    """

    def __init__(self, ambient_channels: int = 4, channel_offset: int = 0) -> None:
        """Initialize the MusicAndAmbienceManager.

        Args:
            ambient_channels: Number of channels to reserve for ambient sounds (default: 4).
            channel_offset: Starting channel index for ambient sounds (default: 0).
        """
        self.music: Optional[c_void_p] = None
        self._current_file: Optional[str] = None

        # Ambient sound management
        self.ambient_sounds: dict[str, c_void_p] = {}
        self._ambient_channels = ambient_channels
        self._channel_offset = channel_offset
        self._channel_end = channel_offset + ambient_channels
        self._playing_ambient: dict[int, str] = {}  # channel -> sound name mapping

        # Ensure enough channels are allocated globally
        current_channels = mixer.Mix_AllocateChannels(-1)  # pyright: ignore
        if current_channels < self._channel_end:
            mixer.Mix_AllocateChannels(self._channel_end)  # pyright: ignore

        logger.info(
            f"MusicAndAmbienceManager initialized: {ambient_channels} ambient channels (offset: {channel_offset})"
        )

    def load_music(self, file_path: str | bytes) -> None:
        """Load a music file for playback.

        Args:
            file_path: Path to the music file (MP3, OGG, WAV, etc.).

        Raises:
            AudioLoadError: If the music file cannot be loaded.
        """
        # Free previous music if loaded
        if self.music:
            mixer.Mix_FreeMusic(self.music)  # pyright: ignore
            self.music = None

        path_bytes = self._to_path_bytes(file_path)
        path_str = self._to_path_str(file_path)

        self.music = mixer.Mix_LoadMUS(path_bytes)  # pyright: ignore
        if not self.music:  # pyright: ignore[reportUnnecessaryComparison]
            error_msg = self._get_error()
            logger.error(f"Failed to load music file '{path_str}': {error_msg}")
            raise AudioLoadError(f"Failed to load music file '{path_str}': {error_msg}")

        self._current_file = path_str
        logger.info(f"Music loaded: {path_str}")

    def play_music(self, loops: int = -1) -> None:
        """Play the loaded music.

        Args:
            loops: Number of times to loop (-1 for infinite, 0 for once).

        Raises:
            AudioPlaybackError: If no music is loaded or playback fails.
        """
        if not self.music:  # pyright: ignore[reportUnnecessaryComparison]
            logger.error("Cannot play music: no music file loaded")
            raise AudioPlaybackError("No music file loaded")

        result = mixer.Mix_PlayMusic(self.music, loops)  # pyright: ignore
        if result == -1:
            error_msg = self._get_error()
            logger.error(f"Mix_PlayMusic failed: {error_msg}")
            raise AudioPlaybackError(f"Mix_PlayMusic failed: {error_msg}")

        loop_desc = "infinite loop" if loops == -1 else f"{loops + 1} time(s)"
        logger.info(f"Music playback started: {self._current_file} ({loop_desc})")

    def pause_music(self) -> None:
        """Pause music playback if currently playing."""
        mixer.Mix_PauseMusic()  # pyright: ignore
        logger.debug("Music paused")

    def resume_music(self) -> None:
        """Resume music playback if currently paused."""
        mixer.Mix_ResumeMusic()  # pyright: ignore
        logger.debug("Music resumed")

    def stop_music(self) -> None:
        """Stop music playback immediately."""
        mixer.Mix_HaltMusic()  # pyright: ignore
        logger.debug("Music stopped")

    def fade_in_music(self, loops: int = -1, fade_ms: int = 1000) -> None:
        """Play music with a fade-in effect.

        Args:
            loops: Number of times to loop (-1 for infinite).
            fade_ms: Fade-in duration in milliseconds.

        Raises:
            AudioPlaybackError: If no music is loaded or playback fails.
        """
        if not self.music:  # pyright: ignore[reportUnnecessaryComparison]
            logger.error("Cannot fade in music: no music file loaded")
            raise AudioPlaybackError("No music file loaded")

        result = mixer.Mix_FadeInMusic(self.music, loops, fade_ms)  # pyright: ignore
        if result == -1:
            error_msg = self._get_error()
            logger.error(f"Mix_FadeInMusic failed: {error_msg}")
            raise AudioPlaybackError(f"Mix_FadeInMusic failed: {error_msg}")

        logger.info(f"Music fade-in started: {fade_ms}ms")

    def fade_out_music(self, fade_ms: int = 1000) -> None:
        """Fade out and stop music playback.

        Args:
            fade_ms: Fade-out duration in milliseconds.
        """
        mixer.Mix_FadeOutMusic(fade_ms)  # pyright: ignore
        logger.debug(f"Music fade-out started: {fade_ms}ms")

    def set_music_volume(self, volume: int) -> None:
        """Set the music volume.

        Args:
            volume: Volume level (0-128, where 128 is maximum).
        """
        volume = self._clamp_volume(volume)
        mixer.Mix_VolumeMusic(volume)  # pyright: ignore
        logger.debug(f"Music volume set to {volume}/128")

    def get_music_volume(self) -> int:
        """Get the current music volume.

        Returns:
            Current volume level (0-128).
        """
        return mixer.Mix_VolumeMusic(-1)  # pyright: ignore

    def is_music_playing(self) -> bool:
        """Check if music is currently playing.

        Returns:
            True if music is playing, False otherwise.
        """
        return mixer.Mix_PlayingMusic() != 0  # pyright: ignore

    def is_music_paused(self) -> bool:
        """Check if music is currently paused.

        Returns:
            True if music is paused, False otherwise.
        """
        return mixer.Mix_PausedMusic() != 0  # pyright: ignore

    def load_ambient(self, name: str, file_path: str | bytes) -> None:
        """Load an ambient sound file for looping playback.

        Args:
            name: Identifier for the ambient sound.
            file_path: Path to the sound file (WAV, OGG, etc.).

        Raises:
            AudioLoadError: If the sound file cannot be loaded.
        """
        # Free previous sound if it exists
        if name in self.ambient_sounds:
            mixer.Mix_FreeChunk(self.ambient_sounds[name])  # pyright: ignore

        path_bytes = self._to_path_bytes(file_path)
        path_str = self._to_path_str(file_path)

        sound = mixer.Mix_LoadWAV(path_bytes)  # pyright: ignore
        if not sound:
            error_msg = self._get_error()
            logger.error(f"Failed to load ambient sound '{name}' from '{path_str}': {error_msg}")
            raise AudioLoadError(f"Failed to load ambient sound '{name}' from '{path_str}': {error_msg}")

        self.ambient_sounds[name] = sound
        logger.info(f"Ambient sound loaded: {name} ({path_str})")

    def play_ambient(self, name: str, loops: int = -1, volume: int = 128) -> int:  # pyright: ignore[reportReturnType]
        """Play an ambient sound on a reserved channel.

        Args:
            name: Identifier of the ambient sound to play.
            loops: Number of additional loops (-1 for infinite, 0 for once).
            volume: Volume level for this ambient sound (0-128).

        Returns:
            Channel number used for playback, or -1 if failed.

        Raises:
            AudioPlaybackError: If the sound is not loaded or no channels available.
        """
        if name not in self.ambient_sounds:
            logger.error(f"Cannot play ambient sound '{name}': not loaded")
            raise AudioPlaybackError(f"Ambient sound '{name}' not loaded")

        # Find an available channel in our range
        channel = -1
        for ch in range(self._channel_offset, self._channel_end):
            if not mixer.Mix_Playing(ch):  # pyright: ignore
                channel = ch
                break

        if channel == -1:
            logger.warning(f"No available ambient channels to play '{name}'")
            return -1

        # Set volume for this specific chunk
        volume = self._clamp_volume(volume)
        mixer.Mix_VolumeChunk(self.ambient_sounds[name], volume)  # pyright: ignore

        result = mixer.Mix_PlayChannel(channel, self.ambient_sounds[name], loops)  # pyright: ignore
        if result == -1:
            error_msg = self._get_error()
            logger.warning(f"Failed to play ambient sound '{name}': {error_msg}")
        else:
            self._playing_ambient[result] = name
            logger.info(f"Playing ambient sound '{name}' on channel {result} (volume: {volume}/128)")

        return result  # pyright: ignore[reportReturnType]

    def stop_ambient(self, name: str) -> None:
        """Stop a specific ambient sound by name.

        Args:
            name: Identifier of the ambient sound to stop.
        """
        # Find and stop all channels playing this ambient sound
        channels_stopped = []
        for channel, playing_name in list(self._playing_ambient.items()):
            if playing_name == name:
                mixer.Mix_HaltChannel(channel)  # pyright: ignore
                channels_stopped.append(channel)
                del self._playing_ambient[channel]

        if channels_stopped:
            logger.debug(f"Stopped ambient sound '{name}' on channels: {channels_stopped}")
        else:
            logger.debug(f"Ambient sound '{name}' was not playing")

    def stop_all_ambient(self) -> None:
        """Stop all ambient sounds."""
        for channel in range(self._channel_offset, self._channel_end):
            mixer.Mix_HaltChannel(channel)  # pyright: ignore
        self._playing_ambient.clear()
        logger.debug(f"Stopped all ambient sounds (channels {self._channel_offset}-{self._channel_end - 1})")

    def fade_ambient(self, name: str, fade_ms: int) -> None:
        """Fade out and stop a specific ambient sound.

        Args:
            name: Identifier of the ambient sound to fade out.
            fade_ms: Fade-out duration in milliseconds.
        """
        for channel, playing_name in self._playing_ambient.items():
            if playing_name == name:
                mixer.Mix_FadeOutChannel(channel, fade_ms)  # pyright: ignore
                logger.debug(f"Fading out ambient sound '{name}' on channel {channel} ({fade_ms}ms)")

    def set_ambient_volume(self, volume: int) -> None:
        """Set the volume for all ambient sound channels.

        Args:
            volume: Volume level (0-128, where 128 is maximum).
        """
        volume = self._clamp_volume(volume)
        for channel in range(self._channel_offset, self._channel_end):
            mixer.Mix_Volume(channel, volume)  # pyright: ignore
        logger.debug(f"Ambient volume set to {volume}/128 for channels {self._channel_offset}-{self._channel_end - 1}")

    def cleanup(self) -> None:
        """Clean up audio resources and shut down SDL2."""
        # Stop all ambient sounds
        self.stop_all_ambient()

        # Free ambient sounds
        for _, sound in self.ambient_sounds.items():
            mixer.Mix_FreeChunk(sound)  # pyright: ignore
        self.ambient_sounds.clear()

        # Free music
        if self.music:
            mixer.Mix_FreeMusic(self.music)  # pyright: ignore
            self.music = None

        mixer.Mix_CloseAudio()  # pyright: ignore
        mixer.Mix_Quit()  # pyright: ignore
        sdl2.SDL_Quit()  # pyright: ignore
        logger.info("MusicAndAmbienceManager cleaned up")


def get_audio_status() -> dict[str, int | bool]:
    """Get diagnostic information about the audio system.

    Returns:
        Dictionary containing audio system status including:
        - num_channels: Number of allocated mixing channels
        - music_playing: Whether music is currently playing
        - music_paused: Whether music is currently paused
        - music_volume: Current music volume (0-128)
    """
    return {
        "num_channels": mixer.Mix_AllocateChannels(-1),  # pyright: ignore
        "music_playing": mixer.Mix_PlayingMusic() != 0,  # pyright: ignore
        "music_paused": mixer.Mix_PausedMusic() != 0,  # pyright: ignore
        "music_volume": mixer.Mix_VolumeMusic(-1),  # pyright: ignore
    }


__all__ = [
    "MusicAndAmbienceManager",
    "get_audio_status",
]

# Backward compatibility alias
AudioManager = MusicAndAmbienceManager
