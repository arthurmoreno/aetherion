from __future__ import annotations

from typing import Any, Callable, Optional, TypedDict

import sdl2
from aetherion.logger import logger

import aetherion
from aetherion import EntityInterface, GameClock, PubSubTopicBroker, TopicReader, WorldView
from aetherion.animation.dimetric import (
    calc_pixel_offset_inverted_quadratic,
    calc_pixel_offset_linear,
    calc_pixel_offset_quadratic,
)
from aetherion.camera.models import CameraModel, CameraSettings
from aetherion.entities.base import Classification
from aetherion.entities.beasts import BeastEntity
from aetherion.events.action_event import InputEventActionType
from aetherion.game_state.state import SharedState
from aetherion.paths import resolve_path
from aetherion.renderer.views import BaseView

# ---------------------------------------------------------------------------
# Type aliases
# ---------------------------------------------------------------------------

type MouseState = dict[str, int | bool]
type LightIntensityMap = dict[int, float]


class CameraUIViews(TypedDict):
    lifebar: Any
    gradient_arrow: Any


class ViewRegistry(TypedDict):
    terrains: dict[int, Any]
    entities: dict[Classification, Any]
    items: dict[int, Any]
    camera_ui: CameraUIViews


# Adapt the legacy "camera-ui" key to the typed dict
# def _get_camera_ui(views: dict[str, Any]) -> CameraUIViews:
#     return views["camera-ui"]


# TODO: Make both configurable through settings and remove hardcoded values.
LAYERS_TO_DRAW = 5
SUN_LIGHT_SCALING_FACTOR = 0.6


def get_sun_light(shared_state: SharedState) -> float:
    game_clock: GameClock | None = shared_state.game_clock
    if game_clock:
        sun_intensity = aetherion.SunIntensity.getIntensity(game_clock)
    else:
        sun_intensity = 0.0

    scaling_factor = SUN_LIGHT_SCALING_FACTOR
    sun_light = sun_intensity * scaling_factor

    # print(f"Sun light: {sun_light}")
    return sun_light


def adjust_camera_entity_moving(
    world_view: WorldView,
    entity: Any,
    screen_x_offset: int,
    screen_y_offset: int,
    tile_size_on_screen: int,
) -> tuple[int, int, None]:
    # Check if the entity is moving (WALK or MOVING_FROM event)
    # event = world_view.get_event(entity.x, entity.y, entity.z)
    entity_interface: EntityInterface | None = world_view.get_entity(entity.x, entity.y, entity.z)
    if entity_interface is None:
        return screen_x_offset, screen_y_offset, None

    is_moving = (
        entity_interface.has_component(aetherion.ComponentFlag.MOVING_COMPONENT.value) if entity_interface else False
    )
    if is_moving:
        moving_component = entity_interface.get_moving_component()
        # Retrieve event information
        completion_time = moving_component.completion_time
        time_remaining = moving_component.time_remaining

        diff_x = entity.x - moving_component.moving_from_x
        diff_y = entity.y - moving_component.moving_from_y
        diff_z = entity.z - moving_component.moving_from_z

        screen_x_offset -= diff_x * tile_size_on_screen
        screen_y_offset += diff_y * tile_size_on_screen
        if diff_z:
            screen_x_offset += diff_z * tile_size_on_screen
            screen_y_offset -= diff_z * tile_size_on_screen

        # Adjust the camera offset based on the entity's velocity
        velocity_x = moving_component.vx
        velocity_y = moving_component.vy
        velocity_z = moving_component.vz

        # Calculate pixel offset based on completion time and remaining time
        pixel_offset = calc_pixel_offset_linear(completion_time, time_remaining, tile_size_on_screen)

        if velocity_z > 0 and moving_component.will_stop_z:
            pixel_offset_z = calc_pixel_offset_inverted_quadratic(completion_time, time_remaining, tile_size_on_screen)
        elif velocity_z < 0 and moving_component.will_stop_z:
            pixel_offset_z = calc_pixel_offset_quadratic(completion_time, time_remaining, tile_size_on_screen)
        else:
            pixel_offset_z = calc_pixel_offset_linear(completion_time, time_remaining, tile_size_on_screen)

        # Adjust screen offsets based on velocity and pixel offset
        # X-axis adjustment (right if velocity_x > 0, left if velocity_x < 0)
        screen_x_offset += pixel_offset * (abs(diff_x) if velocity_x > 0 else -abs(diff_x) if velocity_x < 0 else 0)

        # Y-axis adjustment (up if velocity_y > 0, down if velocity_y < 0)
        screen_y_offset += pixel_offset * (-abs(diff_y) if velocity_y > 0 else abs(diff_y) if velocity_y < 0 else 0)

        # Z-axis adjustment — Combined with X and Y movements:
        if velocity_z != 0:
            z_factor_x = 1 if velocity_z < 0 else -1  # Adjust left or right for z
            z_factor_y = -1 if velocity_z < 0 else 1  # Adjust up or down for z

            # Apply the Z velocity effect in combination with existing X and Y adjustments
            screen_x_offset += (pixel_offset_z) * z_factor_x  # Adjust the x-axis based on z-movement
            screen_y_offset += (pixel_offset_z) * z_factor_y  # Adjust the y-axis based on z-movement

    return screen_x_offset, screen_y_offset, None


def _noop_entity_handler(
    camera_model: CameraModel,
    camera_settings: CameraSettings,
    world_view: WorldView,
    entity: EntityInterface,
    view_object: Any,
    screen_x: int,
    screen_y: int,
    layer_index: int,
    mouse_state: MouseState,
    selected_entity: Optional[int],
    entity_hovered: EntityInterface | None,
    sun_light: float,
) -> EntityInterface | None:
    return entity_hovered


def _noop_terrain_handler(
    camera_model: CameraModel,
    camera_settings: CameraSettings,
    terrain: EntityInterface,
    view_object: Any,
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
    return None


class Camera:
    """Camera class responsible for drawing a beast perspective."""

    def __init__(
        self,
        renderer: Any,
        views: dict[str, Any],
        settings: CameraSettings,
        pubsub_broker: PubSubTopicBroker[InputEventActionType],
        beast_entity_handler: Callable[..., Any] | None = None,
        plant_entity_handler: Callable[..., Any] | None = None,
        terrain_handler: Callable[..., Any] | None = None,
    ) -> None:
        self.renderer: Any = renderer
        self.views: dict[str, Any] = views
        self.world_view: WorldView | None = None
        self.mouse_topic_name: str = "mouse_action_queue"
        self.topic_reader: TopicReader[InputEventActionType] = TopicReader(
            bus=pubsub_broker, topics=[self.mouse_topic_name]
        )

        self.entities_group_0: str = "entities_group_0"
        self.entities_group_1: str = "entities_group_1"
        self.entities_group_2: str = "entities_group_2"
        self.terrain_group_0: str = "terrain_group_0"
        self.terrain_group_1: str = "terrain_group_1"
        self.terrain_group_2: str = "terrain_group_2"
        self.gui_group: str = "gui_group"
        self.light_group: str = "light_group"
        self.effect_group: str = "effect_group"

        self.render_queue = aetherion.RenderQueue(font_path=str(resolve_path("res://assets/Toriko.ttf")))
        self.render_queue.set_priority_order(
            {
                self.terrain_group_0: 0,
                self.entities_group_0: 1,
                self.terrain_group_1: 2,
                self.entities_group_1: 3,
                self.terrain_group_2: 4,
                self.entities_group_2: 5,
                self.gui_group: 6,
                self.light_group: 7,
                self.effect_group: 8,
            }
        )

        self.light_intensities: LightIntensityMap = {
            0: 0.8 - SUN_LIGHT_SCALING_FACTOR,
            1: 0.9 - SUN_LIGHT_SCALING_FACTOR,
            2: 1.0 - SUN_LIGHT_SCALING_FACTOR,
            3: 0.9 - SUN_LIGHT_SCALING_FACTOR,
            4: 0.8 - SUN_LIGHT_SCALING_FACTOR,
        }
        # Handler registries for entity and terrain rendering

        self._beast_entity_handler: Callable[..., Any]
        if beast_entity_handler:
            self._beast_entity_handler = beast_entity_handler
        else:
            self._beast_entity_handler = _noop_entity_handler

        self._plant_entity_handler: Callable[..., Any]
        if plant_entity_handler:
            self._plant_entity_handler = plant_entity_handler
        else:
            self._plant_entity_handler = _noop_entity_handler

        self._terrain_handler: Callable[..., Any]
        if terrain_handler:
            self._terrain_handler = terrain_handler
        else:
            self._terrain_handler = _noop_terrain_handler

        self.settings: CameraSettings = settings

        # Build a camera model for handlers that expect CameraModel instead of full Camera
        self._camera_model = self._build_camera_model()

    def _build_camera_model(self) -> CameraModel:
        return CameraModel(
            render_queue=self.render_queue,
            terrain_group_0=self.terrain_group_0,
            terrain_group_1=self.terrain_group_1,
            terrain_group_2=self.terrain_group_2,
            gui_group=self.gui_group,
            effect_group=self.effect_group,
            entities_group_0=self.entities_group_0,
            entities_group_1=self.entities_group_1,
            entities_group_2=self.entities_group_2,
            light_group=self.light_group,
            light_intensities=[self.light_intensities.get(i, 0.0) for i in range(LAYERS_TO_DRAW)],
            views=self.views,
            LAYERS_TO_DRAW=LAYERS_TO_DRAW,
            settings=self.settings,
        )

    def non_terrain_entity_handler(
        self,
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
    ) -> EntityInterface | None:
        ui_view_lifebar: BaseView | None
        if "lifebar" in self.views["camera-ui"]:
            ui_view_lifebar = self.views["camera-ui"]["lifebar"]
        else:
            ui_view_lifebar = None

        _screen_x, _screen_y = None, None
        is_moving = entity.has_component(aetherion.ComponentFlag.MOVING_COMPONENT.value)
        if is_moving and entity.get_entity_type().main_type == aetherion.EntityEnum_BEAST:
            _screen_x, _screen_y = self._beast_entity_handler(
                camera_model,
                camera_settings,
                world_view,
                entity,
                view_object,
                screen_x,
                screen_y,
                layer_index,
                mouse_state,
                selected_entity,
                entity_hovered,
                sun_light,
            )
        else:
            _screen_x, _screen_y = self._plant_entity_handler(
                camera_model,
                camera_settings,
                world_view,
                entity,
                view_object,
                screen_x,
                screen_y,
                layer_index,
                mouse_state,
                selected_entity,
                entity_hovered,
                sun_light,
            )

        # TODO: This should be another entity_handler implementation.
        if ui_view_lifebar is not None:
            max_health_level = entity.get_health().max_health
            health_level = entity.get_health().health_level

            ui_view_lifebar.set_health(health_level, max_health_level)
            ui_view_lifebar.set_position_draw(
                camera_model.render_queue, _screen_x, _screen_y, layer_index=layer_index, group=camera_model.gui_group
            )

        return entity_hovered

    def draw_player_perspective_layer(
        self,
        world_view: WorldView,
        z: int,
        top_left: tuple[int, int],
        blocks_width: int,
        blocks_height: int,
        screen_x_offset: int,
        screen_y_offset: int,
        entity_hovered: EntityInterface | None,
        selected_entity: int | None,
        layer_index: int,
        mouse_state: MouseState,
        player: EntityInterface,
        sun_light: float,
        water_camera_stats: bool,
        terrain_gradient_camera_stats: bool,
        iterate_right_to_left: bool = False,
        iterate_bottom_to_top: bool = False,
    ) -> EntityInterface | None:
        world_shape_x = world_view.width
        world_shape_y = world_view.height

        if iterate_bottom_to_top:
            screen_y_initial = (blocks_height - 1) * self.settings.tile_size_on_screen + screen_y_offset
        else:
            screen_y_initial = 0 + screen_y_offset

        screen_y = screen_y_initial

        world_layer_x_start = int(top_left[0])
        world_layer_y_start = int(top_left[1])

        water_view: BaseView | None
        if aetherion.TerrainEnum_WATER in self.views["terrains"]:
            water_view = self.views["terrains"][aetherion.TerrainEnum_WATER]
        else:
            water_view = None

        gradient_view_object: BaseView | None
        if "gradient_arrow" in self.views["camera-ui"]:
            gradient_view_object = self.views["camera-ui"]["gradient_arrow"]
        else:
            gradient_view_object = None

        ui_view_lifebar: BaseView | None
        if "lifebar" in self.views["camera-ui"]:
            ui_view_lifebar = self.views["camera-ui"]["lifebar"]
        else:
            ui_view_lifebar = None

        for j in range(blocks_height):
            if iterate_right_to_left:
                screen_x = (blocks_width - 1) * self.settings.tile_size_on_screen - screen_x_offset
                world_layer_x = world_layer_x_start + blocks_width - 1
            else:
                screen_x = 0 - screen_x_offset
                world_layer_x = world_layer_x_start

            if iterate_bottom_to_top:
                world_layer_y = world_layer_y_start + blocks_height - 1 - j
            else:
                world_layer_y = world_layer_y_start + j

            for i in range(blocks_width):
                terrain: EntityInterface | None = (
                    world_view.get_terrain(world_layer_x, world_layer_y, z)
                    if world_view.check_if_terrain_exist(world_layer_x, world_layer_y, z)
                    else None
                )
                entity: EntityInterface | None = (
                    world_view.get_entity(world_layer_x, world_layer_y, z)
                    if world_view.check_if_entity_exist(world_layer_x, world_layer_y, z)
                    else None
                )
                event = None
                if (
                    world_layer_x >= 0
                    and world_layer_x < world_shape_x
                    and world_layer_y >= 0
                    and world_layer_y < world_shape_y
                ):
                    if entity:
                        classification = Classification(
                            entity.get_entity_type().main_type, entity.get_entity_type().sub_type0
                        )
                        view_object = self.views["entities"].get(classification)
                    else:
                        classification = None
                        view_object = None

                    if view_object is not None:
                        entity_hovered = self.non_terrain_entity_handler(
                            self._camera_model,
                            self.settings,
                            world_view,
                            entity,
                            view_object,
                            screen_x,
                            screen_y,
                            layer_index,
                            mouse_state,
                            selected_entity,
                            entity_hovered,
                            sun_light,
                        )

                if (
                    world_layer_x >= 0
                    and world_layer_x < world_shape_x
                    and world_layer_y >= 0
                    and world_layer_y < world_shape_y
                ):
                    view_object = (
                        self.views["terrains"].get(terrain.get_entity_type().sub_type0)
                        if terrain and aetherion.should_draw_terrain(terrain, self.settings.empty_tile_debugging)
                        else None
                    )

                    if terrain:
                        aetherion.draw_tile_effects(
                            terrain,
                            world_view,
                            self.render_queue,
                            layer_index,
                            self.gui_group,
                            screen_x,
                            screen_y,
                            self.settings.tile_size_on_screen,
                        )

                    if view_object is not None and not aetherion.is_terrain_an_empty_water(terrain):
                        current_entity_hovered = self._terrain_handler(
                            self._camera_model,
                            self.settings,
                            terrain,
                            view_object,
                            selected_entity,
                            world_view,
                            mouse_state,
                            screen_x,
                            screen_y,
                            layer_index,
                            entity_hovered,
                            sun_light,
                            player,
                            gradient_view_object,
                            water_view,
                        )
                        if current_entity_hovered:
                            entity_hovered = terrain

                if iterate_right_to_left:
                    world_layer_x -= 1
                    screen_x -= self.settings.tile_size_on_screen
                else:
                    world_layer_x += 1
                    screen_x += self.settings.tile_size_on_screen

            if iterate_bottom_to_top:
                screen_y -= self.settings.tile_size_on_screen
            else:
                screen_y += self.settings.tile_size_on_screen
        return entity_hovered

    def draw_player_perspective(
        self, response: Any, shared_state: SharedState, player: BeastEntity | None = None
    ) -> None:
        """Render the world from the player's dimetric perspective.

        :param response: Server response containing world view and player entity.
        :param shared_state: Shared game state (mouse, selection, etc.).
        """
        if response is None:
            return

        self.world_view = aetherion.WorldView.deserialize_flatbuffer(response.getWorldView())
        if self.world_view is None:
            return

        # TODO: Make the camera module to use better the events.
        mouse_state: MouseState = shared_state.mouse_state
        for mouse_event in self.topic_reader.drain():
            if mouse_event.event_type == InputEventActionType.MOUSE_LEFT_BUTTON_DOWN:
                mouse_state["left"] = True
            elif mouse_event.event_type == InputEventActionType.MOUSE_LEFT_BUTTON_UP:
                mouse_state["left"] = False
            elif mouse_event.event_type == InputEventActionType.MOUSE_RIGHT_BUTTON_DOWN:
                mouse_state["right"] = True
            elif mouse_event.event_type == InputEventActionType.MOUSE_RIGHT_BUTTON_UP:
                mouse_state["right"] = False
        shared_state.mouse_state = mouse_state

        sdl2.SDL_RenderClear(self.renderer._renderer)

        # import cProfile
        # import io
        # import pstats
        # from pstats import SortKey

        # pr = cProfile.Profile(timeunit=0.0001)
        # pr.enable()
        # with world.events_lock:

        player_entity_interface = response.getEntity()
        if player is None:
            raise Exception("Player entity is None in camera draw_player_perspective")
        #     player = BeastEntity.from_entity_interface(player_entity_interface)
        sun_light = get_sun_light(shared_state)
        water_camera_stats: bool = shared_state.water_camera_stats
        terrain_gradient_camera_stats: bool = shared_state.terrain_gradient_camera_stats

        blocks_width = self.settings.blocks_in_screen["width"]
        blocks_height = self.settings.blocks_in_screen["height"]

        top_left = (
            player.x - self.settings.player_block_position.get("x"),
            player.y - self.settings.player_block_position.get("y"),
        )

        layer_to_draw = player.z - self.settings.layers_bellow_player

        screen_x_offset = self.settings.tile_size_on_screen - (
            self.settings.tile_size_on_screen * self.settings.layers_bellow_player
        )
        screen_y_offset = self.settings.tile_size_on_screen - (
            self.settings.tile_size_on_screen * self.settings.layers_bellow_player
        )

        screen_x_offset -= self.settings.camera_screen_width_adjust_offset
        screen_y_offset += self.settings.camera_screen_height_adjust_offset

        screen_y_offset += self.settings.tile_size_on_screen * self.settings.layers_to_draw

        screen_x_offset, screen_y_offset, _ = adjust_camera_entity_moving(
            self.world_view, player, screen_x_offset, screen_y_offset, self.settings.tile_size_on_screen
        )

        # start_time = time.perf_counter()

        entities_selected = {}
        selected_entity: int | None = shared_state.selected_entity
        z_shape = self.world_view.depth
        for layer_index in range(LAYERS_TO_DRAW):
            if layer_to_draw >= 0 and layer_to_draw < z_shape:
                entity_hovered = None
                entity_hovered = self.draw_player_perspective_layer(
                    self.world_view,
                    layer_to_draw,
                    top_left,
                    blocks_width,
                    blocks_height,
                    screen_x_offset,
                    screen_y_offset,
                    entity_hovered,
                    selected_entity,
                    layer_index,
                    mouse_state,
                    player_entity_interface,
                    sun_light,
                    water_camera_stats,
                    terrain_gradient_camera_stats,
                    self.settings.camera_iterate_right_to_left,
                    self.settings.camera_iterate_bottom_to_top,
                )
                entities_selected[layer_to_draw] = entity_hovered
            else:
                logger.debug(f"Z layer {layer_to_draw} not rendered (out of bounds)")
            layer_to_draw = layer_to_draw + 1
            screen_x_offset += self.settings.tile_size_on_screen
            screen_y_offset -= self.settings.tile_size_on_screen

        # end_time = time.perf_counter()
        # time_spent_rendering = end_time - start_time
        # print(f"Time spent preparing renquer_queue: {time_spent_rendering}")
        # start_time = end_time

        self.render_queue.render(self.renderer.renderer_ptr)
        self.render_queue.clear()

        # end_time = time.perf_counter()
        # time_spent_rendering = end_time - start_time
        # print(f"Time spent rendering and cleanning renquer_queue: {time_spent_rendering}")

        selected_entity = None
        if entities_selected:
            for z, entity in entities_selected.items():
                if entity:
                    selected_entity = entity.get_entity_id()

        if (
            mouse_state.get("right")
            and shared_state.selected_entity is not None
            and shared_state.selected_entity == selected_entity
        ):
            logger.debug("right click pressed")
            shared_state.selected_entity = None
        elif mouse_state.get("right") and selected_entity is not None:
            logger.debug("right click pressed")
            logger.debug(f"{entities_selected}")
            shared_state.selected_entity = selected_entity
            shared_state.selected_entity_interface = self.world_view.get_entity_by_id(selected_entity)
            shared_state.selected_entity_just_set = True
        elif mouse_state.get("left"):
            logger.debug(f"{shared_state.selected_entity}")
            logger.debug(f"{entities_selected}")
        shared_state.hovered_entity = selected_entity
        shared_state.hovered_entity_interface = (
            self.world_view.get_entity_by_id(selected_entity) if selected_entity else None
        )

        # pr.disable()
        # s = io.StringIO()
        # sortby = SortKey.CUMULATIVE
        # ps = pstats.Stats(pr, stream=s).sort_stats(sortby)
        # ps.print_stats()
        # print(s.getvalue())

        # pr.disable()
        # s = io.StringIO()
        # sortby = SortKey.CUMULATIVE
        # ps = pstats.Stats(pr, stream=s).sort_stats(sortby)
        # ps.print_stats()
        # print(s.getvalue())

        # print("stop")
