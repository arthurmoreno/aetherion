"""Parity gate for the C++ dimetric tile walker migration.

The Python and C++ implementations of `Camera.draw_player_perspective_layer`
must produce identical handler-dispatch sequences for a fixed scene.
Any diff blocks the migration phase.

Plan: `.claude/docs/epics-plans/2026-05-11-dimetric-tile-walker-cpp-migration.md`.
"""

from __future__ import annotations

from unittest.mock import patch

import pytest

pytest.importorskip("aetherion._aetherion")

import aetherion  # noqa: E402
from aetherion import DirectionEnum, EntityInterface, Position, WorldView  # noqa: E402
from aetherion.camera.dimetric import Camera  # noqa: E402
from aetherion.camera.models import CameraSettings  # noqa: E402
from aetherion.events import PubSubTopicBroker  # noqa: E402


class _FakeView:
    """Minimal view-object that the handlers and walker poke into.

    Provides the `.sprite.sprite_id` attribute chain the entity handlers
    read, plus the `set_direction` / `set_raspberry_load_sprite` methods
    they may call. Returned for every classification lookup.
    """

    sprite = type("S", (), {"sprite_id": "x"})()

    def set_direction(self, *_):
        pass

    def set_raspberry_load_sprite(self, *_):
        pass


class _AnyDict(dict):
    """`dict` subclass whose `.get(...)` returns a `_FakeView` for any
    key. Lets us bypass `Classification` identity matching — both walkers
    look up by classification, both find the same fake view, so dispatch
    proceeds in both paths and the parity comparison is meaningful."""

    def get(self, _key, _default=None):
        return _FakeView()


class _EntityRecorder:
    """Records every call summary; returns `(0, 0, None)` because the
    beast/plant handler contract returns `(_screen_x, _screen_y,
    entity_hovered)`."""

    def __init__(self):
        self.calls: list[tuple] = []

    def __call__(self, _cm, _cs, _wv, entity, _view, sx, sy, layer, *_):
        eid = entity.get_entity_id() if entity is not None else None
        self.calls.append((eid, sx, sy, layer))
        return 0, 0, None


class _TerrainRecorder:
    """Same idea but the terrain handler returns a single
    `EntityInterface | None`."""

    def __init__(self):
        self.calls: list[tuple] = []

    def __call__(self, _cm, _cs, terrain, _view, _sel, _wv, _ms, sx, sy, layer, *_):
        tid = terrain.get_entity_id() if terrain is not None else None
        self.calls.append((tid, sx, sy, layer))
        return None


class _StubRenderQueue:
    """No-op render queue. The Camera constructor builds a real
    `aetherion.RenderQueue(font_path=...)` which fails in headless CI
    because the bundled font isn't on disk in site-packages; patching
    the class with this stub sidesteps that. We don't compare queue
    contents in this test — handler-dispatch parity is enough."""

    def __init__(self, *_, **__):
        pass

    def __getattr__(self, _name):
        return lambda *_, **__: None


def _make_world() -> tuple[WorldView, int]:
    world = WorldView()
    world.voxelGridView.initVoxelGridView(8, 8, 3, 0, 0, 0)
    beast = EntityInterface()
    beast.set_entity_id(101)
    p = Position()
    p.x, p.y, p.z = 3, 3, 1
    p.direction = DirectionEnum.DOWN
    beast.set_position(p)
    world.addEntity(101, beast)
    world.voxelGridView.set_entity(3, 3, 1, 101)
    return world, 101


def _make_camera(beast_h, plant_h, terrain_h) -> Camera:
    settings = CameraSettings(
        blocks_in_screen={"width": 5, "height": 5, "depth": 3},
        player_block_position={"x": 2, "y": 2},
        layers_to_draw=3,
        layers_bellow_player=1,
        sprite_size=16,
        sprite_scale=1,
        tile_size_on_screen=16,
        right_offset=0,
        up_offset=0,
        game_screen_width=256,
        game_screen_height=256,
        camera_screen_width_adjust_offset=0,
        camera_screen_height_adjust_offset=0,
        empty_tile_debugging=False,
        camera_iterate_right_to_left=False,
        camera_iterate_bottom_to_top=False,
    )
    views = {"entities": _AnyDict(), "terrains": {}, "camera-ui": {}}

    class _StubRenderer:
        _renderer = None
        renderer_ptr = None

    with patch.object(aetherion, "RenderQueue", _StubRenderQueue):
        return Camera(
            renderer=_StubRenderer(),
            views=views,
            settings=settings,
            pubsub_broker=PubSubTopicBroker(),
            beast_entity_handler=beast_h,
            plant_entity_handler=plant_h,
            terrain_handler=terrain_h,
        )


def _run(camera: Camera, world: WorldView):
    return camera.draw_player_perspective_layer(
        world,
        1,  # z
        (0, 0),  # top_left
        5,
        5,  # blocks_width, blocks_height
        0,
        0,  # screen_x_offset, screen_y_offset
        None,  # entity_hovered
        None,  # selected_entity
        0,  # layer_index
        {"x": -1, "y": -1, "left": False, "right": False},  # mouse_state
        None,  # player
        1.0,  # sun_light
        False,
        False,
        False,
        False,
    )


def _capture(use_cpp: bool, world: WorldView):
    beast = _EntityRecorder()
    plant = _EntityRecorder()
    terrain = _TerrainRecorder()
    camera = _make_camera(beast, plant, terrain)
    camera.use_cpp_walker = use_cpp
    result = _run(camera, world)
    return result, beast.calls, plant.calls, terrain.calls


def test_python_and_cpp_walkers_match():
    """Run the same scene through both paths; the return value and
    per-handler call lists must be identical."""
    world, _ = _make_world()

    py_result, py_beast, py_plant, py_terrain = _capture(use_cpp=False, world=world)
    cpp_result, cpp_beast, cpp_plant, cpp_terrain = _capture(use_cpp=True, world=world)

    assert cpp_beast == py_beast
    assert cpp_plant == py_plant
    assert cpp_terrain == py_terrain
    assert cpp_result == py_result
