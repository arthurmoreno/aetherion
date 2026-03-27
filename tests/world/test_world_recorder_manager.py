from __future__ import annotations

from pathlib import Path

import pytest

from aetherion.world import recorder as recorder_module
from aetherion.world.recorder import WorldRecorderManager


class _FakeRecorder:
    def __init__(self, recording_config=None):
        self.recording_config = recording_config or {}
        self.is_recording = False
        self.loaded_file = None
        self.saved_file = None
        self.snapshots = []

    def start_recording(self):
        self.is_recording = True

    def stop_recording(self):
        self.is_recording = False

    def get_snapshot_count(self):
        return len(self.snapshots)

    def save_to_file(self, filepath: str):
        self.saved_file = filepath
        Path(filepath).write_bytes(b"ok")

    def load_from_file(self, filepath: str):
        self.loaded_file = filepath


def test_start_recording_generates_name_and_prevents_parallel(monkeypatch, tmp_path):
    monkeypatch.setattr(recorder_module, "WorldRecorder", _FakeRecorder)
    manager = WorldRecorderManager(recordings_dir=str(tmp_path))

    name = manager.start_recording()
    assert name.startswith("recording_")
    assert manager.is_recording() is True

    with pytest.raises(RuntimeError, match="already in progress"):
        manager.start_recording(name="another")


def test_stop_recording_returns_none_when_no_active_recorder(tmp_path):
    manager = WorldRecorderManager(recordings_dir=str(tmp_path))
    assert manager.stop_recording() is None


def test_save_recording_raises_without_active_recorder(tmp_path):
    manager = WorldRecorderManager(recordings_dir=str(tmp_path))
    with pytest.raises(RuntimeError, match="No active recorder"):
        manager.save_recording()


def test_save_recording_uses_default_path(monkeypatch, tmp_path):
    monkeypatch.setattr(recorder_module, "WorldRecorder", _FakeRecorder)
    manager = WorldRecorderManager(recordings_dir=str(tmp_path))
    manager.start_recording(name="session_1")
    manager.stop_recording()

    saved = manager.save_recording()
    assert saved.endswith("session_1.pkl")
    assert Path(saved).exists()


def test_load_recording_and_delete_recording(monkeypatch, tmp_path):
    monkeypatch.setattr(recorder_module, "WorldRecorder", _FakeRecorder)
    manager = WorldRecorderManager(recordings_dir=str(tmp_path))

    path = tmp_path / "run_1.pkl"
    path.write_bytes(b"binary")

    manager.load_recording("run_1")
    assert manager.get_current_recording_name() == "run_1"
    assert manager.get_active_recorder() is not None

    assert manager.delete_recording("run_1") is True
    assert manager.delete_recording("run_1") is False
