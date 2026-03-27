from __future__ import annotations

import queue
import time

import msgpack

from aetherion.networking.receiver import WebSocketReceiver


class _FakeWebSocket:
    def __init__(self, frames):
        self.frames = list(frames)

    def recv(self, timeout=1.0):
        if self.frames:
            return self.frames.pop(0)
        raise TimeoutError()


def test_receiver_routes_control_error_and_latest_state():
    control_msg = msgpack.packb({"type": "subscribed", "status": "ok"}, use_bin_type=True)
    error_msg = msgpack.packb({"type": "error", "message": "boom"}, use_bin_type=True)
    state_old = b"state-old"
    state_new = b"state-new"

    ws = _FakeWebSocket([control_msg, error_msg, state_old, state_new])
    state_queue: queue.Queue[bytes] = queue.Queue(maxsize=2)
    control_queue: queue.Queue[dict[str, str]] = queue.Queue(maxsize=2)
    error_queue: queue.Queue[dict[str, str]] = queue.Queue(maxsize=2)

    receiver = WebSocketReceiver(ws, state_queue, control_queue, error_queue)
    receiver.start()
    time.sleep(0.05)
    receiver.stop(join=True, timeout=1.0)

    assert receiver.is_alive() is False
    assert control_queue.get_nowait()["type"] == "subscribed"
    assert error_queue.get_nowait()["type"] == "error"
    assert state_queue.get_nowait() == state_new


def test_receiver_start_is_idempotent_and_stop_joins_cleanly():
    ws = _FakeWebSocket([])
    state_queue: queue.Queue[bytes] = queue.Queue(maxsize=1)
    control_queue: queue.Queue[dict[str, str]] = queue.Queue(maxsize=1)
    error_queue: queue.Queue[dict[str, str]] = queue.Queue(maxsize=1)

    receiver = WebSocketReceiver(ws, state_queue, control_queue, error_queue)
    receiver.start()
    first_thread = receiver._thread
    receiver.start()
    second_thread = receiver._thread

    receiver.stop(join=True, timeout=1.0)

    assert first_thread is second_thread
    assert receiver.is_alive() is False
