import ctypes
import multiprocessing
import time
from typing import Any, Callable

import sdl2
from sdl2 import SDL_Event

import aetherion

# from aetherion import GameWindow
from aetherion import (
    BasicGameWindow,
    BeastConnectionMetadata,
    EventBus,
    GameEventType,
    GameWindow,
    InputActionProcessor,
    OpenGLGameWindow,
    PerceptionResponseFlatB,
    PubSubTopicBroker,
    Renderer,
    SharedState,
    World,
    WorldInterfaceMetadata,
    get_renderer,
)
from aetherion.audio.audio_manager import AudioManager
from aetherion.audio.sdl_utils import init_mixer
from aetherion.audio.sound_effects import SoundEffectManager
from aetherion.engine_config import EngineConfig
from aetherion.entities.base import Classification
from aetherion.entities.beasts import BeastEntity
from aetherion.events.action_event import InputEventActionType
from aetherion.logger import logger
from aetherion.networking.admin_connection import ServerAdminConnection
from aetherion.networking.connection import (
    BeastConnection,
    ServerBeastConnection,
)
from aetherion.paths import resolve_path
from aetherion.renderer.sprites import Sprite
from aetherion.renderer.views import BaseView
from aetherion.resource_manager import ResourceManager
from aetherion.scenes.base import BaseScene, SceneManager
from aetherion.scheduler import Scheduler
from aetherion.world.constants import WorldInstanceTypes
from aetherion.world.interface import WorldInterface
from aetherion.world.manager import WorldManager


def init_sdl2() -> None:
    # Initialize SDL2 and its extensions
    sdl2.ext.init()  # pyright: ignore
    sdl2.sdlimage.IMG_Init(sdl2.sdlimage.IMG_INIT_PNG)  # pyright: ignore
    sdl2.sdlttf.TTF_Init()  # pyright: ignore


def init_sdl2_audio() -> None:
    # Initialize SDL with the audio subsystem
    if sdl2.SDL_Init(sdl2.SDL_INIT_AUDIO) != 0:  # pyright: ignore
        error = sdl2.SDL_GetError()  # pyright: ignore
        logger.error(f"SDL_Init Error: {error}")
        raise RuntimeError(f"SDL_Init Error: {error}")


def on_close():
    # Clean up resources
    sdl2.sdlttf.TTF_Quit()  # pyright: ignore
    sdl2.sdlimage.IMG_Quit()  # pyright: ignore
    sdl2.ext.quit()  # pyright: ignore
    logger.info("Application closed.")
    exit()


def shutdown_all_children():
    children = multiprocessing.active_children()
    print(f"Shutting down {len(children)} child processes...")
    for child in children:
        child.terminate()  # Request termination
    for child in children:
        child.join()


class GameEngine:
    game_window: GameWindow | None = None
    renderer: Renderer | None = None
    scheduler: Scheduler | None = None
    shared_state: SharedState | None = None
    scene_manager: SceneManager
    world_manager: WorldManager
    running: bool = False

    # Should be fixed to a world interface instance and to a player connection - World instance type
    # And not necessarily to the game engine.
    world_type: WorldInstanceTypes = WorldInstanceTypes.SYNCHRONOUS
    world_interface: WorldInterface | None = None

    player: BeastEntity | None = None
    server_admin_connection: ServerAdminConnection | None = None
    player_connection: BeastConnection | None = None
    views: dict[str, dict[int | Classification | str, BaseView]] | None = None
    user_input_controller = None
    event_bus: EventBus[GameEventType] | None = None
    input_action_processor: InputActionProcessor | None = None

    window_ptr: int | None = None

    def __init__(
        self,
        config: EngineConfig,
        views: dict[str, dict[int | Classification | str, BaseView]] | None = None,
    ):
        """Initialize the game engine with provided configuration and optional views."""
        if config.user_input_controller_class is None:
            raise ValueError("user_input_controller_class must be provided in config.")
        if config.player_input_map_factory is None:
            raise ValueError("player_input_map_factory must be provided in config.")
        self.config: EngineConfig = config

        self.user_input_controller_class: type[Any] = config.user_input_controller_class
        self.player_input_map_factory: Callable[[BeastConnection], dict[InputEventActionType, Any]] = (
            config.player_input_map_factory
        )

        init_sdl2()
        self.init_window()
        self.init_renderer()
        self.init_scheduler()
        self.audio_manager: AudioManager = AudioManager()
        self.sound_effect_manager: SoundEffectManager = SoundEffectManager()
        self.resource_manager: ResourceManager = ResourceManager()
        self.event_bus = EventBus()
        self.pubsub_broker: PubSubTopicBroker[InputEventActionType] = PubSubTopicBroker()
        self.scene_manager = SceneManager()

        self.world_manager = WorldManager(
            self.event_bus,
            ai_manager_factory=config.ai_manager_factory,
            event_handlers=config.worldmanager_event_handlers,
            default_event_handlers=config.worldmanager_event_handlers,
        )
        self.beast_connection_metadata: dict[str, BeastConnectionMetadata] = {}

        self.views = views if views is not None else {}
        self.shared_state = SharedState()

    def register_world_factory(self, name: str, factory: Callable[[], World]) -> None:
        """Register a world factory under a name."""
        self.world_manager.register_factory(name, factory)

    def set_world_type(self, world_type: WorldInstanceTypes = WorldInstanceTypes.SYNCHRONOUS):
        self.world_type = world_type

    def init_engine_components(self):
        self.set_input_action_processor()
        self.user_input_controller = self.user_input_controller_class(self.pubsub_broker, self.event_bus)
        self.user_input_controller.start()

    @property
    def renderer_ptr(self) -> int:
        """Return the renderer pointer."""
        if self.renderer is None:
            raise RuntimeError("Renderer is not initialized.")
        return self.renderer.renderer_ptr

    def set_window_ptr(self) -> None:
        """Set the window pointer for the game window."""
        if self.game_window is None:
            raise RuntimeError("Game window is not initialized.")

        self.window_ptr = ctypes.cast(self.game_window.window, ctypes.c_void_p).value

    def init_window(self, opengl: bool = True):
        logger.info(f"SCREEN_WIDTH: {self.config.screen_width}; SCREEN_HEIGHT: {self.config.screen_height}")

        # Create the SDL2 window using GameWindow
        window_flags = sdl2.SDL_WINDOW_RESIZABLE
        width, height = self.config.screen_width, self.config.screen_height
        if opengl:
            self.game_window = OpenGLGameWindow("The Life Simulator", width, height, window_flags)
        else:
            self.game_window = BasicGameWindow("The Life Simulator", width, height, window_flags)
        self.game_window.show()

        self.set_window_ptr()

    def init_renderer(self) -> None:
        """Initialize the renderer based on the game window type."""

        if self.game_window is None:
            raise RuntimeError("Game window must be initialized before creating a renderer.")

        self.renderer = get_renderer(self.game_window)

    def init_scheduler(self):
        self.scheduler = Scheduler()

    def set_views(self, views: dict[str, dict[int | Classification | str, BaseView]] | None = None) -> None:
        """Initialize views for the game engine."""
        self.views = views

    def set_player_connection(self, player_connection: BeastConnection) -> None:
        self.player_connection = player_connection

    def set_input_controller(self, user_input_controller) -> None:
        self.user_input_controller = user_input_controller

    def add_scene(self, scene_name: str, scene_class: type[BaseScene], **kwargs: dict[str, object]) -> None:
        """Add a scene to the scene manager."""
        if self.game_window is None or self.renderer is None:
            raise RuntimeError("Game window and renderer must be initialized before adding scenes.")

        scene_instance: BaseScene = scene_class(
            self.game_window, self.renderer, self.views, self.event_bus, self.pubsub_broker, **kwargs
        )
        _ = self.scene_manager.register(scene_name, scene_instance)

    def register_inventory_empty_sprite(self, empty_slot_sprite: Sprite) -> None:
        """Register a sprite to be used as an empty inventory slot."""
        self.resource_manager.register(key="default_texture", resource=empty_slot_sprite)

    def get_scene(self, scene_name: str) -> BaseScene:
        return self.scene_manager.get_scene(scene_name)

    def change_scene(self, scene_name: str):
        """Change to a registered scene by name."""
        self.scene_manager.change(scene_name)

    def set_input_action_processor(self):
        if not self.player_connection:
            # logger.error("Player connection is not set. Cannot create input action processor.")
            return

        if self.player_input_map_factory:
            command_specs = self.player_input_map_factory(self.player_connection)
            self.input_action_processor = InputActionProcessor(self.pubsub_broker, command_specs=command_specs)

    def process_input_queue(self, shared_state: SharedState):
        if not self.input_action_processor:
            # logger.debug("Input action processor is not set. Cannot process input queue.")
            return
        self.input_action_processor.process_inputs(shared_state)

    def terminate(self):
        if self.world_manager.ai_manager:
            logger.info("Terminating AI Manager...")
            self.world_manager.ai_manager.terminate()
        # shutdown_all_children()

    def check_if_imgui_wants_capture_keyboard(self, ready_to_render_imgui: bool) -> bool:
        """Check if ImGui wants to capture keyboard input."""
        if self.world_type == WorldInstanceTypes.SYNCHRONOUS and ready_to_render_imgui:
            return aetherion.wants_capture_keyboard()
        return False

    def clear_screen(self) -> None:
        # Clear the renderer with a gray background (within the collor pallete)
        sdl2.SDL_SetRenderDrawColor(self.renderer._renderer, 65, 68, 83, 255)
        sdl2.SDL_RenderClear(self.renderer._renderer)

    def handle_input_events(self, event: SDL_Event, shared_state: SharedState) -> SharedState:
        self.user_input_controller.set_steps_to_advance()

        self.user_input_controller.reset_mouse_state()
        while sdl2.SDL_PollEvent(ctypes.byref(event)) != 0:
            if not self.check_if_imgui_wants_capture_keyboard(shared_state.ready_to_render_imgui):
                self.user_input_controller.check_key_state(event)
            self.user_input_controller.check_mouse_state(event)

            # Convert SDL_Event to bytes
            event_bytes = ctypes.string_at(ctypes.byref(event), ctypes.sizeof(event))

            if shared_state.ready_to_render_imgui:
                aetherion.imgui_process_event(event_bytes)

            # Handle quit events
            if event.type == sdl2.SDL_QUIT:
                self.running = False
                # game_engine.terminate()
                break
            elif event.type == sdl2.SDL_WINDOWEVENT:
                if event.window.event == sdl2.SDL_WINDOWEVENT_CLOSE:
                    # game_engine.terminate()
                    self.running = False
                    raise Exception("Game window closed by user")

        # TODO: Should this be part of the input controller - or gui_handler??
        if not self.check_if_imgui_wants_capture_keyboard(shared_state.ready_to_render_imgui):
            self.user_input_controller.process_key_state(imgui_debug_settings={})

        mouse_state: dict[str, int | bool] = self.user_input_controller.get_mouse_state()
        shared_state.mouse_state = mouse_state
        shared_state.fastforward_count = (
            self.user_input_controller.steps_to_advance
            if self.user_input_controller.steps_to_advance > 1
            else shared_state.fastforward_count
        )

        self.process_input_queue(shared_state)

        return shared_state

    def _should_poll_world(self) -> bool:
        pc: BeastConnection | None = self.player_connection
        return (
            pc is not None and pc.server_online and (isinstance(pc, ServerBeastConnection) and not pc.streaming_enabled)
        )

    def run(self) -> None:
        self.running = True
        self.init_engine_components()

        font_path: str = str(resolve_path("res://assets/Toriko.ttf"))
        aetherion.imgui_init(self.window_ptr, self.renderer_ptr, font_path)
        init_sdl2_audio()
        init_mixer()

        self.clear_screen()

        event: SDL_Event = SDL_Event()

        self.shared_state.desired_fps = self.config.fps
        console_logs: list[Any] = []  # noqa: F841 -- reserved for future use
        # shared_state.fastforward_count = 0
        fps = 0  # noqa: F841 -- frame diagnostics

        try:
            while self.running:
                beginning_of_frame = time.time()

                # ready_to_render_imgui: bool = (
                #     self.player_connection.ready_to_render_world if self.player_connection else False
                # )
                # ready_to_render_imgui: bool = True
                # shared_state.ready_to_render_imgui = ready_to_render_imgui
                self.shared_state = self.handle_input_events(event, self.shared_state)

                # Clear the renderer with a gray background (within the collor pallete)
                self.clear_screen()

                if self.player_connection and not self.player_connection.server_online:
                    self.shared_state.response = None
                elif self.player_connection and self.player_connection.server_online:
                    response: PerceptionResponseFlatB | None = self.player_connection.wait_for_next_world_state(
                        self.shared_state
                    )
                    self.shared_state.response = response
                else:
                    self.shared_state.response = None

                self.world_manager.process_ai_decisions()

                self.shared_state = self.scheduler.execute_scheduled_funcs(self.shared_state)
                self.scene_manager.update(0, self.shared_state, self.player_connection)
                self.scene_manager.render(self.renderer, self.shared_state, self.player_connection)

                if self._should_poll_world():
                    self.player_connection.request_world_state()

                if self.player_connection and self.shared_state.selected_entity_just_set:
                    print("selecting eco entity to be debugged")
                    self.shared_state.selected_entity_just_set = False
                    self.player_connection.set_entity_to_debug(self.shared_state.selected_entity)

                if self.shared_state.ready_to_render_imgui and self.shared_state.needs_render_imgui:
                    self.shared_state.needs_render_imgui = False
                    aetherion.imgui_render(self.renderer_ptr)

                # Present the renderer
                sdl2.SDL_RenderPresent(self.renderer._renderer)

                if self.event_bus:
                    self.event_bus.process_events()
                self.shared_state.all_world_metadata = self.world_manager.worlds_metadata
                self.shared_state.all_beast_connection_metadata = self.beast_connection_metadata

                if self.shared_state.connected_world:
                    world_metadata: WorldInterfaceMetadata | None = self.world_manager.worlds_metadata.get(
                        self.shared_state.connected_world
                    )
                    self.world_manager.update_shared_state_snapshots(self.shared_state)
                    if world_metadata and world_metadata.type == WorldInstanceTypes.SYNCHRONOUS:
                        # Update the world through the manager (respects pause/play/step state)
                        self.world_manager.update()
                    elif world_metadata and world_metadata.type == WorldInstanceTypes.SERVER:
                        # Handle server world type
                        pass
                    else:
                        # If no world is connected, reset the world interface
                        raise RuntimeError("No world is connected or the world type is not recognized.")

                # Cap the frame rate
                total_time_spent = time.time() - beginning_of_frame
                frame_delay: float = 1 / self.shared_state.desired_fps
                sleep_time = max(0, frame_delay - (total_time_spent))
                fps: float = 1 / (total_time_spent)  # noqa: F841 -- frame diagnostics
                self.shared_state.fps = fps

                if self.shared_state.fastforward_count > 0:
                    self.shared_state.fastforward_count -= 1
                else:
                    time.sleep(sleep_time)

        except aetherion.EcosystemEngineException as e:
            import traceback

            traceback.print_exc()
            logger.error(f"❌ CRITICAL: Water simulation error occurred: {e}")

            # Check for detailed errors
            if self.world_manager.current:
                errors = self.world_manager.current.world.get_water_sim_errors()
                if errors:
                    logger.error(f"Water simulation errors ({len(errors)} errors):")
                    for error in errors:
                        logger.error(f"  Thread {error.thread_id}: {error.error_message}")

            # Re-raise to stop the game loop
            raise
        except Exception as e:
            import traceback

            traceback.print_exc()
            logger.error(f"An error occurred: {e}")

        finally:
            # Clean up resources
            self.audio_manager.cleanup()
            self.sound_effect_manager.cleanup()
            self.terminate()
            on_close()
