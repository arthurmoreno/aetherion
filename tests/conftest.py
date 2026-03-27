from __future__ import annotations

from dataclasses import dataclass, field
from types import SimpleNamespace
from typing import Any


def make_event(data: dict[str, Any]) -> SimpleNamespace:
    return SimpleNamespace(data=data)


class FakeEventBus:
    def __init__(self) -> None:
        self.subscriptions: list[tuple[object, object]] = []
        self.emitted: list[dict[str, Any]] = []

    def subscribe(self, event_type: object, handler: object) -> None:
        self.subscriptions.append((event_type, handler))

    def emit(self, event_type: object, data: dict[str, Any], source: str) -> None:
        self.emitted.append({"event_type": event_type, "data": data, "source": source})


@dataclass
class FakeMetadata:
    key: str
    name: str
    description: str = ""
    type: Any = None
    status: str = "ready"
    host: str | None = None
    port: str | None = None
    gravity: float = 5.0
    friction: float = 1.0
    allow_multi_direction: bool = True
    evaporation_coefficient: float = 8.0
    heat_to_water_evaporation: float = 120.0
    water_minimum_units: int = 30000
    metabolism_cost_to_apply_force: float = 1.9999999949504854e-06


@dataclass
class FakeEntity:
    entity_id: int = 7

    def get_entity_id(self) -> int:
        return self.entity_id


@dataclass
class FakeWorldBridge:
    update_calls: int = 0
    move_calls: list[tuple[int, list[Any]]] = field(default_factory=list)
    take_calls: list[tuple[int, int, int]] = field(default_factory=list)
    use_calls: list[tuple[int, int, int, int]] = field(default_factory=list)
    debug_calls: list[int] = field(default_factory=list)

    def update_world(self) -> None:
        self.update_calls += 1

    def dispatch_move_entity_event_by_id(self, entity_id: int, directions: list[Any]) -> None:
        self.move_calls.append((entity_id, directions))

    def dispatch_take_item_event_by_id(self, entity_id: int, hovered_entity: int, selected_entity: int) -> None:
        self.take_calls.append((entity_id, hovered_entity, selected_entity))

    def dispatch_use_item_event_by_id(
        self, entity_id: int, item_slot: int, hovered_entity: int, selected_entity: int
    ) -> None:
        self.use_calls.append((entity_id, item_slot, hovered_entity, selected_entity))

    def dispatch_set_entity_to_debug(self, entity_id: int) -> None:
        self.debug_calls.append(entity_id)
