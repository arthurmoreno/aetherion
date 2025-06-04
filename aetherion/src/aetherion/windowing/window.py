from abc import ABC, abstractmethod

import sdl2
from sdl2 import (
    SDL_GL_BLUE_SIZE,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_GREEN_SIZE,
    SDL_GL_RED_SIZE,
    SDL_GL_CreateContext,
    SDL_GL_DeleteContext,
    SDL_GL_SetAttribute,
    SDL_GL_SwapWindow,
    SDL_Window,
)


class GameWindow(ABC):
    """Interface for SDL windows."""

    @abstractmethod
    def show(self) -> None: ...

    @abstractmethod
    def hide(self) -> None: ...

    @abstractmethod
    def refresh_size(self) -> None: ...

    @abstractmethod
    def get_size(self) -> tuple[int, int]: ...

    @abstractmethod
    def close(self) -> None: ...

    def swap_buffers(self) -> None:
        """Optional: swap buffers for OpenGL windows."""
        pass


# Basic SDL window (no OpenGL context)
class BasicGameWindow(GameWindow):
    def __init__(self, title: str, width: int, height: int, flags: int = 0):
        self.window: SDL_Window = sdl2.SDL_CreateWindow(
            title.encode("utf-8"),
            sdl2.SDL_WINDOWPOS_CENTERED,
            sdl2.SDL_WINDOWPOS_CENTERED,
            width,
            height,
            flags,
        )
        if not self.window:
            raise RuntimeError(f"SDL_CreateWindow failed: {sdl2.SDL_GetError().decode()}")
        self.width: int = width
        self.height: int = height
        self.title: str = title
        self.flags: int = flags

    def show(self) -> None:
        sdl2.SDL_ShowWindow(self.window)

    def hide(self) -> None:
        sdl2.SDL_HideWindow(self.window)

    def refresh_size(self) -> None:
        w, h = self.window.size
        self.width, self.height = int(w), int(h)

    def get_size(self) -> tuple[int, int]:
        return self.window.size

    def close(self) -> None:
        self.window.close()


# SDL window with its own OpenGL context
class OpenGLGameWindow(GameWindow):
    def __init__(self, title: str, width: int, height: int, flags: int = 0):
        # Set GL attributes before creating window
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8)
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8)
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8)
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)

        self.window: SDL_Window = sdl2.SDL_CreateWindow(
            title.encode("utf-8"),
            sdl2.SDL_WINDOWPOS_CENTERED,
            sdl2.SDL_WINDOWPOS_CENTERED,
            width,
            height,
            sdl2.SDL_WINDOW_OPENGL | flags,
        )
        if not self.window:
            raise RuntimeError(f"SDL_CreateWindow failed: {sdl2.SDL_GetError().decode()}")

        self.width: int = width
        self.height: int = height
        self.title: str = title
        self.flags: int = flags

        # Create GL context
        self.gl_context = SDL_GL_CreateContext(self.window)
        if not self.gl_context:
            raise RuntimeError(f"SDL_GL_CreateContext failed: {sdl2.SDL_GetError().decode()}")

    def show(self) -> None:
        sdl2.SDL_ShowWindow(self.window)

    def hide(self) -> None:
        sdl2.SDL_HideWindow(self.window)

    def refresh_size(self) -> None:
        w, h = self.window.size
        self.width, self.height = int(w), int(h)

    def get_size(self) -> tuple[int, int]:
        return self.window.size

    def swap_buffers(self) -> None:
        SDL_GL_SwapWindow(self.window)

    def close(self) -> None:
        if hasattr(self, "gl_context") and self.gl_context:
            SDL_GL_DeleteContext(self.gl_context)
            self.gl_context = None
        self.window.close()
