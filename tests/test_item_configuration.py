from aetherion import ItemConfiguration


def test_item_configuration():
    config = ItemConfiguration("item_001")
    assert config.get_item_id() == "item_001"

    config.set_default_value("health", 100)
    config.set_default_value("name", "Apple")
    config.set_default_value("weight", 0.5)

    assert config.get_default_value("health") == 100
    assert config.get_default_value("name") == "Apple"
    assert config.get_default_value("weight") == 0.5
    assert config.get_default_value("nonexistent") is None

    config.set_in_game_textures(["tex1", "tex2"])
    assert config.get_in_game_textures() == ["tex1", "tex2"]

    config.set_inventory_textures(["inv1"])
    assert config.get_inventory_textures() == ["inv1"]
