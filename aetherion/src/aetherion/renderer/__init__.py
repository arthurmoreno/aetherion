import ctypes

import sdl2
from sdl2.render import SDL_Renderer

from aetherion import BasicGameWindow, GameWindow, OpenGLGameWindow

from .fonts import Font as Font
from .text import Text as Text


class Renderer:
    _renderer: SDL_Renderer
    renderer_ptr: int

    def __init__(self, game_window: BasicGameWindow) -> None:
        self._renderer = sdl2.SDL_CreateRenderer(game_window.window, -1, 0)
        self.renderer_ptr = ctypes.cast(self._renderer, ctypes.c_void_p).value


class RendererOpenGL:
    _renderer: SDL_Renderer
    renderer_ptr: int

    def __init__(self, game_window: OpenGLGameWindow) -> None:
        flags = sdl2.SDL_RENDERER_ACCELERATED | sdl2.SDL_RENDERER_PRESENTVSYNC
        self._renderer = sdl2.SDL_CreateRenderer(game_window.window, -1, flags)
        self.renderer_ptr = ctypes.cast(self._renderer, ctypes.c_void_p).value


singleton_instance = None


def get_renderer(game_window: GameWindow) -> Renderer:
    global singleton_instance
    if singleton_instance is None:
        if isinstance(game_window, BasicGameWindow):
            singleton_instance = Renderer(game_window)
        elif isinstance(game_window, OpenGLGameWindow):
            singleton_instance = RendererOpenGL(game_window)
        else:
            raise TypeError("Unsupported GameWindow type for renderer creation")
    return singleton_instance
