from __future__ import annotations

import sys
from pathlib import Path

from aetherion import paths


def test_resolve_path_supports_res_user_core_protocols(monkeypatch):
    monkeypatch.setattr(paths, "get_project_root", lambda *_: Path("/tmp/project"))
    monkeypatch.setattr(paths, "get_user_data_dir", lambda *_: Path("/tmp/project/.aetherion"))
    monkeypatch.setattr(paths, "get_core_data_dir", lambda *_: Path("/tmp/project/aetherion/data"))

    assert paths.resolve_path("res://assets/sprite.png") == Path("/tmp/project/assets/sprite.png")
    assert paths.resolve_path("user://save.sav") == Path("/tmp/project/.aetherion/save.sav")
    assert paths.resolve_path("core://items.json") == Path("/tmp/project/aetherion/data/items.json")


def test_resolve_path_non_virtual_returns_plain_path():
    assert paths.resolve_path("plain/relative.txt") == Path("plain/relative.txt")


def test_get_project_root_from_entrypoint(monkeypatch):
    monkeypatch.setattr(paths, "is_running_pyinstaller_bundle", lambda: False)
    monkeypatch.setattr(sys, "argv", ["/opt/game/run.py"])

    assert paths.get_project_root() == Path("/opt/game")


def test_get_project_root_pyinstaller_meipass(monkeypatch):
    monkeypatch.setattr(paths, "is_running_pyinstaller_bundle", lambda: True)
    monkeypatch.setattr(sys, "_MEIPASS", "/tmp/meipass", raising=False)

    assert paths.get_project_root() == Path("/tmp/meipass")
