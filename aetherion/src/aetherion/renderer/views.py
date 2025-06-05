from .sprites import Sprite
from .utils import apply_lighting_to_sprite


class BaseView:
    sprite: Sprite

    def update(
        self, screen_x: int | None = None, screen_y: int | None = None, batch=None, group=None, light_intensity=1
    ) -> None:
        self.sprite.batch = batch
        self.sprite.group = group

        apply_lighting_to_sprite(self.sprite, light_intensity)
        self.sprite.position = (screen_x, screen_y, 0)
        self.sprite.visible = True
