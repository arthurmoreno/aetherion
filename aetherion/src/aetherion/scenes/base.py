from abc import ABC, abstractmethod
from typing import Self

from pydantic import BaseModel, ConfigDict, Field

from aetherion import GameWindow, SceneGraph


class NodeSpec(BaseModel):
    """Specification for scene graph nodes with validation and serialization support."""

    name: str
    cls: str  # import path to class, e.g. "game.scenes.scene_a.nodes.player.PlayerController"
    kwargs: dict[str, object] = Field(default_factory=dict)
    children: list[Self] = Field(default_factory=list)
    uuid: int | None = None  # stable id (reference for internal entt) for hot-reload mapping

    model_config = ConfigDict(
        arbitrary_types_allowed=True,
        validate_assignment=True,
        str_strip_whitespace=True,
    )


class Node(ABC):
    """Base class for all scene graph nodes."""

    def __init__(self, name: str, **kwargs):
        self.name = name
        self.children: list[Node] = []
        self.parent: Node | None = None

    @abstractmethod
    def on_load(self):
        """Called when the node is loaded."""
        pass

    @abstractmethod
    def on_enter(self):
        """Called when the node becomes active."""
        pass

    @abstractmethod
    def update(self, dt: float, shared_state: dict):
        """Update the node logic."""
        pass

    @abstractmethod
    def render(self, renderer, shared_state, player_connection=None):
        """Draw the current frame."""
        pass

    @abstractmethod
    def on_unload(self):
        """Free resources (called once)."""
        pass


class BaseScene(ABC):
    """Defines the lifecycle of a game scene."""

    scene_graph: SceneGraph

    @abstractmethod
    def __init__(self, game_window: GameWindow, renderer, views, event_bus, pubsub_broker, **kwargs):
        """
        Initialize the scene.
        :param game_window: the main game window
        :param renderer: rendering backend
        :param views: camera/views configuration
        :param event_bus: event bus for communication
        :param kwargs: any extra parameters
        """
        self.scene_graph = SceneGraph()

    def load_scene_graph(self, path: str | None = None):
        """Load a scene graph from a file."""
        pass

    @abstractmethod
    def on_load(self):
        """Load assets & prepare data (called once)."""
        pass

    @abstractmethod
    def on_enter(self):
        """Called when this scene becomes active."""
        pass

    @abstractmethod
    def update(self, dt, shared_state, player_connection=None):
        """Advance logic; dt is time since last frame."""
        pass

    @abstractmethod
    def render(self, renderer, shared_state, player_connection=None):
        """Draw the current frame."""
        pass

    @abstractmethod
    def on_exit(self):
        """Called when the scene is popped or switched."""
        pass

    @abstractmethod
    def on_unload(self):
        """Free resources (called once)."""
        pass


# Alias for backward compatibility
class Scene(BaseScene):
    def on_load(self):
        pass

    def on_enter(self):
        pass

    def update(self, dt, shared_state):
        pass

    def render(self, renderer, shared_state):
        pass

    def on_exit(self):
        pass

    def on_unload(self):
        pass


class SceneManager:
    """Manages scene registration and lifecycle transitions."""

    def __init__(self):
        self.scenes: dict[str, BaseScene] = {}
        self.current: BaseScene | None = None

    def register(self, name: str, scene: BaseScene) -> BaseScene:
        """Register a scene instance under a name."""
        if name in self.scenes:
            return self.scenes[name]

        if not isinstance(scene, BaseScene):
            raise TypeError(f"{scene} must inherit from BaseScene")

        self.scenes[name] = scene

        return scene

    def get_scene(self, scene_name: str) -> BaseScene:
        return self.scenes[scene_name]

    def change(self, name):
        """Switch to a registered scene by name."""
        if self.current:
            self.current.on_exit()
        self.current = self.scenes.get(name)
        if self.current:
            # TODO: Reload all Scene Python files here.
            self.current.load_scene_graph()
            self.current.on_load()
            self.current.on_enter()

    def update(self, dt, shared_state, player_connection=None):
        """Update the current scene; handle automatic transitions."""
        if not self.current:
            return
        next_scene = self.current.update(dt, shared_state, player_connection)
        if isinstance(next_scene, str):
            self.change(next_scene)

    def render(self, renderer, shared_state, player_connection=None):
        """Render the current scene."""
        if self.current:
            self.current.render(renderer, shared_state, player_connection)


# Provide a global manager instance for convenience
scene_manager = SceneManager()
