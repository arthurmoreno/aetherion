import sdl2


class Rectangle:
    def __init__(self, renderer, x: int, y: int, width: int, height: int, color=(255, 255, 255)):
        """
        Initializes a Rectangle object.

        Args:
            renderer: The renderer object responsible for drawing.
            x (int): The x-coordinate of the rectangle's top-left corner.
            y (int): The y-coordinate of the rectangle's top-left corner.
            width (int): The width of the rectangle.
            height (int): The height of the rectangle.
            color (tuple, optional): The color of the rectangle in RGB format. Defaults to white.
        """
        self.renderer = renderer
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.color = color  # RGB tuple

        # Initialize the SDL_Rect
        self.rect = sdl2.SDL_Rect(x, y, width, height)

    def set_position(self, x: int, y: int):
        """
        Sets the position of the rectangle.

        Args:
            x (int): The new x-coordinate.
            y (int): The new y-coordinate.
        """
        self.x = x
        self.y = y
        self.rect.x = x
        self.rect.y = y

    def set_size(self, width: int, height: int):
        """
        Sets the size of the rectangle.

        Args:
            width (int): The new width.
            height (int): The new height.
        """
        self.width = width
        self.height = height
        self.rect.w = width
        self.rect.h = height

    def set_color(self, color: tuple):
        """
        Sets the color of the rectangle.

        Args:
            color (tuple): The new color in RGB format.
        """
        self.color = color

    def render(self):
        """
        Renders the rectangle to the screen.
        """
        # Set the drawing color
        sdl2.SDL_SetRenderDrawColor(
            self.renderer._renderer,
            self.color[0],
            self.color[1],
            self.color[2],
            255,  # Alpha channel (opaque)
        )

        # Render the filled rectangle
        if sdl2.SDL_RenderFillRect(self.renderer._renderer, self.rect) != 0:
            print("SDL_RenderFillRect Error:", sdl2.SDL_GetError())

    def render_outline(self, thickness: int = 1):
        """
        Renders the outline of the rectangle with the specified thickness.

        Args:
            thickness (int, optional): The thickness of the outline. Defaults to 1.
        """
        # Set the drawing color
        sdl2.SDL_SetRenderDrawColor(
            self.renderer._renderer,
            self.color[0],
            self.color[1],
            self.color[2],
            255,  # Alpha channel (opaque)
        )

        for i in range(thickness):
            # Adjust the rectangle size for each layer of the outline
            outline_rect = sdl2.SDL_Rect(self.rect.x - i, self.rect.y - i, self.rect.w + 2 * i, self.rect.h + 2 * i)
            if sdl2.SDL_RenderDrawRect(self.renderer._renderer, outline_rect) != 0:
                print("SDL_RenderDrawRect Error:", sdl2.SDL_GetError())

    def destroy(self):
        """
        Cleans up resources used by the rectangle, if any.
        """
        # Currently, SDL_Rect doesn't require explicit destruction.
        # Implement if you add resources that need manual cleanup.
        pass
