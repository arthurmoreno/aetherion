from typing import Any

from aetherion.engine_config import EngineConfig


class DummyController:
    pass


def _dummy_input_map_factory(_connection: Any) -> dict[str, object]:
    return {}


def test_engine_config_stores_screen_and_fps():
    config = EngineConfig(
        screen_width=1920,
        screen_height=1080,
        fps=75,
        user_input_controller_class=DummyController,
        player_input_map_factory=_dummy_input_map_factory,
    )

    assert config.screen_width == 1920
    assert config.screen_height == 1080
    assert config.fps == 75


def test_engine_config_requires_input_controller_class():
    config = EngineConfig(
        user_input_controller_class=DummyController,
        player_input_map_factory=_dummy_input_map_factory,
    )
    assert config.user_input_controller_class is DummyController

    config_without_controller = EngineConfig(player_input_map_factory=_dummy_input_map_factory)
    assert config_without_controller.user_input_controller_class is None


def test_engine_config_requires_player_input_map_factory():
    config = EngineConfig(
        user_input_controller_class=DummyController,
        player_input_map_factory=_dummy_input_map_factory,
    )
    assert callable(config.player_input_map_factory)

    config_without_factory = EngineConfig(user_input_controller_class=DummyController)
    assert config_without_factory.player_input_map_factory is None
