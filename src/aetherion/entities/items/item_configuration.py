from typing import Any

import aetherion
from aetherion.entities.items.models import ItemConfiguration
from aetherion.logger import logger


class ItemConfigurationHandler:
    def __init__(self) -> None:
        # mapping: item_id (str) -> ItemConfiguration (from aetherion)
        self.item_configuration: dict[str, Any] = {}

    def register_item_configuration(self, json_item_configuration: ItemConfiguration | dict[str, Any]) -> None:
        # accept either a dict-like JSON or an ItemConfiguration model
        if isinstance(json_item_configuration, ItemConfiguration):
            cfg = json_item_configuration
        else:
            cfg = ItemConfiguration.parse_obj(json_item_configuration)

        item_id = cfg.id
        in_game_textures = cfg.in_game_textures
        inventory_textures = cfg.inventory_textures

        default_values = cfg.default_values

        item = aetherion.ItemConfiguration(str(item_id))
        item.set_in_game_textures(in_game_textures)
        item.set_inventory_textures(inventory_textures)

        for key, value in default_values.items():
            item.set_default_value(key, value)

        # Register the item configuration and get a handle (int pointer-like)
        handle = aetherion.register_item_configuration(item)
        # store locally for quick access
        self.item_configuration[str(item_id)] = item
        logger.info(f"Registered item configuration for item, handle received: {handle}")

    def register_all_items(self, items_configurations: list[ItemConfiguration]) -> None:
        for item_configuration in items_configurations:
            self.register_item_configuration(item_configuration)

    def get_item_configuration(self, item_id: int | str) -> Any | None:
        return self.item_configuration.get(str(item_id))
