from enum import IntEnum
from typing import Literal

import aetherion
from aetherion import DirectionEnum, EntityEnum, ItemEnum
from aetherion._aetherion import (EntityTypeComponent, Inventory, Position,
                                  PyRegistry, VoxelGrid)
from aetherion.entities import BaseEntity


class BeastEnum(IntEnum):
    PLAYER = 1
    SQUIRREL = 2
    AEOLUS = 3  # God of wind - LLM-powered agent


def _offset_from_direction(direction: DirectionEnum) -> tuple[int, int, int]:
    dx = dy = dz = 0
    if direction == DirectionEnum.RIGHT:
        dx = 1
    elif direction == DirectionEnum.LEFT:
        dx = -1
    elif direction == DirectionEnum.UP:
        dy = -1
    elif direction == DirectionEnum.DOWN:
        dy = 1
    elif direction == DirectionEnum.UPWARD:
        dz = 1
    elif direction == DirectionEnum.DOWNWARD:
        dz = -1
    return dx, dy, dz


def _horizontal_front_xy(pos: Position) -> tuple[int, int]:
    fx, fy = pos.x, pos.y
    if pos.direction == DirectionEnum.RIGHT:
        fx += 1
    elif pos.direction == DirectionEnum.LEFT:
        fx -= 1
    elif pos.direction == DirectionEnum.UP:
        fy -= 1
    elif pos.direction == DirectionEnum.DOWN:
        fy += 1
    return fx, fy


def _max_terrain_z_in_column(voxel_grid: VoxelGrid, x: int, y: int, z_max_inclusive: int) -> int | None:
    if z_max_inclusive < 0:
        return None
    coords = voxel_grid.get_all_terrain_in_region(x, y, 0, x, y, z_max_inclusive)
    if not coords:
        return None
    return max(c.z for c in coords)



def _try_take_item_from_terrain(
    entity_id: int,
    registry: PyRegistry,
    voxel_grid: VoxelGrid,
    x: int,
    y: int,
    z: int,
    inventory: Inventory,
) -> Literal["taken", "full", "none"]:
    terrain_id = voxel_grid.get_terrain(x, y, z)
    if terrain_id == -2 or terrain_id == -1:
        # Here for our purposes -1 is also none because it means it will not have Inventory component.
        # Trying to get the Inventory component will raise an exception or segfault.
        return "none"

    entity_type: EntityTypeComponent = voxel_grid.get_terrain_entity_type_component(x, y, z)
    target_inventory: Inventory = registry.get_component(terrain_id, "Inventory")

    if entity_type.main_type != EntityEnum.TERRAIN.value or not target_inventory.item_ids:
        return "none"

    if inventory.is_full():
        if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
            console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
            console_logs.add_log("Your inventory is full!")
            registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)
        return "full"

    item_id = target_inventory.pop_item()
    inventory.add_item(item_id)

    if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
        console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
        console_logs.add_log("Item collected.")
        registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)

    registry.set_component(terrain_id, "Inventory", target_inventory)
    registry.set_component(entity_id, "Inventory", inventory)
    return "taken"

def _try_take_item_from_entity(
    entity_id: int,
    registry: PyRegistry,
    voxel_grid: VoxelGrid,
    x: int,
    y: int,
    z: int,
    inventory: Inventory,
) -> Literal["taken", "full", "none"]:
    entity_at_target = voxel_grid.get_entity(x, y, z)
    if (
        entity_at_target == -1
        or entity_at_target == -2
        or not registry.has_all_components(entity_at_target, ["EntityTypeComponent", "Inventory"])
    ):
        return "none"

    entity_type: EntityTypeComponent = registry.get_component(entity_at_target, "EntityTypeComponent")
    target_inventory: Inventory = registry.get_component(entity_at_target, "Inventory")

    if entity_type.main_type != 1 or not target_inventory.item_ids:
        return "none"

    if inventory.is_full():
        if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
            console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
            console_logs.add_log("Your inventory is full!")
            registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)
        return "full"

    item_id = target_inventory.pop_item()
    inventory.add_item(item_id)

    if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
        console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
        console_logs.add_log("Raspberry collected.")
        registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)

    registry.set_component(entity_at_target, "Inventory", target_inventory)
    registry.set_component(entity_id, "Inventory", inventory)
    return "taken"


def _try_take_item_from_cell(
    entity_id: int,
    registry: PyRegistry,
    voxel_grid: VoxelGrid,
    x: int,
    y: int,
    z: int,
    inventory: Inventory,
) -> Literal["taken", "full", "none"]:
    result = _try_take_item_from_entity(entity_id, registry, voxel_grid, x, y, z, inventory)
    if result != "none":
        return result
    return _try_take_item_from_terrain(entity_id, registry, voxel_grid, x, y, z, inventory)


def _take_item_probe_cells(pos: Position, voxel_grid: VoxelGrid) -> list[tuple[int, int, int]]:
    dx, dy, dz = _offset_from_direction(pos.direction)
    seen: set[tuple[int, int, int]] = set()
    out: list[tuple[int, int, int]] = []

    def add_cell(cell: tuple[int, int, int]) -> None:
        if cell not in seen:
            seen.add(cell)
            out.append(cell)

    add_cell((pos.x + dx, pos.y + dy, pos.z + dz))

    if pos.z >= 0:
        fx, fy = _horizontal_front_xy(pos)
        z_top = _max_terrain_z_in_column(voxel_grid, fx, fy, pos.z)
        if z_top is not None:
            add_cell((fx, fy, z_top))

    if pos.z > 0:
        z_support = _max_terrain_z_in_column(voxel_grid, pos.x, pos.y, pos.z - 1)
        if z_support is not None:
            add_cell((pos.x, pos.y, z_support))

    return out


class BeastPhysicalSys:
    def set_direction(self, direction: DirectionEnum):
        self.direction = direction

    def on_take_item_behavior(
        self,
        entity_id: int,
        registry: PyRegistry,
        voxel_grid: VoxelGrid,
        hovered_entity_id: int,
        selected_entity_id: int,
    ):
        if not registry.is_valid(entity_id) or not registry.has_all_components(
            entity_id, ["Position", "EntityTypeComponent", "Inventory"]
        ):
            return

        pos: Position = registry.get_component(entity_id, "Position")
        inventory: Inventory = registry.get_component(entity_id, "Inventory")

        for x, y, z in _take_item_probe_cells(pos, voxel_grid):
            result = _try_take_item_from_cell(entity_id, registry, voxel_grid, x, y, z, inventory)
            if result != "none":
                return

    def on_use_food(self, entity_id, registry, voxel_grid, item_id, inventory):
        food_item = registry.get_component(item_id, "FoodItem")

        digestion_comp = registry.get_component(entity_id, "DigestionComponent")

        digestion_comp.add_item(
            item_id,
            0,
            0,
            food_item.energy_density,
            food_item.mass,
            food_item.volume,
            food_item.convertion_efficiency,
            food_item.energy_health_ratio,
        )

        registry.set_component(entity_id, "Inventory", inventory)
        registry.set_component(entity_id, "DigestionComponent", digestion_comp)

        del food_item
        del digestion_comp

    def on_use_tool_or_weapon(self, entity_id, registry, voxel_grid, item_id, hovered_entity_id, selected_entity_id):
        if hovered_entity_id != -1 or selected_entity_id != -1:
            weapon_attrs = registry.get_component(item_id, "WeaponAttributes")
            weapon_attrs.damage

            meele_attack_comp = aetherion.MeeleAttackComponent()
            meele_attack_comp.weapon = item_id
            meele_attack_comp.hovered_entity = hovered_entity_id
            meele_attack_comp.selected_entity = selected_entity_id
            registry.set_component(entity_id, "MeeleAttackComponent", meele_attack_comp)

            del weapon_attrs
            del meele_attack_comp

    def on_use_item_behavior(self, entity_id, registry, voxel_grid, item_slot, hovered_entity_id, selected_entity_id):
        inventory = registry.get_component(entity_id, "Inventory")
        item_id = inventory.remove_item_by_slot(item_slot)

        if item_id == -1:
            return

        item_type = registry.get_component(item_id, "ItemTypeComponent")

        if item_type.main_type == ItemEnum.TOOL.value:
            self.on_use_tool_or_weapon(entity_id, registry, voxel_grid, item_id, hovered_entity_id, selected_entity_id)
        elif item_type.main_type == ItemEnum.FOOD.value:
            self.on_use_food(entity_id, registry, voxel_grid, item_id, inventory)
        else:
            print("This item is not 'usable'")

        del inventory


class BeastEntity(BaseEntity, BeastPhysicalSys):
    pass
