# Custom exceptions
class AudioInitError(Exception):
    """Raised when SDL2 or SDL2_mixer initialization fails."""

    pass


class AudioLoadError(Exception):
    """Raised when loading audio files fails."""

    pass


class AudioPlaybackError(Exception):
    """Raised when audio playback operations fail."""

    pass
