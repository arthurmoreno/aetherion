import pytest

from aetherion import PhysicsSettings, PhysicsStats


def test_physics_stats():
    stats = PhysicsStats()
    stats.mass = 10.0
    stats.max_speed = 5.0
    stats.min_speed = 0.1
    stats.force_x = 1.0
    stats.force_y = 2.0
    stats.force_z = 3.0

    assert stats.mass == pytest.approx(10.0)
    assert stats.max_speed == pytest.approx(5.0)
    assert stats.min_speed == pytest.approx(0.1)
    assert stats.force_x == pytest.approx(1.0)
    assert stats.force_y == pytest.approx(2.0)
    assert stats.force_z == pytest.approx(3.0)


def test_physics_settings():
    settings = PhysicsSettings()
    settings.set_gravity(9.8)
    settings.set_friction(0.5)
    settings.set_allow_multi_direction(True)

    assert settings.get_gravity() == pytest.approx(9.8)
    assert settings.get_friction() == pytest.approx(0.5)
    assert settings.get_allow_multi_direction() is True
