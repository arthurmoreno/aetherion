import aetherion


class Sprite:
    def __init__(self, renderer, sprite_id, sprite_image_path, x=0, y=0, scale_x=0, scale_y=0):
        self.sprite_id = sprite_id
        self.renderer = renderer
        aetherion.load_texture_on_manager(
            self.renderer.renderer_ptr, sprite_image_path, self.sprite_id, scale_x, scale_y
        )

        self.x = x
        self.y = y

    def render(self):
        aetherion.render_texture_from_manager(self.renderer.renderer_ptr, self.sprite_id, self.x, self.y)
