# import queue
from time import time
from typing import Any

import sdl2
from sdl2 import (
    SDLK_0,
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_4,
    SDLK_5,
    SDLK_6,
    SDLK_7,
    SDLK_8,
    SDLK_9,
    SDLK_DOWN,
    SDLK_LEFT,
    SDLK_RIGHT,
    SDLK_SPACE,
    SDLK_UP,
    SDLK_f,
    SDLK_l,
    SDLK_p,
    SDLK_s,
    SDLK_t,
)

from aetherion import EventBus, PubSubTopicBroker, TopicReader
from aetherion.events import GameEvent, GameEventType
from aetherion.events.action_event import InputEventActionType
from aetherion.logger import logger


class UserInputController:
    def __init__(
        self, pubsub_broker: PubSubTopicBroker[InputEventActionType], event_bus: EventBus[GameEventType]
    ) -> None:
        self.pubsub_broker: PubSubTopicBroker[InputEventActionType] = pubsub_broker
        self.event_bus: EventBus[GameEventType] = event_bus
        self.topic_name: str = "input_action_queue"
        self.mouse_topic_name: str = "mouse_action_queue"
        self.topic_reader: TopicReader[InputEventActionType] = TopicReader(
            bus=self.pubsub_broker, topics=[self.topic_name]
        )
        self.keys_down = {}  # Dictionary tracking key states
        self.last_key_pressed: dict[Any, float] = {
            SDLK_LEFT: 0.0,
            SDLK_RIGHT: 0.0,
            SDLK_UP: 0.0,
            SDLK_DOWN: 0.0,
            SDLK_SPACE: 0.0,
            SDLK_l: 0.0,
        }
        self.world = None  # Assign your world instance here if needed
        self.tracked_keys = {
            sdl2.SDLK_LEFT: "LEFT",
            sdl2.SDLK_RIGHT: "RIGHT",
            sdl2.SDLK_UP: "UP",
            sdl2.SDLK_DOWN: "DOWN",
            sdl2.SDLK_SPACE: "SPACE",
        }
        self.last_mouse_state: dict[str, int | bool] = {"x": 0, "y": 0, "left": False, "right": False}
        self.steps_to_advance = 0

    def start(self):
        pass

    def reset_mouse_state(self):
        self.last_mouse_state = {**self.last_mouse_state, "left": False, "right": False}

    def check_mouse_state(self, event: sdl2.SDL_Event) -> None:
        mouse_x, mouse_y = None, None
        if event.type == sdl2.SDL_MOUSEMOTION:
            mouse_x, mouse_y = event.motion.x, event.motion.y
            self.last_mouse_state["x"] = mouse_x
            self.last_mouse_state["y"] = mouse_y

        elif event.type == sdl2.SDL_MOUSEBUTTONDOWN:
            # Check which mouse button was pressed

            _ = self.event_bus.emit(
                event_type=GameEventType.AUDIO_SOUND_EFFECT_PLAY,
                data={
                    "name": "button_click",
                    "file_path": "res://assets/soundeffects/ES_ButtonClickInputResponseTapShort-Epidemic_Sound.wav",
                    "fade_ms": 1000,
                    "volume": 64,
                },
                source="input_controller",
            )

            if event.button.button == sdl2.SDL_BUTTON_LEFT:
                self.pubsub_broker.publish_event(
                    self.mouse_topic_name, GameEvent(event_type=InputEventActionType.MOUSE_LEFT_BUTTON_DOWN)
                )
                logger.info("Left mouse button pressed.")
            elif event.button.button == sdl2.SDL_BUTTON_RIGHT:
                self.pubsub_broker.publish_event(
                    self.mouse_topic_name, GameEvent(event_type=InputEventActionType.MOUSE_RIGHT_BUTTON_DOWN)
                )
                logger.info("Right mouse button pressed.")

        elif event.type == sdl2.SDL_MOUSEBUTTONUP:
            # Check which mouse button was released
            if event.button.button == sdl2.SDL_BUTTON_LEFT:
                self.pubsub_broker.publish_event(
                    self.mouse_topic_name, GameEvent(event_type=InputEventActionType.MOUSE_LEFT_BUTTON_UP)
                )
                logger.info("Left mouse button released.")
            elif event.button.button == sdl2.SDL_BUTTON_RIGHT:
                self.pubsub_broker.publish_event(
                    self.mouse_topic_name, GameEvent(event_type=InputEventActionType.MOUSE_RIGHT_BUTTON_UP)
                )
                logger.info("Right mouse button released.")

    def set_steps_to_advance(self):
        self.steps_to_advance = 0

    def get_mouse_state(self) -> dict[str, int | bool]:
        return self.last_mouse_state

    def check_key_state(self, event: sdl2.SDL_Event):
        if event.type == sdl2.SDL_KEYDOWN:
            key = event.key.keysym.sym
            if key in self.tracked_keys:
                if self.last_key_pressed[key] == 0:
                    self.last_key_pressed[key] = time()
                    logger.info(f"{self.tracked_keys[key]} key pressed at {self.last_key_pressed[key]:.2f}")
        elif event.type == sdl2.SDL_KEYUP:
            key = event.key.keysym.sym
            if key in self.tracked_keys:
                self.last_key_pressed[key] = 0
                logger.info(f"{self.tracked_keys[key]} key released and timer reset")

        # Perform movement based on the last pressed key
        if event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_SPACE:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.JUMP))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_p:
            if self.world:
                self.world.pause_debug = True
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_f:
            # TODO: Missing to add into client-server architecture
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.TAKE_ITEM))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_1:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_0))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_2:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_1))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_3:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_2))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_4:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_3))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_5:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_4))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_6:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_5))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_7:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_6))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_8:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_7))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_9:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_8))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_0:
            self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.USE_ITEM_9))
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_l:
            if self.last_key_pressed[SDLK_l] == 0:
                self.last_key_pressed[SDLK_l] = time()
            else:
                now: float = time()
                time_passed: float = now - self.last_key_pressed[SDLK_l]
                if time_passed >= 0.1:
                    self.last_key_pressed[SDLK_l] = 0
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_t:
            steps_to_advance = 6000
            self.steps_to_advance = steps_to_advance
        elif event.type == sdl2.SDL_KEYDOWN and event.key.keysym.sym == SDLK_s:
            breakpoint()
            logger.info("stop")

    def process_key_state(self, imgui_debug_settings: dict[str, bool] = {}):
        # Determine the last keys pressed among arrow keys, sorted by the time they were pressed
        last_keys_pressed_sorted = {
            k: v for k, v in sorted(self.last_key_pressed.items(), key=lambda item: item[1], reverse=True) if v != 0
        }

        # Convert to a list to access elements by index
        keys_list = list(last_keys_pressed_sorted.keys())

        # Get the first and second pressed keys
        last_pressed_key = keys_list[0] if len(keys_list) > 0 else None
        second_pressed_key = keys_list[1] if len(keys_list) > 1 else None

        if imgui_debug_settings.get("allowMultiDirection", True):
            if last_pressed_key == SDLK_LEFT and not second_pressed_key:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_LEFT))
            elif last_pressed_key == SDLK_RIGHT and not second_pressed_key:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_RIGHT))
            elif last_pressed_key == SDLK_UP and not second_pressed_key:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_UP))
            elif last_pressed_key == SDLK_DOWN and not second_pressed_key:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_DOWN))
            elif last_pressed_key == SDLK_SPACE and not second_pressed_key:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.JUMP))

            elif (last_pressed_key == SDLK_LEFT and second_pressed_key == SDLK_UP) or (
                last_pressed_key == SDLK_UP and second_pressed_key == SDLK_LEFT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_LEFT_UP)
                )
            elif (last_pressed_key == SDLK_LEFT and second_pressed_key == SDLK_DOWN) or (
                last_pressed_key == SDLK_DOWN and second_pressed_key == SDLK_LEFT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_LEFT_DOWN)
                )

            elif (last_pressed_key == SDLK_RIGHT and second_pressed_key == SDLK_UP) or (
                last_pressed_key == SDLK_UP and second_pressed_key == SDLK_RIGHT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_RIGHT_UP)
                )
            elif (last_pressed_key == SDLK_RIGHT and second_pressed_key == SDLK_DOWN) or (
                last_pressed_key == SDLK_DOWN and second_pressed_key == SDLK_RIGHT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_RIGHT_DOWN)
                )

            elif (last_pressed_key == SDLK_LEFT and second_pressed_key == SDLK_SPACE) or (
                last_pressed_key == SDLK_SPACE and second_pressed_key == SDLK_LEFT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_LEFT_JUMP)
                )

            elif (last_pressed_key == SDLK_RIGHT and second_pressed_key == SDLK_SPACE) or (
                last_pressed_key == SDLK_SPACE and second_pressed_key == SDLK_RIGHT
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_RIGHT_JUMP)
                )

            elif (last_pressed_key == SDLK_UP and second_pressed_key == SDLK_SPACE) or (
                last_pressed_key == SDLK_SPACE and second_pressed_key == SDLK_UP
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_UP_JUMP)
                )

            elif (last_pressed_key == SDLK_DOWN and second_pressed_key == SDLK_SPACE) or (
                last_pressed_key == SDLK_SPACE and second_pressed_key == SDLK_DOWN
            ):
                self.pubsub_broker.publish_event(
                    self.topic_name, GameEvent(event_type=InputEventActionType.WALK_DOWN_JUMP)
                )
        else:
            if last_pressed_key == SDLK_LEFT:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_LEFT))
            elif last_pressed_key == SDLK_RIGHT:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_RIGHT))
            elif last_pressed_key == SDLK_UP:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_UP))
            elif last_pressed_key == SDLK_DOWN:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.WALK_DOWN))
            elif last_pressed_key == SDLK_SPACE:
                self.pubsub_broker.publish_event(self.topic_name, GameEvent(event_type=InputEventActionType.JUMP))
