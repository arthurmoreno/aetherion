import sdl2
import sdl2.sdlmixer as mixer

from aetherion.audio.exceptions import AudioInitError
from aetherion.logger import logger


def init_sdl() -> None:
    """Initialize SDL2 with video and audio subsystems.

    Raises:
        AudioInitError: If SDL2 initialization fails.
    """
    result = sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO | sdl2.SDL_INIT_AUDIO)  # pyright: ignore
    if result != 0:
        error_msg = sdl2.SDL_GetError()  # pyright: ignore
        if isinstance(error_msg, bytes):
            error_msg = error_msg.decode("utf-8")
        logger.error(f"SDL_Init failed: {error_msg}")
        raise AudioInitError(f"SDL_Init failed: {error_msg}")
    logger.info("SDL2 initialized successfully with audio support")


def init_mixer() -> None:
    """Initialize SDL2_mixer with MP3 and OGG support.

    Opens audio device with 44.1kHz, stereo, 4096 byte buffer.

    Raises:
        AudioInitError: If SDL2_mixer initialization or audio device opening fails.
    """
    # Initialize SDL_mixer with desired audio formats
    init_flags = mixer.MIX_INIT_MP3 | mixer.MIX_INIT_OGG  # pyright: ignore
    result = mixer.Mix_Init(init_flags)  # pyright: ignore
    if result != init_flags:
        error_msg = mixer.Mix_GetError()  # pyright: ignore
        if isinstance(error_msg, bytes):
            error_msg = error_msg.decode("utf-8")
        logger.error(f"Mix_Init failed: {error_msg}")
        mixer.Mix_Quit()  # pyright: ignore
        sdl2.SDL_Quit()  # pyright: ignore
        raise AudioInitError(f"Mix_Init failed: {error_msg}")

    logger.info("SDL2_mixer initialized with MP3 and OGG support")

    # Open audio device
    result = mixer.Mix_OpenAudio(44100, mixer.MIX_DEFAULT_FORMAT, 2, 4096)  # pyright: ignore
    if result != 0:
        error_msg = mixer.Mix_GetError()  # pyright: ignore
        if isinstance(error_msg, bytes):
            error_msg = error_msg.decode("utf-8")
        logger.error(f"Mix_OpenAudio failed: {error_msg}")
        mixer.Mix_Quit()  # pyright: ignore
        sdl2.SDL_Quit()  # pyright: ignore
        raise AudioInitError(f"Mix_OpenAudio failed: {error_msg}")

    logger.info("Audio device opened: 44.1kHz, stereo, 4096 buffer")
