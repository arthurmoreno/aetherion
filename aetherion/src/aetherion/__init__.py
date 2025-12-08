# aetherion/__init__.py

# Import everything from the extension module (the shared library)
# This assumes the shared library is named "lifesimcore.cpython-312-x86_64-linux-gnu.so"
# but its module name is resolved as "lifesimcore".
from aetherion._aetherion import *  # noqa

from aetherion.entities import *  # noqa: F403
from aetherion.windowing.window import *  # noqa: F403
from aetherion.renderer import *  # noqa: F403
from aetherion.scenes import *  # noqa: F403
from aetherion.events import *  # noqa: F403
from aetherion.events.action_event import *  # noqa: F403
from aetherion.constants import *  # noqa: F403
from aetherion.world import *  # noqa: F403
from aetherion.world.interface import *  # noqa: F403
from aetherion.networking import *  # noqa: F403
from aetherion.game_state.connections import *  # noqa: F403
from aetherion.game_state.state import *  # noqa: F403
from aetherion.game_state.world_interface import *  # noqa: F403
from aetherion.input.action_processor import *  # noqa: F403
from aetherion.input.event_action_handler import *  # noqa: F403
from aetherion.resource_manager import *  # noqa: F403

from aetherion.scheduler import *  # noqa: F403
from aetherion.paths import *  # noqa: F403
