from aetherion.world.constants import WorldInstanceTypes


def _minimal_world_create_payload():
    return {
        "world_name": "New World",
        "world_factory_name": "dungeon",
        "world_description": "A fresh world ready for exploration",
        "world_config": {
            "type": WorldInstanceTypes.SYNCHRONOUS,
            "world_seed": "1234567890",
            "world_height": 100,
            "world_width": 100,
            "world_depth": 10,
            "gravity": 5.0,
            "friction": 1.0,
            "allow_multi_direction": True,
            "evaporation_coefficient": 8.0,
            "heat_to_water_evaporation": 120.0,
            "water_minimum_units": 30000,
            "metabolism_cost_to_apply_force": 1.9999999949504854e-06,
        },
    }


def test_world_create_payload_contains_required_top_level_keys():
    payload = _minimal_world_create_payload()
    required_keys = {"world_name", "world_factory_name", "world_description", "world_config"}

    assert required_keys.issubset(payload.keys())


def test_world_create_payload_world_config_contains_required_simulation_keys():
    world_config = _minimal_world_create_payload()["world_config"]
    required_config_keys = {
        "type",
        "world_seed",
        "world_height",
        "world_width",
        "world_depth",
        "gravity",
        "friction",
        "allow_multi_direction",
        "evaporation_coefficient",
        "heat_to_water_evaporation",
        "water_minimum_units",
        "metabolism_cost_to_apply_force",
    }

    assert required_config_keys.issubset(world_config.keys())
    assert world_config["type"] == WorldInstanceTypes.SYNCHRONOUS
