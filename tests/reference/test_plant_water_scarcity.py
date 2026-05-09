"""Water-scarcity → plant death tests for the bisection plan
`.claude/docs/epics-plans/2026-05-06-water-scarcity-plant-death.md`.

Recipe: place a single GRASS tile, place a plant entity directly above it,
tick the world. The async ecosystem worker visits the grass tile every
tick and enqueues `PlantWaterUptakeEvent` for the plant above it; the
main-thread mutator (`makePlantSuckWater`) reads `matter.water_matter`
and either drinks (refills `PlantResources.water`) or counts the tick
as stress. The new `processPlants` drought-stress loop (in
`EcosystemEngine.cpp`) decrements the counter each tick and damages
`HealthComponent.healthLevel` once over threshold; the existing
`HealthSystem::processHealth` enqueues `KillEntityEvent` on
`healthLevel <= 0`.
"""

from __future__ import annotations

from time import sleep

from helpers import build_minimal_test_manager

from aetherion import (
    DirectionEnum,
    EntityEnum,
    EntityTypeComponent,
    GridType,
    HealthComponent,
    PhysicsSettings,
    Position,
)
from aetherion._aetherion import WaterStressComponent
from aetherion.entities import BaseEntity
from aetherion.reference.world.scenarios.primitives import (
    place_stone,
)

# ─────────────────────────────────────────────────────────────────────────────
# Fixtures — minimal plant entity
# ─────────────────────────────────────────────────────────────────────────────


class _Plant(BaseEntity):
    """ENTITY-typed plant with HealthComponent. Sits above a grass tile."""

    grid_type = GridType.ENTITY

    def __init__(self, x: int, y: int, z: int, *, max_health: int = 100) -> None:
        BaseEntity.__init__(self, x, y, z)

        position = Position()
        position.x = x
        position.y = y
        position.z = z
        position.direction = DirectionEnum.DOWN
        self.position = position

        entity_type = EntityTypeComponent()
        entity_type.main_type = EntityEnum.PLANT.value
        entity_type.sub_type0 = 0
        entity_type.sub_type1 = 0
        self.entity_type = entity_type

        health = HealthComponent()
        health.health_level = max_health
        health.max_health = max_health
        self.health = health


def _tick(manager, steps: int, *, sleep_s: float = 0.005) -> None:
    """Run the world manager `steps` times with a small sleep so the async
    ecosystem worker has time to enqueue events between updates."""
    for _ in range(steps):
        manager.update()
        sleep(sleep_s)


def _set_drought_knobs(
    *,
    stress_per_dry_tick: int = 2,
    max_water_stress_ticks: int = 100,
    drought_damage_per_cycle: int = 5,
) -> None:
    """Configure the global drought tunables for a test. The C++ singleton
    reads these on every `processPlants` tick — set before any `manager.update()`
    that should observe the new values."""
    settings = PhysicsSettings()
    settings.set_stress_per_dry_tick(stress_per_dry_tick)
    settings.set_max_water_stress_ticks(max_water_stress_ticks)
    settings.set_drought_damage_per_cycle(drought_damage_per_cycle)


def _spawn_plant_on_grass(manager, x: int, y: int) -> int:
    """Place GRASS at (x, y, 0), spawn a plant entity at (x, y, 1), return
    the plant's entity id."""
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()
    place_stone(voxel_grid, x, y, 0)  # GRASS tile (terrain_matter=7, water_matter=0)
    plant = world.create_entity(_Plant(x, y, 1))
    return plant.get_id()


def _read_stress(world, entity_id: int) -> WaterStressComponent | None:
    return world.get_py_registry().get_component(entity_id, "WaterStressComponent")


def _read_health(world, entity_id: int) -> HealthComponent | None:
    return world.get_py_registry().get_component(entity_id, "HealthComponent")


# ─────────────────────────────────────────────────────────────────────────────
# Smoke — confirms the wiring works at all
# ─────────────────────────────────────────────────────────────────────────────


def test_plant_on_dry_grass_accumulates_stress():
    """After a handful of ticks on a fully-dry grass tile, the plant's
    water_stress_ticks should be strictly positive — proves the
    drought-stress loop in `processPlants` is firing.
    """
    from aetherion import EntityEnum, TerrainEnum, TerrainIdTypeEnum

    # Aggressive increment so 5 ticks unambiguously produce non-zero stress
    # without crossing the threshold (stays a pure increment-only smoke).
    _set_drought_knobs(
        stress_per_dry_tick=10,
        max_water_stress_ticks=10_000,
        drought_damage_per_cycle=1,
    )

    manager = build_minimal_test_manager(8, 8, 4)
    plant_id = _spawn_plant_on_grass(manager, 4, 4)
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()

    # Diagnostic: confirm the world state is what processPlants observes.
    assert voxel_grid.get_terrain(4, 4, 0) == TerrainIdTypeEnum.ON_GRID_STORAGE.value, (
        "grass terrain should be ON_GRID_STORAGE"
    )
    grass_type = voxel_grid.get_terrain_entity_type_component(4, 4, 0)
    assert grass_type.main_type == EntityEnum.TERRAIN.value
    assert grass_type.sub_type0 == TerrainEnum.GRASS.value
    grass_matter = voxel_grid.get_terrain_matter_container_component(4, 4, 0)
    assert grass_matter.water_matter == 0, "test scenario expects DRY grass"
    entity_above = world.get_entity(4, 4, 1)
    assert entity_above == plant_id, f"expected plant_id={plant_id} at entity-grid (4,4,1); got {entity_above}"

    _tick(manager, steps=5)

    stress = _read_stress(world, plant_id)
    assert stress is not None, "plant should have WaterStressComponent emplaced"
    assert stress.water_stress_ticks > 0, (
        f"expected stress to accumulate on a dry tile after 5 ticks; got water_stress_ticks={stress.water_stress_ticks}"
    )


# ─────────────────────────────────────────────────────────────────────────────
# Three plan-mandated tests
# ─────────────────────────────────────────────────────────────────────────────


def test_plant_dies_in_drought():
    """A plant on a perpetually-dry grass tile crosses the stress
    threshold, takes damage every tick thereafter, and the existing
    health system kills it when healthLevel reaches zero."""
    # Aggressive: 1 dry tick crosses threshold; damage drops 100 HP to
    # negative on the same tick; HealthSystem kills the next tick.
    _set_drought_knobs(
        stress_per_dry_tick=10,
        max_water_stress_ticks=5,
        drought_damage_per_cycle=200,
    )

    manager = build_minimal_test_manager(8, 8, 4)
    plant_id = _spawn_plant_on_grass(manager, 4, 4)

    # Budget: 1 tick to cross threshold, 1 tick of damage, 1 tick for
    # HealthSystem to enqueue KillEntityEvent, plus slack.
    _tick(manager, steps=10)

    # After death, the entity is gone — the registry returns None for any
    # component lookup on a killed entity.
    health = _read_health(manager.current.world, plant_id)
    assert health is None, (
        f"plant should be dead after 10 ticks of drought; got health={health.health_level if health else None}"
    )


def test_plant_survives_in_oasis():
    """A plant sitting on a grass tile that always has water_matter > 0
    is detected as supplied every tick, stress stays at 0, no damage is
    applied — the same aggressive config that kills a desert plant in
    the drought test must NOT kill an oasis plant."""
    _set_drought_knobs(
        stress_per_dry_tick=10,
        max_water_stress_ticks=5,
        drought_damage_per_cycle=200,
    )

    manager = build_minimal_test_manager(8, 8, 4)
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()

    plant_id = _spawn_plant_on_grass(manager, 4, 4)

    repo = voxel_grid.terrain_grid_repository

    def _refill_grass_water(level: int = 100) -> None:
        matter = voxel_grid.get_terrain_matter_container_component(4, 4, 0)
        matter.water_matter = level
        repo.set_terrain_matter_container(4, 4, 0, matter)

    # Same tick budget as the drought test would have killed in 10 — must
    # not kill here. Run longer to be safe.
    steps = 50
    for _ in range(steps):
        _refill_grass_water(level=100)
        manager.update()
        sleep(0.005)

    stress = _read_stress(world, plant_id)
    health = _read_health(world, plant_id)

    assert stress is not None and health is not None, f"plant should still exist in an oasis after {steps} ticks"
    assert stress.water_stress_ticks == 0, (
        f"plant in oasis should never accumulate stress; got water_stress_ticks={stress.water_stress_ticks}"
    )
    assert health.health_level == health.max_health, (
        f"plant in oasis should be at full health; got health_level={health.health_level}/{health.max_health}"
    )


def test_plant_survives_marginal_water_with_recovery():
    """Per-tick decrement is load-bearing: a plant that gets water more
    often than it doesn't should survive indefinitely. With increment=1
    on a dry tick and decrement=1 on a wet tick, a 2-of-3 wet pattern
    nets -1 every 3 ticks (or stays at 0). Without the decrement, stress
    would climb monotonically and the plant would eventually die."""
    _set_drought_knobs(
        stress_per_dry_tick=1,  # symmetric with the implicit -1 decrement
        max_water_stress_ticks=20,
        drought_damage_per_cycle=200,
    )

    manager = build_minimal_test_manager(8, 8, 4)
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()
    plant_id = _spawn_plant_on_grass(manager, 4, 4)

    repo = voxel_grid.terrain_grid_repository

    # Pattern: water on for 2 ticks, off for 1.
    steps = 60
    for tick in range(steps):
        matter = voxel_grid.get_terrain_matter_container_component(4, 4, 0)
        matter.water_matter = 100 if (tick % 3) != 2 else 0
        repo.set_terrain_matter_container(4, 4, 0, matter)
        manager.update()
        sleep(0.005)

    stress = _read_stress(world, plant_id)
    health = _read_health(world, plant_id)

    assert stress is not None and health is not None, (
        f"plant should still exist with marginal water after {steps} ticks "
        f"— if absent, the decrement branch in processPlants is broken"
    )
    # If the decrement were missing, stress would have climbed by ~20 by now.
    assert stress.water_stress_ticks < 20, (
        f"marginal plant should not approach threshold; got water_stress_ticks={stress.water_stress_ticks}"
    )
