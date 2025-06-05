import sdl2


class Font:
    def __init__(self, font_path, font_size):
        self.font_path = font_path
        self.font_size = font_size
        self._font = sdl2.sdlttf.TTF_OpenFont(font_path, font_size)
