import sys


def is_running_pyinstaller_bundle() -> bool:
    """
    Check if the script is running as a PyInstaller bundle.
    """
    return getattr(sys, "frozen", False) and hasattr(sys, "_MEIPASS")
