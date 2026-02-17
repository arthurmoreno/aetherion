import sys
from pathlib import Path

from aetherion.utils import is_running_pyinstaller_bundle


def get_project_root() -> Path:
    """
    Returns the root directory of the project (where the entry point run.py is located).

    This finds the actual game root by looking at sys.argv[0] (the entry point script).
    """
    if is_running_pyinstaller_bundle():
        # In PyInstaller bundle, we map res:// to the _internal directory or where resources are
        # Based on views.py: base_resource_path = "./_internal/resources"
        # We assume res:// maps to the folder containing _internal or the root of extraction
        # If views.py uses ./_internal, it means relative to CWD.
        # But usually we want absolute paths.
        if hasattr(sys, "_MEIPASS"):
            return Path(sys._MEIPASS)
        return Path(".")
    else:
        # Find the entry point script (run.py) from sys.argv[0]
        # This gives us the actual game root directory
        entry_point = Path(sys.argv[0]).resolve()

        # entry_point is run.py, so its parent is the game root
        if entry_point.name == "run.py":
            return entry_point.parent

        # Fallback: if we can't determine from entry point,
        # assume lifesim is a module and return its parent
        return Path(__file__).resolve().parent.parent


def get_user_data_dir() -> Path:
    """
    Returns the user data directory.
    """
    return get_project_root() / ".aetherion"


def get_core_data_dir() -> Path:
    """
    Returns the core engine data directory.
    """
    return get_project_root() / "aetherion" / "data"


def resolve_path(virtual_path: str) -> Path:
    """
    Resolves a virtual path (e.g., res://...) to an absolute system path.

    Args:
        virtual_path: The virtual path string (e.g., "res://sprites/player.png")

    Returns:
        Path: The resolved absolute path.

    Raises:
        ValueError: If the protocol is not supported.
    """
    if virtual_path.startswith("res://"):
        return get_project_root() / virtual_path.replace("res://", "")
    elif virtual_path.startswith("user://"):
        return get_user_data_dir() / virtual_path.replace("user://", "")
    elif virtual_path.startswith("core://"):
        return get_core_data_dir() / virtual_path.replace("core://", "")
    else:
        # Fallback for non-virtual paths (e.g. absolute paths or relative to CWD)
        # Ideally we should warn or forbid this, but for transition we allow it.
        return Path(virtual_path)
