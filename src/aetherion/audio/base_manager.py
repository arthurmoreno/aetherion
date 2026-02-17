import sdl2.sdlmixer as mixer


class BaseAudioManager:
    """Base class for audio managers with common SDL2 mixer utilities.

    Provides shared functionality for error handling, path conversion,
    and SDL2_mixer primitive operations.
    """

    @staticmethod
    def _get_error() -> str:
        """Get the last SDL2_mixer error message as a string.

        Returns:
            The error message from SDL2_mixer.
        """
        error_msg = mixer.Mix_GetError()  # pyright: ignore
        if isinstance(error_msg, bytes):
            return error_msg.decode("utf-8")
        return str(error_msg)

    @staticmethod
    def _to_path_bytes(file_path: str | bytes) -> bytes:
        """Convert file path to bytes for SDL2_mixer.

        Args:
            file_path: Path as string or bytes.

        Returns:
            Path encoded as bytes.
        """
        return file_path if isinstance(file_path, bytes) else file_path.encode("utf-8")

    @staticmethod
    def _to_path_str(file_path: str | bytes) -> str:
        """Convert file path to string for logging.

        Args:
            file_path: Path as string or bytes.

        Returns:
            Path as string.
        """
        return file_path.decode("utf-8") if isinstance(file_path, bytes) else file_path

    @staticmethod
    def _clamp_volume(volume: int) -> int:
        """Clamp volume to SDL2_mixer valid range.

        Args:
            volume: Volume level to clamp.

        Returns:
            Volume clamped to 0-128 range.
        """
        return max(0, min(128, volume))
