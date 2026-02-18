from __future__ import annotations

from typing import Literal

import aetherion
from aetherion import EntityEnum, EntityInterface, RenderQueue, WorldView
from aetherion.camera.models import CameraSettings

TYPE_OF_MOUSE_PROCESSOR: Literal["python", "cpp"] = "python"

type MouseState = dict[str, int | None]
type SDLColor = tuple[int, int, int]


def is_mouse_within(mx: int, my: int, x: int, y: int, width: int, height: int) -> bool:
    return x <= mx <= x + width and y <= my <= y + height


def set_to_draw_select_entity_square(
    x: int | float,
    y: int | float,
    render_queue: RenderQueue,
    layer_index: int,
    terrain_group_0: str,
    camera_settings: CameraSettings,
    selected: bool = False,
) -> None:
    sdl_color: SDLColor
    if selected:
        sdl_color = (255, 255, 0)
    else:
        sdl_color = (255, 255, 255)
    selected_area_width: int = camera_settings.tile_size_on_screen
    selected_area_height: int = camera_settings.tile_size_on_screen
    render_queue.add_task_draw_rect(
        layer_index,
        terrain_group_0,
        int(x),
        int(y),
        int(selected_area_width),
        int(selected_area_height),
        3,
        sdl_color,
    )


class EntityMouseSelection:
    def __init__(
        self,
        screen_x: int,
        screen_y: int,
        entity: EntityInterface,
        lock_on_target: int | None,
        layer_index: int,
        mouse_state: MouseState,
        camera_settings: CameraSettings,
    ) -> None:
        self.current_entity_selected: bool = False
        self.selected_square_drawn: bool = False

        # First selection area (offset by tile size from camera settings)
        self.screen_x: int = screen_x
        self.screen_y: int = screen_y
        self.camera_settings: CameraSettings = camera_settings
        self.offset_x: int = screen_x + self.camera_settings.tile_size_on_screen
        self.offset_y: int = screen_y + self.camera_settings.tile_size_on_screen
        self.layer_index: int = layer_index

        self.lock_on_target: int | None = lock_on_target
        self.entity: EntityInterface = entity

        self.mouse_x: int | None = mouse_state.get("x")
        self.mouse_y: int | None = mouse_state.get("y")

        self.entity_x: int = 0
        self.entity_y: int = 0
        self.entity_z: int = 0

    def draw_lock_on_target(self, render_queue: RenderQueue, terrain_group_0: str) -> None:
        if self.lock_on_target is not None and self.entity.get_entity_id() == self.lock_on_target:
            set_to_draw_select_entity_square(
                self.offset_x,
                self.offset_y,
                render_queue,
                self.layer_index,
                terrain_group_0,
                self.camera_settings,
                selected=True,
            )
            self.selected_square_drawn = True

    def is_mouse_coordinates_invalid(self) -> bool:
        return self.mouse_x is None or self.mouse_y is None

    def set_entity_coordinates(self) -> None:
        entity_pos = self.entity.get_position()
        self.entity_x, self.entity_y, self.entity_z = entity_pos.x, entity_pos.y, entity_pos.z

    def set_selection_and_draw_hovered(self, render_queue: RenderQueue, layer_index: int, terrain_group_0: str) -> None:
        self.current_entity_selected = True
        if not self.selected_square_drawn:
            set_to_draw_select_entity_square(
                self.offset_x, self.offset_y, render_queue, layer_index, terrain_group_0, self.camera_settings
            )

    def check_voxel_botton_selection(
        self, world_view: WorldView, render_queue: RenderQueue, layer_index: int, terrain_group_0: str
    ) -> bool:
        # This part checks if the mouse is selecting the botton of the voxel (the base of the voxel)
        if is_mouse_within(
            self.mouse_x,
            self.mouse_y,
            self.offset_x,
            self.offset_y,
            self.camera_settings.tile_size_on_screen,
            self.camera_settings.tile_size_on_screen,
        ):
            corner_se_terrain = world_view.get_terrain(self.entity_x + 1, self.entity_y + 1, self.entity_z)
            above_se_terrain = world_view.get_terrain(self.entity_x + 2, self.entity_y + 2, self.entity_z + 1)

            if corner_se_terrain is None and above_se_terrain is None:
                self.set_selection_and_draw_hovered(render_queue, layer_index, terrain_group_0)
                return True
            else:
                area_ray_collision = True  # Variable is set but not used further in this block

            above_se_terrain = world_view.get_terrain(self.entity_x + 1, self.entity_y + 1, self.entity_z + 1)
            if not area_ray_collision and above_se_terrain is None:
                self.set_selection_and_draw_hovered(render_queue, layer_index, terrain_group_0)
                return True

        return False

    def check_voxel_top_selection(
        self, world_view: WorldView, render_queue: RenderQueue, layer_index: int, terrain_group_0: str
    ) -> bool:
        # Second selection area (exact screen position)
        # This part checks if the mouse is selecting the top
        # of the voxel (For terrains it is the upper part of the terrain)
        if self.entity.get_entity_type().main_type == EntityEnum.TERRAIN.value:
            if is_mouse_within(
                self.mouse_x,
                self.mouse_y,
                self.screen_x,
                self.screen_y,
                self.camera_settings.tile_size_on_screen,
                self.camera_settings.tile_size_on_screen,
            ):
                above_se_terrain = world_view.get_terrain(self.entity_x + 1, self.entity_y + 1, self.entity_z + 1)
                above_entity = world_view.get_entity(self.entity_x, self.entity_y, self.entity_z + 1)

                if above_se_terrain is None and above_entity is None:
                    self.set_selection_and_draw_hovered(render_queue, layer_index, terrain_group_0)
                    return True
        return False


def _get_and_draw_selected_entity(
    world_view: WorldView,
    entity: EntityInterface,
    mouse_state: MouseState,
    screen_x: int,
    screen_y: int,
    render_queue: RenderQueue,
    layer_index: int,
    terrain_group_0: str,
    selected_entity: int | None,
    camera_settings: CameraSettings,
) -> bool:
    entity_mouse_selection = EntityMouseSelection(
        screen_x, screen_y, entity, selected_entity, layer_index, mouse_state, camera_settings
    )
    entity_mouse_selection.draw_lock_on_target(render_queue, terrain_group_0)

    if entity_mouse_selection.is_mouse_coordinates_invalid():
        return entity_mouse_selection.current_entity_selected
    entity_mouse_selection.set_entity_coordinates()

    if entity_mouse_selection.check_voxel_botton_selection(world_view, render_queue, layer_index, terrain_group_0):
        return entity_mouse_selection.current_entity_selected

    if entity_mouse_selection.check_voxel_top_selection(world_view, render_queue, layer_index, terrain_group_0):
        return entity_mouse_selection.current_entity_selected

    return entity_mouse_selection.current_entity_selected


def get_and_draw_selected_entity(
    world_view: WorldView,
    entity: EntityInterface,
    mouse_state: MouseState,
    screen_x: int,
    screen_y: int,
    render_queue: RenderQueue,
    layer_index: int,
    terrain_group_0: str,
    selected_entity: int | None,
    camera_settings: CameraSettings,
) -> bool:
    if TYPE_OF_MOUSE_PROCESSOR == "python":
        return _get_and_draw_selected_entity(
            world_view,
            entity,
            mouse_state,
            screen_x,
            screen_y,
            render_queue,
            layer_index,
            terrain_group_0,
            selected_entity,
            camera_settings,
        )
    else:
        selected_entity_id: int = -1
        if selected_entity:
            breakpoint()
            selected_entity_id = selected_entity
        return aetherion.get_and_draw_selected_entity(
            world_view,
            entity,
            mouse_state,
            screen_x,
            screen_y,
            render_queue,
            layer_index,
            selected_entity_id,
            terrain_group_0,
            camera_settings.tile_size_on_screen,
        )
