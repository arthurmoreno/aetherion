import sdl2

from sdl2 import SDL_Window


class GameWindow:
    def __init__(self, title: str, width: int, height: int, flags: int=0):
        # Create the SDL2 window
        self.window: SDL_Window = sdl2.SDL_CreateWindow(
            title.encode("utf-8"),
            sdl2.SDL_WINDOWPOS_CENTERED,
            sdl2.SDL_WINDOWPOS_CENTERED,
            width,
            height,
            sdl2.SDL_WINDOW_OPENGL | flags,
        )
        self.width: int = width
        self.height: int = height
        self.title: str = title
        self.flags: int = flags

    def show(self) -> None:
        pass
        # self.window.show()

    def hide(self) -> None:
        self.window.hide()

    def refresh_size(self) -> None:
        # Update width and height properties
        size: tuple[int, int] = self.window.size
        self.width, self.height = size

    def get_size(self) -> tuple[int, int]:
        return self.window.size

    def close(self) -> None:
        self.window.close()

