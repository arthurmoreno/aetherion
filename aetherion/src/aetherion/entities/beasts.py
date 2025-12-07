from enum import IntEnum

import aetherion
from aetherion import DirectionEnum, ItemEnum
from aetherion.entities import BaseEntity


class BeastEnum(IntEnum):
    PLAYER = 1
    SQUIRREL = 2
    AEOLUS = 3  # God of wind - LLM-powered agent


class BeastPhysicalSys:
    def set_direction(self, direction):
        self.direction = direction

    def on_take_item_behavior(self, entity_id, registry, voxel_grid, hovered_entity_id, selected_entity_id):
        if not registry.is_valid(entity_id) or not registry.has_all_components(
            entity_id, ["Position", "EntityTypeComponent", "Inventory"]
        ):
            return

        pos = registry.get_component(entity_id, "Position")
        inventory = registry.get_component(entity_id, "Inventory")

        # Determine the target position based on the direction
        taking_from_x, taking_from_y, taking_from_z = pos.x, pos.y, pos.z
        if pos.direction == DirectionEnum.RIGHT:
            taking_from_x += 1
        elif pos.direction == DirectionEnum.LEFT:
            taking_from_x -= 1
        elif pos.direction == DirectionEnum.UP:
            taking_from_y -= 1
        elif pos.direction == DirectionEnum.DOWN:
            taking_from_y += 1
        elif pos.direction == DirectionEnum.UPWARD:
            taking_from_z += 1
        elif pos.direction == DirectionEnum.DOWNWARD:
            taking_from_z -= 1

        entity_at_target = voxel_grid.get_entity(taking_from_x, taking_from_y, taking_from_z)

        # if entity_at_target != -1:
        #     entity_type = registry.get_component(entity_at_target, "EntityTypeComponent")

        #     if entity_type.main_type == 1:
        #         import ipdb; ipdb.set_trace()
        #         print("stop")

        # Check if there is a valid entity at the target position
        if (
            entity_at_target != -1
            and entity_at_target != -2
            and registry.has_all_components(entity_at_target, ["EntityTypeComponent", "Inventory"])
        ):
            entity_type = registry.get_component(entity_at_target, "EntityTypeComponent")
            target_inventory = registry.get_component(entity_at_target, "Inventory")

            # if entity_type.main_type == 1:
            #     import ipdb; ipdb.set_trace()
            #     print("stop")

            # Check if the entity is a valid item source and contains items
            if entity_type.main_type == 1 and target_inventory.item_ids:
                if not inventory.is_full():
                    # Take the last item from the target inventory
                    item_id = target_inventory.pop_item()
                    inventory.add_item(item_id)

                    # Log success message
                    if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
                        console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
                        console_logs.add_log("Raspberry collected.")
                        registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)

                    registry.set_component(entity_at_target, "Inventory", target_inventory)
                    registry.set_component(entity_id, "Inventory", inventory)
                else:
                    # Log inventory full message
                    if registry.has_all_components(entity_id, ["ConsoleLogsComponent"]):
                        console_logs = registry.get_component(entity_id, "ConsoleLogsComponent")
                        console_logs.add_log("Your inventory is full!")
                        registry.set_component(entity_id, "ConsoleLogsComponent", console_logs)

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
