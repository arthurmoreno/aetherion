from typing import Optional

import sdl2

from aetherion.renderer.fonts import Font


class Text:
    def __init__(
        self,
        text: str,
        renderer,
        font_path: bytes = b"assets/Toriko.ttf",
        font_size: int = 32,
        color: tuple[int, int, int] = (255, 255, 255),
    ):
        """
        Initializes a Text object.

        Args:
            text (str): The text to render.
            renderer: The renderer object responsible for drawing.
            font_path (bytes, optional): Path to the TTF font file. Defaults to b"assets/Toriko.ttf".
            font_size (int, optional): Size of the font. Defaults to 32.
            color (tuple, optional): The color of the text in RGB format. Defaults to white.
        """
        self.renderer = renderer
        self.font = Font(font_path, font_size)
        self.color = color  # RGB tuple
        self.text = text

        # Initialize SDL_Color
        self.text_color = sdl2.SDL_Color(*self.color)

        # Initialize texture and dimensions
        self.text_texture: Optional[sdl2.SDL_Texture] = None
        self.text_width: int = 0
        self.text_height: int = 0

        # Render the initial text
        self._render_text()

    def _render_text(self):
        """
        Renders the text to an SDL_Texture.
        """
        # Render text to a surface
        surface = sdl2.sdlttf.TTF_RenderUTF8_Blended(self.font._font, self.text.encode("utf-8"), self.text_color)
        if not surface:
            print("TTF_RenderUTF8_Blended Error:", sdl2.sdlttf.TTF_GetError())
            return

        # Create texture from surface
        self.text_texture = sdl2.SDL_CreateTextureFromSurface(self.renderer._renderer, surface)
        if not self.text_texture:
            print("SDL_CreateTextureFromSurface Error:", sdl2.SDL_GetError())
            sdl2.SDL_FreeSurface(surface)
            return

        # Get width and height
        self.text_width = surface.contents.w
        self.text_height = surface.contents.h
        sdl2.SDL_FreeSurface(surface)

    def set_position(self, x: int, y: int):
        """
        Sets the position where the text will be rendered.

        Args:
            x (int): The x-coordinate for the text.
            y (int): The y-coordinate for the text.
        """
        self.x = x
        self.y = y

    def set_text(self, new_text: str):
        """
        Updates the text content and re-renders the texture.

        Args:
            new_text (str): The new text to display.
        """
        self.text = new_text
        self._render_text()

    def set_color(self, color: tuple[int, int, int]):
        """
        Sets the color of the text and re-renders the texture.

        Args:
            color (tuple): The new color in RGB format.
        """
        self.color = color
        self.text_color = sdl2.SDL_Color(*self.color)
        self._render_text()

    def render(self, x: int, y: int):
        """
        Renders the text at the specified position.

        Args:
            x (int): The x-coordinate for the text.
            y (int): The y-coordinate for the text.
        """
        if not self.text_texture:
            return

        # Set destination rectangle
        dst_rect = sdl2.SDL_Rect(x, y, self.text_width, self.text_height)

        # Render the texture
        if sdl2.SDL_RenderCopy(self.renderer._renderer, self.text_texture, None, dst_rect) != 0:
            print("SDL_RenderCopy Error:", sdl2.SDL_GetError())

    def destroy(self):
        """
        Cleans up the text texture and font resources.
        """
        if self.text_texture:
            sdl2.SDL_DestroyTexture(self.text_texture)
            self.text_texture = None
        if self.font:
            self.font.destroy()
            self.font = None
