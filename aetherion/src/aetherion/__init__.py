# lifesimcore/__init__.py

# Import everything from the extension module (the shared library)
# This assumes the shared library is named "lifesimcore.cpython-312-x86_64-linux-gnu.so"
# but its module name is resolved as "lifesimcore".
from aetherion._aetherion import *  # noqa

from aetherion.entities import *
from aetherion.windowing.window import *
