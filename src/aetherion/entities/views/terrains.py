from typing import Any, Dict, Hashable

from aetherion.renderer import Renderer
from aetherion.renderer.image_set import ImageSet
from aetherion.renderer.sprites import Sprite
from aetherion.renderer.views import BaseView


class TerrainView(BaseView):
    name: str | None = None
    sprite_variation_name: Hashable | None = None
    sprite_id: str = "terrain_view"
    terrain_type: str = ""

    def __init__(
        self,
        renderer: Renderer,
        image_set: ImageSet,
        sprite_scale: float = 1.0,
        batch: Any | None = None,
        group: Any | None = None,
        *args: Any,
        **kwargs: Any,
    ) -> None:
        self.renderer: Renderer = renderer
        self.image_set: ImageSet = image_set
        self.sprite_scale: float = sprite_scale
        if batch is not None:
            self.batch = batch
        self.group = group
        # self.load_image()
        self.sprites: Dict[Hashable, Sprite] = {}

        self.create_sprites()

    def create_sprites(self) -> None:
        for key, image in self.image_set.images.items():
            sprite = Sprite(
                self.renderer,
                f"{self.terrain_type}{self.sprite_id}{key}",
                image,
                x=0,
                y=0,
                scale_x=int(32 * self.sprite_scale),
                scale_y=int(32 * self.sprite_scale),
            )

            self.sprites[key] = sprite

    def set_terrain_variation_sprite(self, terrain_variation: Hashable) -> None:
        self.sprite_variation_name = terrain_variation

    @property
    def sprite(self) -> Sprite | None:
        if self.sprite_variation_name is None:
            return None
        return self.sprites[self.sprite_variation_name]

    def set_sprite(self, sprite: Sprite) -> None:
        """Replace the currently-selected variation's Sprite instance.

        If no variation is selected, this will set the sprite under the
        `None` key.
        """
        key: Hashable = self.sprite_variation_name
        self.sprites[key] = sprite

    def to_raw(self) -> str | None:
        return self.name


class EmptyView(TerrainView):
    terrain_type = "empty"
