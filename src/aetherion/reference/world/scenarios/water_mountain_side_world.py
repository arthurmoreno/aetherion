# ruff: noqa: E501 — dungeon mask rows are copied verbatim from the minimal dungeon demo.

from __future__ import annotations

from aetherion import World
from aetherion.reference.systems.spring_water import SpringWaterSystem
from aetherion.reference.world.utils import mountain_ridge_source_xyz
from aetherion.reference.world.world_factories import MountainSideWorldFactory


def mountain_side_stream_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    world_factory = MountainSideWorldFactory(width=world_width, height=world_height, depth=world_depth)
    world = world_factory.generate_world()
    world.process_ecosystem_async = True
    world.water_auto_balancing = False
    world.simulate_water_movement = True
    world.simulate_water_evaporation = False
    world.simulate_vapor_movement = False
    world.simulate_vapor_condensation = False

    rx, ry, rz = mountain_ridge_source_xyz(world_width, world_height, world_depth)
    spring_pace: int = world_config.get("spring_pace", 5)
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry, source_z=rz))

    return world


def mountain_side_spring_world_factory(world_config: dict[str, int]) -> World:
    world_width: int = world_config.get("world_width", 3)
    world_height: int = world_config.get("world_height", 3)
    world_depth: int = world_config.get("world_depth", 3)

    world_factory = MountainSideWorldFactory(width=world_width, height=world_height, depth=world_depth)
    world = world_factory.generate_world()
    world.process_ecosystem_async = True
    world.water_auto_balancing = False
    world.simulate_water_movement = True
    world.simulate_water_evaporation = False
    world.simulate_vapor_movement = False
    world.simulate_vapor_condensation = False

    rx, ry, rz = mountain_ridge_source_xyz(world_width, world_height, world_depth)
    spring_pace: int = world_config.get("spring_pace", 1)
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 6, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 5, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 4, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 3, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 2, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry - 1, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 1, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 2, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 3, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 4, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 5, source_z=rz))
    world.add_python_system(SpringWaterSystem(pace=spring_pace, source_x=rx, source_y=ry + 6, source_z=rz))

    return world
