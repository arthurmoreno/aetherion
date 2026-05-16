# aetherion/__init__.py

import atexit
import gc

# Import everything from the extension module (the shared library)
# This assumes the shared library is named "lifesimcore.cpython-312-x86_64-linux-gnu.so"
# but its module name is resolved as "lifesimcore".
from aetherion._aetherion import *  # noqa


def _release_all_worlds_at_exit() -> None:
    """Break Python<->C++ cycles on every live World before Py_Finalize.

    Without this, any World that holds Python event handlers, systems, or
    scripts keeps nanobind wrappers alive through interpreter shutdown,
    producing 'leaked N instances/types/functions' diagnostics and an
    SIGABRT. Registered here (not only in the test conftest) so game
    processes benefit automatically.
    """
    try:
        from aetherion._aetherion import World  # noqa: PLC0415
    except Exception:
        return
    for obj in gc.get_objects():
        if isinstance(obj, World):
            try:
                obj.release_python_state()
            except Exception:
                pass


atexit.register(_release_all_worlds_at_exit)

# isort: off
# engine.game imports BasicGameWindow from aetherion, which is only in scope
# after windowing.window is imported. This block must stay in dependency order.
from aetherion.entities import *  # noqa: F403, E402
from aetherion.windowing.window import *  # noqa: F403, E402
from aetherion.renderer import *  # noqa: F403, E402
from aetherion.scenes import *  # noqa: F403, E402
from aetherion.events import *  # noqa: F403, E402
from aetherion.events.action_event import *  # noqa: F403, E402
from aetherion.constants import *  # noqa: F403, E402
from aetherion.world import *  # noqa: F403, E402
from aetherion.world.interface import *  # noqa: F403, E402
from aetherion.networking import *  # noqa: F403, E402
from aetherion.game_state.connections import *  # noqa: F403, E402
from aetherion.game_state.state import *  # noqa: F403, E402
from aetherion.game_state.world_interface import *  # noqa: F403, E402
from aetherion.input.action_processor import *  # noqa: F403, E402
from aetherion.input.event_action_handler import *  # noqa: F403, E402
from aetherion.resource_manager import *  # noqa: F403, E402
from aetherion.engine.game import *  # noqa: F403, E402
from aetherion.scheduler import *  # noqa: F403, E402
from aetherion.paths import *  # noqa: F403, E402
# isort: on
