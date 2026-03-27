from types import SimpleNamespace

from pytest import MonkeyPatch

from aetherion.entities.items import item_configuration as item_configuration_module


def test_register_all_items_registers_every_item_from_configuration(monkeypatch: MonkeyPatch):
    registered_item_ids = []

    class FakeBoundItemConfiguration:
        def __init__(self, item_id: str):
            self.item_id = item_id
            self.in_game_textures = []
            self.inventory_textures = []
            self.default_values = {}

        def set_in_game_textures(self, textures):
            self.in_game_textures = list(textures)

        def set_inventory_textures(self, textures):
            self.inventory_textures = list(textures)

        def set_default_value(self, key, value):
            self.default_values[key] = value

    class FakeItemConfigurationModel:
        @staticmethod
        def parse_obj(data):
            return SimpleNamespace(
                id=data["id"],
                in_game_textures=data["in_game_textures"],
                inventory_textures=data["inventory_textures"],
                default_values=data["default_values"],
            )

    def fake_register_item_configuration(item):
        registered_item_ids.append(item.item_id)
        return 1

    monkeypatch.setattr(item_configuration_module, "ItemConfiguration", FakeItemConfigurationModel)
    monkeypatch.setattr(item_configuration_module.aetherion, "ItemConfiguration", FakeBoundItemConfiguration)
    monkeypatch.setattr(
        item_configuration_module.aetherion, "register_item_configuration", fake_register_item_configuration
    )

    handler = item_configuration_module.ItemConfigurationHandler()
    items_configurations = [
        {
            "id": "food_apple",
            "in_game_textures": ["apple_world.png"],
            "inventory_textures": ["apple_inventory.png"],
            "default_values": {"health": 5},
        },
        {
            "id": "tool_pickaxe",
            "in_game_textures": ["pickaxe_world.png"],
            "inventory_textures": ["pickaxe_inventory.png"],
            "default_values": {"durability": 100},
        },
    ]

    handler.register_all_items(items_configurations)

    assert registered_item_ids == ["food_apple", "tool_pickaxe"]
    assert handler.get_item_configuration("food_apple").default_values["health"] == 5
    assert handler.get_item_configuration("tool_pickaxe").default_values["durability"] == 100
