from __future__ import annotations

import queue
from types import SimpleNamespace

import msgpack
import pytest

from aetherion.networking.connection import ServerBeastConnection
from aetherion.networking.exceptions import AuthenticationError
from aetherion.world.constants import WorldInstanceTypes


def _make_connection() -> ServerBeastConnection:
    conn = ServerBeastConnection(name="player-1")
    conn.websocket = SimpleNamespace(sent=[], send=lambda payload: conn.websocket.sent.append(payload))
    # Keep tests independent from JWT backend availability in env.
    conn.jwt_provider = SimpleNamespace(get_user_token=lambda: "test-token")
    conn.connected = True
    return conn


def test_connect_rejects_non_server_connection_type():
    conn = ServerBeastConnection(name="p")
    with pytest.raises(ValueError, match="only supports SERVER"):
        conn.connect(WorldInstanceTypes.SYNCHRONOUS)


def test_server_online_returns_false_without_socket():
    conn = ServerBeastConnection(name="p")
    conn.websocket = None
    conn._server_online = False
    assert conn.server_online is False


def test_server_online_sets_flag_on_pong():
    conn = _make_connection()
    conn._server_online = False
    conn.control_queue.put({"type": "pong", "world_ready": True})

    assert conn.server_online is True
    assert conn._server_online is True


def test_server_online_raises_authentication_error():
    conn = _make_connection()
    conn._server_online = False
    conn.control_queue.put({"type": "error", "error_type": "authentication_error", "message": "bad token"})

    with pytest.raises(AuthenticationError):
        _ = conn.server_online


def test_subscribe_entities_noops_when_disconnected():
    conn = ServerBeastConnection(name="p")
    conn.websocket = None
    conn.connected = False
    conn.subscribe_to_entities([1, 2, 3])  # no raise


def test_subscribe_entities_sends_message_when_connected():
    conn = _make_connection()
    conn.control_queue.put({"type": "subscribe_entities_ack"})
    conn.subscribe_to_entities([7, 9])

    assert len(conn.websocket.sent) == 1
    payload = msgpack.unpackb(conn.websocket.sent[0], raw=False)
    assert payload["type"] == "subscribe_entities"
    assert payload["entities"] == [7, 9]


def test_enable_state_stream_and_set_stream_rate_guards():
    conn = _make_connection()
    conn.enable_state_stream(fps=30)
    assert conn.streaming_enabled is True
    first_payload = msgpack.unpackb(conn.websocket.sent[-1], raw=False)
    assert first_payload["type"] == "subscribe_state"
    assert first_payload["fps"] == 30.0

    sent_before = len(conn.websocket.sent)
    conn.enable_state_stream(fps=20)
    assert len(conn.websocket.sent) == sent_before

    conn.set_stream_rate(fps=10, interval_ms=5)
    payload = msgpack.unpackb(conn.websocket.sent[-1], raw=False)
    assert payload["type"] == "set_stream_rate"
    assert payload["fps"] == 10.0
    assert payload["interval_ms"] == 5.0


def test_wait_for_next_world_state_requests_when_not_streaming(monkeypatch):
    conn = _make_connection()
    conn.streaming_enabled = False
    called = {"request": 0}
    monkeypatch.setattr(conn, "request_world_state", lambda: called.__setitem__("request", 1))

    result = conn.wait_for_next_world_state()

    assert result is None
    assert called["request"] == 1


def test_wait_for_next_world_state_returns_last_on_decode_error(monkeypatch):
    conn = _make_connection()
    conn.state_queue = queue.Queue(maxsize=1)
    conn.state_queue.put(b"corrupt")
    conn.last_state_response = "previous"
    monkeypatch.setattr(
        "aetherion.networking.connection.lz4.frame.decompress", lambda _buf: (_ for _ in ()).throw(ValueError)
    )

    result = conn.wait_for_next_world_state()
    assert result == "previous"
