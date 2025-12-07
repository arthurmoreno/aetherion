from __future__ import annotations

import queue
import threading
from queue import Queue
from typing import Any

import msgpack
from websockets.sync.client import ClientConnection


class WebSocketReceiver:
    """Owns the single recv() loop and fans out messages to queues.

    - Control-plane messages (msgpack dict with a 'type') go to control/error queues.
    - Binary state buffers are pushed to state_queue, keeping only the latest.
    """

    def __init__(
        self,
        websocket: ClientConnection,
        state_queue: Queue[bytes],
        control_queue: Queue[dict[str, str]],
        error_queue: Queue[dict[str, str]],
        name: str = "ws-recv",
    ) -> None:
        self.websocket: ClientConnection = websocket
        self.state_queue: Queue[bytes] = state_queue
        self.control_queue: Queue[dict[str, str]] = control_queue
        self.error_queue: Queue[dict[str, str]] = error_queue
        self._stop_evt: threading.Event = threading.Event()
        self._thread: threading.Thread | None = None
        self._thread_name: str = name

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._stop_evt.clear()

        def _recv_loop() -> None:
            while not self._stop_evt.is_set():
                try:
                    data = self.websocket.recv(timeout=1.0)
                except TimeoutError:
                    continue
                except Exception:
                    break
                if data is None:
                    continue
                # Control plane (msgpack dict)
                try:
                    obj: dict[Any, Any] = msgpack.unpackb(data, raw=False, strict_map_key=False)
                    if isinstance(obj, dict) and obj.get("type"):
                        if obj.get("type") == "error":
                            self._put_latest(self.error_queue, obj)
                        else:
                            self._put_latest(self.control_queue, obj)
                        continue
                except Exception:
                    # Not a control-plane dict; treat as state buffer
                    pass

                # Assume compressed state buffer; keep only the latest
                self._drain_queue(self.state_queue)
                self._put_latest(self.state_queue, data)

        self._thread = threading.Thread(target=_recv_loop, name=self._thread_name, daemon=True)
        self._thread.start()

    def stop(self, join: bool = False, timeout: float | None = None) -> None:
        self._stop_evt.set()
        if join and self._thread is not None:
            self._thread.join(timeout=timeout)

    def is_alive(self) -> bool:
        return bool(self._thread and self._thread.is_alive())

    @staticmethod
    def _put_latest(q: queue.Queue[bytes] | queue.Queue[dict[str, str]], item: bytes | dict[str, str]) -> None:
        try:
            q.put_nowait(item)
        except queue.Full:
            try:
                _ = q.get_nowait()
            except queue.Empty:
                pass
            q.put_nowait(item)

    @staticmethod
    def _drain_queue(q: queue.Queue[bytes] | queue.Queue[dict[str, str]]) -> None:
        try:
            while True:
                _ = q.get_nowait()
        except queue.Empty:
            return
