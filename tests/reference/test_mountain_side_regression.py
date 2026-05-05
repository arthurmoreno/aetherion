"""Regression coverage for the mountain-side water reproduction world."""

from __future__ import annotations

from time import sleep

from helpers import build_mountain_side_manager

from aetherion import TerrainEnum
from aetherion.world.manager import WorldManager


def _collect_water_cells_in_band(
    manager: WorldManager, *, y_min: int = 49, y_max: int = 51
) -> list[tuple[int, int, int, int]]:
    world = manager.current.world
    voxel_grid = world.get_voxel_grid()
    cells: list[tuple[int, int, int, int]] = []

    for x in range(world.width):
        for y in range(y_min, y_max + 1):
            for z in range(world.depth):
                terrain_id = voxel_grid.get_terrain(x, y, z)
                if terrain_id == -2:
                    continue
                matter = voxel_grid.get_terrain_matter_container_component(x, y, z)
                terrain_type = voxel_grid.get_terrain_entity_type_component(x, y, z)
                if matter is None or terrain_type is None:
                    continue
                if (
                    terrain_type.sub_type0 == TerrainEnum.WATER.value
                    or matter.water_matter > 0
                    or matter.water_vapor > 0
                ):
                    cells.append((x, y, z, matter.water_matter))

    return cells


def _format_water_band(cells: list[tuple[int, int, int, int]], *, limit: int = 25) -> str:
    if not cells:
        return "<no water cells>"
    preview = ", ".join(f"({x},{y},{z})={water}" for x, y, z, water in cells[:limit])
    suffix = "" if len(cells) <= limit else f", ... total={len(cells)}"
    return preview + suffix


def _assert_water_advances_through_center_corridor(*, steps: int) -> None:
    manager = build_mountain_side_manager("Mountain Side Regression")
    # `WorldManager.load_world` (called by the helper) resets status to
    # "ready", which makes `manager.update()` a no-op. The regression test
    # needs the world to actually tick, so flip it back to "running".
    manager.current_metadata.status = "running"

    for _ in range(steps):
        manager.update()
        sleep(0.005)

    world = manager.current.world
    water_cells = _collect_water_cells_in_band(manager)

    assert not world.has_water_sim_errors(), (
        f"Water simulation reported worker-thread errors after {steps} paced steps."
    )

    assert water_cells, f"Expected water to appear in the center corridor band y=49..51 after {steps} paced updates."

    furthest_x = max(x for x, _y, _z, _water in water_cells)
    total_water = sum(water for _x, _y, _z, water in water_cells)

    print(f"steps={steps} water_cells: {_format_water_band(water_cells)}")
    print(f"steps={steps} furthest_x={furthest_x} total_water={total_water}")

    assert furthest_x > 5, (
        f"Expected water to move beyond the spring source x=5 after {steps} paced updates. "
        f"furthest_x={furthest_x}; cells={_format_water_band(water_cells)}"
    )
    assert total_water > 0, (
        f"Expected positive water matter in the center corridor band after {steps} paced updates. "
        f"cells={_format_water_band(water_cells)}"
    )


def test_mountain_side_world_advances_water_through_center_corridor_100_steps():
    """Engine-managed paced repro with post-step state inspection.

    This uses the same factory registration and world-manager load path the game
    uses, instead of instantiating the raw world directly.
    """
    _assert_water_advances_through_center_corridor(steps=100)


def test_mountain_side_world_advances_water_through_center_corridor_500_steps():
    _assert_water_advances_through_center_corridor(steps=500)
