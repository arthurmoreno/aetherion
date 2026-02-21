from __future__ import annotations

from typing import Any, Optional

from aetherion import EntityInterface, WorldView, get_terrain_camera_stats, get_water_camera_stats
from aetherion.animation.dimetric import entity_animation_handler
from aetherion.camera.models import CameraModel, CameraSettings
from aetherion.camera.mouse import get_and_draw_selected_entity
from aetherion.camera.terrain import draw_terrain
from aetherion.renderer.views import BaseView


def default_terrain_handler(
    camera_model: CameraModel,
    camera_settings: CameraSettings,
    terrain: EntityInterface,
    view_object: BaseView,
    selected_entity: Optional[int],
    world_view: WorldView,
    mouse_state: Any,
    screen_x: int,
    screen_y: int,
    layer_index: int,
    entity_hovered: EntityInterface | None,
    sun_light: float,
    player: EntityInterface,
    gradient_view_object: Any,
    water_view: Any,
) -> EntityInterface | None:
    return draw_terrain(
        terrain,
        view_object,
        camera_settings,
        selected_entity,
        world_view,
        mouse_state,
        screen_x,
        screen_y,
        camera_model.render_queue,
        layer_index,
        camera_model.terrain_group_0,
        camera_model.light_intensities[layer_index],
        camera_model.gui_group,
        camera_model.terrain_group_1,
        camera_model.effect_group,
        sun_light,
        player,
        gradient_view_object,
        get_terrain_camera_stats(),
        water_view,
        camera_model.views,
        get_water_camera_stats(),
        entity_hovered,
    )


def entity_beast_handler(
    camera_model: CameraModel,
    camera_settings: CameraSettings,
    world_view: WorldView,
    entity: EntityInterface,
    view_object: BaseView,
    screen_x: int,
    screen_y: int,
    layer_index: int,
    mouse_state: Any,
    selected_entity: Optional[int],
    entity_hovered: EntityInterface | None,
    sun_light: float,
) -> tuple[int, int]:
    _screen_x, _screen_y = entity_animation_handler(
        None, entity, view_object, screen_x, screen_y, camera_settings.tile_size_on_screen
    )

    moving_component = entity.get_moving_component()
    if moving_component.vz > 0 and layer_index + 1 < camera_model.LAYERS_TO_DRAW:
        layer_to_draw = layer_index
        entity_group = camera_model.effect_group
    elif moving_component.vz < 0 and layer_index + 1 < camera_model.LAYERS_TO_DRAW:
        layer_to_draw = layer_index + 1
        entity_group = camera_model.effect_group
    else:
        layer_to_draw = layer_index
        entity_group = camera_model.entities_group_0

    current_entity_hovered = False
    if entity_hovered is None or (entity is not None and entity.get_entity_id() == selected_entity):
        current_entity_hovered = get_and_draw_selected_entity(
            world_view,
            entity,
            mouse_state,
            _screen_x,
            _screen_y,
            camera_model.render_queue,
            layer_index,
            camera_model.terrain_group_0,
            selected_entity,
            camera_settings,
        )

    light_intensity = camera_model.light_intensities[layer_index]
    if current_entity_hovered:
        entity_hovered = entity
        light_intensity = max(light_intensity - 0.2, 0)

    light_intensity += sun_light
    camera_model.render_queue.add_task_by_id(
        layer_to_draw,
        entity_group,
        view_object.sprite.sprite_id,
        _screen_x,
        _screen_y,
        float(light_intensity),
        float(1.0),
    )

    return _screen_x, _screen_y


def entity_plant_handler(
    camera_model: CameraModel,
    camera_settings: CameraSettings,
    world_view: WorldView,
    entity: EntityInterface,
    view_object: BaseView,
    screen_x: int,
    screen_y: int,
    layer_index: int,
    mouse_state: Any,
    selected_entity: Optional[int],
    entity_hovered: EntityInterface | None,
    sun_light: float,
) -> tuple[int, int]:
    _screen_x, _screen_y = screen_x, screen_y
    entity_direction = entity.get_position().get_direction_as_int()
    try:
        view_object.set_direction(entity_direction)
    except Exception:
        pass

    current_entity_hovered = False
    if entity_hovered is None or (entity is not None and entity.get_entity_id() == selected_entity):
        current_entity_hovered = get_and_draw_selected_entity(
            world_view,
            entity,
            mouse_state,
            _screen_x,
            _screen_y,
            camera_model.render_queue,
            layer_index,
            camera_model.terrain_group_0,
            selected_entity,
            camera_settings,
        )

    light_intensity = camera_model.light_intensities[layer_index]
    if current_entity_hovered:
        entity_hovered = entity
        light_intensity = max(light_intensity - 0.2, 0)

    light_intensity += sun_light
    camera_model.render_queue.add_task_by_id(
        layer_index,
        camera_model.entities_group_0,
        view_object.sprite.sprite_id,
        screen_x,
        screen_y,
        float(light_intensity),
        float(1.0),
    )

    return _screen_x, _screen_y
