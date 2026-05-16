"""Parity gate for `EntityInterface::serialize()` / `deserialize()`.

Locks the wire format of the entity serializer before the POD-memcpy
fast path lands. The test builds entity fixtures covering every
component type in `ComponentTypes`, plus the empty-mask and
all-components-set extremes, and asserts:

  1. `serialize()` is deterministic — two calls on the same entity
     produce the same bytes (catches uninitialised-memory hazards in
     a future `CreateUninitializedVector` arena-write path).
  2. `serialize → deserialize → serialize` is idempotent — round-tripping
     the bytes through the decoder and re-encoding produces the same
     bytes (catches encoder/decoder asymmetry: any future change to the
     encoder for a given component type must be matched by the same
     change to the decoder, otherwise this test fails).
  3. Specific POD field values survive the round-trip on a handful of
     well-known scalar components (entity id, position, velocity,
     health) — guards against off-by-one or wrong-offset bugs that
     could leave bytes-equal but field-equal-via-coincidence.

The non-trivially-copyable components (`Inventory`, `ConsoleLogsComponent`,
`TileEffectsList`, `ParentsComponent`) are exercised via their public
Python APIs (`add_item`, `add_effect`, etc.) so the test stays
representation-agnostic — the encoder is free to use the
`struct_pack` fallback path for these types as long as the round-trip
is faithful.
"""

from __future__ import annotations

import pytest

from aetherion import (
    ConsoleLogsComponent,
    DirectionEnum,
    EntityInterface,
    EntityTypeComponent,
    FoodItem,
    HealthComponent,
    Inventory,
    ItemEnum,
    ItemTypeComponent,
    MatterContainer,
    MetabolismComponent,
    MovingComponent,
    ParentsComponent,
    PerceptionComponent,
    PerceptionResponse,
    PerceptionResponseFlatB,
    PhysicsStats,
    Position,
    TileEffectComponent,
    TileEffectsList,
    Velocity,
    WorldView,
)

# ---------------------------------------------------------------------------
# Component fixture builders — one per component type, each mutating a fresh
# EntityInterface so the resulting mask has exactly one bit set.
# ---------------------------------------------------------------------------


def _build_entity_type(e: EntityInterface) -> None:
    c = EntityTypeComponent()
    c.main_type = 2
    c.sub_type0 = 5
    c.sub_type1 = 7
    e.set_entity_type(c)


def _build_physics_stats(e: EntityInterface) -> None:
    c = PhysicsStats()
    c.mass = 12.5
    c.max_speed = 3.0
    c.min_speed = 0.25
    c.force_x = -1.5
    c.force_y = 2.0
    c.force_z = 0.0
    e.set_physics_stats(c)


def _build_position(e: EntityInterface) -> None:
    c = Position()
    c.x = 10
    c.y = 20
    c.z = 30
    c.direction = DirectionEnum.DOWN
    e.set_position(c)


def _build_velocity(e: EntityInterface) -> None:
    c = Velocity()
    c.vx = 1.5
    c.vy = -2.25
    c.vz = 0.5
    e.set_velocity(c)


def _build_moving_component(e: EntityInterface) -> None:
    c = MovingComponent()
    c.is_moving = True
    c.moving_from_x = 1
    c.moving_from_y = 2
    c.moving_from_z = 3
    c.moving_to_x = 4
    c.moving_to_y = 5
    c.moving_to_z = 6
    c.vx = 0.5
    c.vy = -0.5
    c.vz = 0.0
    c.will_stop_x = False
    c.will_stop_y = True
    c.will_stop_z = False
    c.completion_time = 100
    c.time_remaining = 42
    c.direction = DirectionEnum.RIGHT
    e.set_moving_component(c)


def _build_health(e: EntityInterface) -> None:
    c = HealthComponent()
    c.health_level = 75.5
    c.max_health = 100.0
    e.set_health(c)


def _build_perception(e: EntityInterface) -> None:
    c = PerceptionComponent()
    c.perception_area = 20
    c.z_perception_area = 10
    e.set_perception(c)


def _build_inventory(e: EntityInterface) -> None:
    c = Inventory()
    # `resize(n)` is rejected unless `n <= max_items`, so cap must go
    # first. `add_item` then needs at least one `-1` slot to land in,
    # which `resize` populates.
    c.max_items = 8
    c.resize(8)
    c.add_item(11)
    c.add_item(22)
    c.add_item(33)
    e.set_inventory(c)


def _build_console_logs(e: EntityInterface) -> None:
    c = ConsoleLogsComponent()
    c.max_size = 64
    # `add_log` keys by current time, which is non-deterministic across
    # test runs. The default-constructed component (empty buffer) is
    # enough to exercise the non-trivial serialize fallback path.
    e.set_console_logs(c)


def _build_matter_container(e: EntityInterface) -> None:
    c = MatterContainer()
    c.terrain_matter = 1
    c.water_vapor = 2
    c.water_matter = 3
    c.bio_mass_matter = 4
    e.set_matter_container(c)


def _build_item_enum(e: EntityInterface) -> None:
    e.set_item_enum(ItemEnum.WEAPON)


def _build_food_item(e: EntityInterface) -> None:
    c = FoodItem()
    c.energy_density = 1.25
    c.mass = 0.5
    c.volume = 0.75
    c.convertion_efficiency = 0.9
    c.energy_health_ratio = 1.1
    e.set_food_item(c)


def _build_parents(e: EntityInterface) -> None:
    c = ParentsComponent()
    c.parents = [101, 202, 303]
    e.set_parents(c)


def _build_item_type_comp(e: EntityInterface) -> None:
    c = ItemTypeComponent()
    c.main_type = 3
    c.sub_type0 = 1
    c.sub_type1 = 2
    e.set_item_type_comp(c)


def _build_tile_effect_comp(e: EntityInterface) -> None:
    c = TileEffectComponent()
    c.tile_effect_type = 1
    c.damage_value = 5.5
    c.effect_total_time = 60
    c.effect_remaining_time = 30
    e.set_tile_effect_comp(c)


def _build_tile_effects_list(e: EntityInterface) -> None:
    c = TileEffectsList()
    # Single element only. `TileEffectsList` carries an
    # `std::unordered_set<int>` alongside the vector; `struct_pack`
    # serializes that set as a sequence in whatever iteration order the
    # rebuilt set happens to have after `deserialize`. With ≥2 elements
    # the round-trip wire format becomes non-deterministic
    # (re-encoding the deserialized entity produces different bytes
    # depending on hash-table layout). One element keeps the gate
    # exercising the `struct_pack` fallback path without depending on
    # libstdc++'s unordered-container internals.
    c.add_effect(7)
    e.set_tile_effects_list(c)


def _build_metabolism(e: EntityInterface) -> None:
    c = MetabolismComponent()
    c.energy_reserve = 50.0
    c.max_energy_reserve = 100.0
    e.set_metabolism(c)


# Order chosen to match `ComponentTypes` in `src/EntityInterface.hpp` —
# the encoder walks the tuple in declaration order, so the serialized
# layout follows this same order for every fixture.
SINGLE_COMPONENT_BUILDERS: dict[str, callable] = {
    "entity_type": _build_entity_type,
    "physics_stats": _build_physics_stats,
    "position": _build_position,
    "velocity": _build_velocity,
    "moving_component": _build_moving_component,
    "health": _build_health,
    "perception": _build_perception,
    "inventory": _build_inventory,
    "console_logs": _build_console_logs,
    "matter_container": _build_matter_container,
    "item_enum": _build_item_enum,
    "food_item": _build_food_item,
    "parents": _build_parents,
    "item_type_comp": _build_item_type_comp,
    "tile_effect_comp": _build_tile_effect_comp,
    "tile_effects_list": _build_tile_effects_list,
    "metabolism": _build_metabolism,
}


# The 13 component types `is_trivially_copyable_v<C>` is true for — every
# component except the three with `std::vector` / `std::map` /
# `std::unordered_set` members. The POD-only fixture is the one Step C's
# `computeSerializedSize` + `CreateUninitializedVector` arena write hits
# its exact-size-zero-realloc path on.
POD_COMPONENT_KEYS = (
    "entity_type",
    "physics_stats",
    "position",
    "velocity",
    "moving_component",
    "health",
    "perception",
    "matter_container",
    "item_enum",
    "food_item",
    "item_type_comp",
    "tile_effect_comp",
    "metabolism",
)

NON_POD_COMPONENT_KEYS = (
    "inventory",
    "console_logs",
    "parents",
    "tile_effects_list",
)


def _make_entity(builder_keys: tuple[str, ...]) -> EntityInterface:
    e = EntityInterface()
    e.set_entity_id(42)
    for key in builder_keys:
        SINGLE_COMPONENT_BUILDERS[key](e)
    return e


# ---------------------------------------------------------------------------
# Round-trip + idempotency: the gate.
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "component_key",
    list(SINGLE_COMPONENT_BUILDERS.keys()),
)
def test_serialize_round_trip_single_component(component_key: str) -> None:
    """Every component, on its own, must round-trip cleanly."""
    e = _make_entity((component_key,))
    blob = e.serialize()

    # 1. Encoder is deterministic.
    assert e.serialize() == blob, f"serialize() is non-deterministic for component={component_key}"

    # 2. Round-trip: bytes survive deserialize + re-encode.
    decoded = EntityInterface.deserialize(blob)
    assert decoded.serialize() == blob, f"serialize → deserialize → serialize drifted for component={component_key}"

    # 3. Entity id round-trips.
    assert decoded.get_entity_id() == 42


def test_serialize_round_trip_empty_mask() -> None:
    """Header-only entity (no components) must round-trip."""
    e = EntityInterface()
    e.set_entity_id(7)
    blob = e.serialize()

    assert e.serialize() == blob
    decoded = EntityInterface.deserialize(blob)
    assert decoded.serialize() == blob
    assert decoded.get_entity_id() == 7


def test_serialize_round_trip_all_components() -> None:
    """All 17 components set simultaneously must round-trip."""
    e = _make_entity(tuple(SINGLE_COMPONENT_BUILDERS.keys()))
    blob = e.serialize()

    assert e.serialize() == blob
    decoded = EntityInterface.deserialize(blob)
    assert decoded.serialize() == blob
    assert decoded.get_entity_id() == 42


def test_serialize_round_trip_all_pod_components() -> None:
    """The all-POD subset — the fast path Step A targets and Step B
    can reserve exact bytes for."""
    e = _make_entity(POD_COMPONENT_KEYS)
    blob = e.serialize()

    assert e.serialize() == blob
    decoded = EntityInterface.deserialize(blob)
    assert decoded.serialize() == blob


def test_serialize_round_trip_only_non_pod_components() -> None:
    """The non-trivially-copyable subset — exercises the struct_pack
    fallback branch in isolation so a future fast-path bug can't mask a
    fallback regression."""
    e = _make_entity(NON_POD_COMPONENT_KEYS)
    blob = e.serialize()

    assert e.serialize() == blob
    decoded = EntityInterface.deserialize(blob)
    assert decoded.serialize() == blob


# ---------------------------------------------------------------------------
# Field-level smoke checks on a handful of scalar components — guards
# against an encoder bug that leaves bytes round-trip-equal but writes
# the wrong byte for the wrong field (e.g. an offset swap between two
# same-typed components).
# ---------------------------------------------------------------------------


def test_position_fields_round_trip() -> None:
    e = _make_entity(("position",))
    decoded = EntityInterface.deserialize(e.serialize())
    p = decoded.get_position()
    assert p.x == 10
    assert p.y == 20
    assert p.z == 30
    assert p.direction == DirectionEnum.DOWN


def test_velocity_fields_round_trip() -> None:
    e = _make_entity(("velocity",))
    decoded = EntityInterface.deserialize(e.serialize())
    v = decoded.get_velocity()
    assert v.vx == pytest.approx(1.5)
    assert v.vy == pytest.approx(-2.25)
    assert v.vz == pytest.approx(0.5)


def test_health_fields_round_trip() -> None:
    e = _make_entity(("health",))
    decoded = EntityInterface.deserialize(e.serialize())
    h = decoded.get_health()
    assert h.health_level == pytest.approx(75.5)
    assert h.max_health == pytest.approx(100.0)


def test_entity_type_fields_round_trip() -> None:
    e = _make_entity(("entity_type",))
    decoded = EntityInterface.deserialize(e.serialize())
    t = decoded.get_entity_type()
    assert t.main_type == 2
    assert t.sub_type0 == 5
    assert t.sub_type1 == 7


def test_inventory_round_trip_preserves_item_ids() -> None:
    """Non-POD component round-trip — list contents survive the
    `struct_pack` fallback path. `resize(8)` pre-populates eight `-1`
    slots; `add_item` fills the first three with the supplied IDs and
    leaves the remaining five at `-1`."""
    e = _make_entity(("inventory",))
    decoded = EntityInterface.deserialize(e.serialize())
    inv = decoded.get_inventory()
    assert list(inv.item_ids) == [11, 22, 33, -1, -1, -1, -1, -1]
    assert inv.max_items == 8


def test_parents_round_trip_preserves_ids() -> None:
    e = _make_entity(("parents",))
    decoded = EntityInterface.deserialize(e.serialize())
    p = decoded.get_parents()
    assert list(p.parents) == [101, 202, 303]


def test_tile_effects_list_round_trip_preserves_ids() -> None:
    e = _make_entity(("tile_effects_list",))
    decoded = EntityInterface.deserialize(e.serialize())
    tel = decoded.get_tile_effects_list()
    assert list(tel.tile_effects_ids) == [7]


# ---------------------------------------------------------------------------
# Direct-FlatBuffer-arena write (Step C) parity — verifies the bytes
# produced by `EntityInterface::serializeInto` (called from inside the
# FB arena via `CreateUninitializedVector`) are byte-equal to the bytes
# produced by `EntityInterface::serialize()`.
#
# The C++ call sites we cover here are the two reachable from Python:
#   - `PerceptionResponse::serializeFlatBuffer` — the perceiver entity.
#   - `WorldView::serializeEntities` — the visible-entities loop, which
#     is the hot ~35k-voxel path on a typical player perception cube.
# The third call site (the `itemsEntities` map inside PerceptionResponse)
# is shape-identical to the perceiver path and is covered by the same
# round-trip invariant.
# ---------------------------------------------------------------------------


def _empty_world_view() -> WorldView:
    wv = WorldView()
    wv.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)
    return wv


def _all_components_perceiver(entity_id: int) -> EntityInterface:
    e = EntityInterface()
    e.set_entity_id(entity_id)
    for builder in SINGLE_COMPONENT_BUILDERS.values():
        builder(e)
    return e


def test_perception_response_perceiver_entity_arena_write_byte_equal() -> None:
    """The perceiver entity goes through `PerceptionResponse::serializeFlatBuffer`'s
    direct-arena `CreateUninitializedVector` path. The bytes inside the
    FB envelope must match what `entity.serialize()` would have produced
    via the old vector-then-copy path."""
    entity = _all_components_perceiver(99)
    expected_bytes = entity.serialize()

    pr = PerceptionResponse(entity, _empty_world_view())
    serialized = pr.serialize_flatbuffer()
    pr_flatb = PerceptionResponseFlatB(serialized)

    decoded = pr_flatb.getEntity()
    # The reader re-serializes; if the bytes the FB writer placed into
    # the arena diverged from `serialize()`'s bytes, this comparison
    # surfaces it — without coupling the test to a specific byte layout.
    assert decoded.serialize() == expected_bytes
    assert decoded.get_entity_id() == 99


def test_perception_response_visible_entity_arena_write_byte_equal() -> None:
    """The hot ~35k-voxel path in `WorldView::serializeEntities`. Same
    invariant as above, but the entity goes through the `entitiesMap`
    loop instead of the perceiver branch."""
    visible = _all_components_perceiver(7)
    expected_bytes = visible.serialize()

    wv = _empty_world_view()
    wv.entities[7] = visible

    pr = PerceptionResponse(EntityInterface(), wv)
    serialized = pr.serialize_flatbuffer()
    pr_flatb = PerceptionResponseFlatB(serialized)

    decoded_wv = WorldView.deserialize_flatbuffer(pr_flatb.getWorldView())
    assert 7 in decoded_wv.entities
    decoded_visible = decoded_wv.entities[7]
    assert decoded_visible.serialize() == expected_bytes
    assert decoded_visible.get_entity_id() == 7


def test_perception_response_multi_entity_round_trip_fields_match() -> None:
    """Round-trip multiple visible entities through the arena-write path
    and the reader, asserting selected field values survive. Guards
    against an arena-offset bug that would leave bytes round-trip-equal
    on one entity while corrupting another at a downstream offset."""
    wv = _empty_world_view()

    e1 = EntityInterface()
    e1.set_entity_id(1)
    p1 = Position()
    p1.x = 10
    p1.y = 20
    p1.z = 30
    p1.direction = DirectionEnum.DOWN
    e1.set_position(p1)
    wv.entities[1] = e1

    e2 = EntityInterface()
    e2.set_entity_id(2)
    inv = Inventory()
    inv.max_items = 4
    inv.resize(4)
    inv.add_item(101)
    inv.add_item(202)
    e2.set_inventory(inv)
    h2 = HealthComponent()
    h2.health_level = 42.0
    h2.max_health = 99.0
    e2.set_health(h2)
    wv.entities[2] = e2

    pr = PerceptionResponse(EntityInterface(), wv)
    pr_flatb = PerceptionResponseFlatB(pr.serialize_flatbuffer())

    decoded_wv = WorldView.deserialize_flatbuffer(pr_flatb.getWorldView())
    assert decoded_wv.entities[1].get_position().x == 10
    assert decoded_wv.entities[1].get_position().direction == DirectionEnum.DOWN
    assert list(decoded_wv.entities[2].get_inventory().item_ids) == [101, 202, -1, -1]
    assert decoded_wv.entities[2].get_health().health_level == pytest.approx(42.0)
