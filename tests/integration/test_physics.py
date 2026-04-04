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


class TestWaterSimulationToggles:
    """Tests for the water simulation phase toggles on PhysicsSettings."""

    def _reset_toggles(self, settings: PhysicsSettings) -> None:
        settings.set_simulate_vapor_condensation(True)
        settings.set_simulate_vapor_movement(True)
        settings.set_simulate_water_movement(True)
        settings.set_simulate_water_evaporation(True)

    def test_defaults_are_all_on(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        assert settings.get_simulate_vapor_condensation() is True
        assert settings.get_simulate_vapor_movement() is True
        assert settings.get_simulate_water_movement() is True
        assert settings.get_simulate_water_evaporation() is True

    def test_toggle_vapor_condensation(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        settings.set_simulate_vapor_condensation(False)
        assert settings.get_simulate_vapor_condensation() is False

        settings.set_simulate_vapor_condensation(True)
        assert settings.get_simulate_vapor_condensation() is True

    def test_toggle_vapor_movement(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        settings.set_simulate_vapor_movement(False)
        assert settings.get_simulate_vapor_movement() is False

        settings.set_simulate_vapor_movement(True)
        assert settings.get_simulate_vapor_movement() is True

    def test_toggle_water_movement(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        settings.set_simulate_water_movement(False)
        assert settings.get_simulate_water_movement() is False

        settings.set_simulate_water_movement(True)
        assert settings.get_simulate_water_movement() is True

    def test_toggle_water_evaporation(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        settings.set_simulate_water_evaporation(False)
        assert settings.get_simulate_water_evaporation() is False

        settings.set_simulate_water_evaporation(True)
        assert settings.get_simulate_water_evaporation() is True

    def test_toggles_are_independent(self):
        settings = PhysicsSettings()
        self._reset_toggles(settings)

        settings.set_simulate_vapor_condensation(False)
        settings.set_simulate_water_movement(False)

        assert settings.get_simulate_vapor_condensation() is False
        assert settings.get_simulate_vapor_movement() is True
        assert settings.get_simulate_water_movement() is False
        assert settings.get_simulate_water_evaporation() is True

        self._reset_toggles(settings)
