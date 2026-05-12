"""Perception scaling micro-benchmarks for the per-tick perception path.

These tests probe ``World.create_perception_responses`` — the call that
``WorldInterface.get_perception_responses`` makes once per world tick —
along three independent axes so we can pin down which one is driving the
``wait_for_world_state`` stall the main thread spends ~40% of its frame
budget on (Tracy capture, 2026-05-12):

  1. ``perception_area`` (per-entity view-cube radius)
  2. number of perceivers in a single call
  3. world dimensions, with perceiver count held constant

The default ranges complete in well under a second each and are safe
for CI. Set ``LIFESIM_PERCEPTION_BENCH=1`` to enable the full sweep that
prints a markdown growth table on stdout — run with ``-s -rP`` to view.
"""

from __future__ import annotations

import os
import time
from collections.abc import Iterable
from dataclasses import dataclass

import pytest

import aetherion
from aetherion import (
    DirectionEnum,
    EntityEnum,
    EntityTypeComponent,
    GridType,
    Inventory,
    PerceptionComponent,
    Position,
)
from aetherion.entities import BaseEntity

BENCHMARK_MODE = os.environ.get("LIFESIM_PERCEPTION_BENCH", "0") == "1"


class _Perceiver(BaseEntity):
    """Minimal entity that satisfies the perception pipeline.

    ``createPerceptionResponseC`` throws if the observer is missing
    ``Position`` or ``PerceptionComponent``. ``EntityTypeComponent`` is
    needed by ``allView`` and ``Inventory`` is added so the observer
    self-view exercises ``buildInventoryItems``.
    """

    grid_type = GridType.ENTITY

    def __init__(
        self,
        x: int,
        y: int,
        z: int,
        *,
        perception_area: int = 5,
        z_perception_area: int = 1,
    ) -> None:
        BaseEntity.__init__(self, x, y, z)

        position = Position()
        position.x = x
        position.y = y
        position.z = z
        position.direction = DirectionEnum.DOWN
        self.position = position

        entity_type = EntityTypeComponent()
        entity_type.main_type = EntityEnum.BEAST.value
        entity_type.sub_type0 = 0
        entity_type.sub_type1 = 0
        self.entity_type = entity_type

        perception = PerceptionComponent()
        perception.perception_area = perception_area
        perception.z_perception_area = z_perception_area
        self.perception = perception

        self.inventory = Inventory()


def _make_world(width: int, height: int, depth: int) -> aetherion.World:
    """Bare ``World`` with an initialised voxel grid — no WorldManager,
    no water-sim threads, no ecosystem worker. Perception only needs the
    registry and the voxel grid, so this keeps timings clean."""
    world = aetherion.World(width, height, depth)
    world.initialize_voxel_grid()
    return world


def _grid_positions(n: int, world_size: int) -> list[tuple[int, int]]:
    """Evenly spaced unique (x, y) positions for n perceivers inside a
    ``world_size`` × ``world_size`` plane. Stops early if the grid is
    exhausted instead of overwriting positions."""
    cols = max(1, int(n**0.5) + 1)
    spacing = max(1, world_size // (cols + 1))
    out: list[tuple[int, int]] = []
    for i in range(n):
        col = i % cols
        row = i // cols
        x = (col + 1) * spacing
        y = (row + 1) * spacing
        if x >= world_size or y >= world_size:
            break
        out.append((x, y))
    return out


def _spawn_perceivers(
    world: aetherion.World,
    positions: Iterable[tuple[int, int]],
    *,
    perception_area: int,
    z_perception_area: int = 1,
    z: int = 1,
) -> list[int]:
    ids: list[int] = []
    for x, y in positions:
        ent = world.create_entity(
            _Perceiver(
                x,
                y,
                z,
                perception_area=perception_area,
                z_perception_area=z_perception_area,
            )
        )
        ids.append(ent.get_id())
    return ids


def _time_perception_call(world: aetherion.World, entity_ids: list[int]) -> float:
    queries: dict[int, list] = {eid: [] for eid in entity_ids}
    t0 = time.perf_counter()
    _ = world.create_perception_responses(queries)
    return time.perf_counter() - t0


def _median_perception_time(
    world: aetherion.World,
    entity_ids: list[int],
    *,
    iterations: int = 5,
    warmup: int = 1,
) -> float:
    for _ in range(warmup):
        _time_perception_call(world, entity_ids)
    samples = sorted(_time_perception_call(world, entity_ids) for _ in range(iterations))
    return samples[len(samples) // 2]


# ────────────────────────────────────────────────────────────────────────────
# Correctness — prove the pipeline works on a bare World before benchmarking.
# ────────────────────────────────────────────────────────────────────────────


def test_create_perception_responses_returns_one_payload_per_observer():
    world = _make_world(20, 20, 4)
    ids = _spawn_perceivers(world, [(5, 5), (15, 15)], perception_area=3)

    responses = world.create_perception_responses({eid: [] for eid in ids})

    assert set(responses.keys()) == set(ids)
    for eid, payload in responses.items():
        assert len(payload) > 0, f"empty perception payload for entity {eid}"


def test_create_perception_responses_handles_single_observer():
    world = _make_world(20, 20, 4)
    [pid] = _spawn_perceivers(world, [(10, 10)], perception_area=5)

    responses = world.create_perception_responses({pid: []})

    assert pid in responses
    assert len(responses[pid]) > 0


# ────────────────────────────────────────────────────────────────────────────
# Scaling along one axis at a time. Each test prints µs/call so a `-s` run
# makes the growth curve visible without needing a separate benchmark tool.
# Assertions are loose ceilings — the *shape* of the printed numbers is the
# real signal.
# ────────────────────────────────────────────────────────────────────────────


@pytest.mark.parametrize("perception_area", [1, 3, 5, 10, 15])
def test_perception_time_grows_with_perception_area(perception_area):
    """Single observer, fixed world; sweep perception_area.

    Volume of the scanned cube is ``(2r+1)² × (2zr+1)``. With z fixed at
    1 the curve should look like ``O(r²)``. The y-intercept exposes
    per-call overhead (lock acquisition, GIL flips, serialization).
    """
    world = _make_world(64, 64, 4)
    [pid] = _spawn_perceivers(world, [(32, 32)], perception_area=perception_area)

    elapsed = _median_perception_time(world, [pid])

    print(f"[area={perception_area:>2}] {elapsed * 1e6:>9.1f} µs per call")
    assert elapsed < 1.0  # ceiling, not a tight assertion


@pytest.mark.parametrize("entity_count", [1, 4, 16, 64])
def test_perception_time_grows_with_entity_count(entity_count):
    """Fixed world, fixed perception_area; sweep observer count.

    ``createPerceptionResponses`` splits jobs into 16 TBB batches, so
    above 16 the per-entity cost should drop noticeably on a multi-core
    box — if it stays flat or grows, the batching is being serialised
    by a downstream lock (registryMutex / entityLifecycleMutex).
    """
    world = _make_world(128, 128, 4)
    positions = _grid_positions(entity_count, world_size=128)
    ids = _spawn_perceivers(world, positions, perception_area=3)
    assert len(ids) == entity_count, "grid was too small to fit all perceivers"

    elapsed = _median_perception_time(world, ids)

    print(
        f"[entities={entity_count:>3}] {elapsed * 1e6:>9.1f} µs total / "
        f"{(elapsed / entity_count) * 1e6:>7.1f} µs per entity"
    )
    assert elapsed < 5.0


@pytest.mark.parametrize("world_dim", [16, 32, 64, 128])
def test_perception_time_flat_with_world_size_when_count_held_constant(world_dim):
    """One observer, fixed perception_area, vary world dimensions.

    The scan is region-bounded — it should be flat across world sizes.
    If this curve grows, the voxel-grid region walk has a hidden
    world-size factor (e.g. iterating the whole grid instead of the
    visible cube), which would be a real root-cause of the FPS wall
    appearing as worlds enlarge.
    """
    world = _make_world(world_dim, world_dim, 4)
    [pid] = _spawn_perceivers(world, [(world_dim // 2, world_dim // 2)], perception_area=5)

    elapsed = _median_perception_time(world, [pid])

    print(f"[world={world_dim:>4}²] {elapsed * 1e6:>9.1f} µs per call")
    assert elapsed < 1.0


# ────────────────────────────────────────────────────────────────────────────
# Combined-axis growth — proves the user's "exponential" hunch by holding
# density constant while world grows: entity count ∝ world_area, so total
# cost should be ``world_area × per_entity_cost(area)``. Env-gated because it
# walks larger worlds and takes a few seconds.
# ────────────────────────────────────────────────────────────────────────────


@pytest.mark.skipif(not BENCHMARK_MODE, reason="set LIFESIM_PERCEPTION_BENCH=1 to run")
@pytest.mark.parametrize("world_dim", [16, 32, 64, 128])
def test_perception_time_constant_density_sweep(world_dim):
    """Constant density: 1 perceiver per 64 tiles. World grows, count
    grows with it, total cost should follow ``world_dim²``."""
    density_denominator = 64
    n_perceivers = max(1, (world_dim * world_dim) // density_denominator)

    world = _make_world(world_dim, world_dim, 4)
    positions = _grid_positions(n_perceivers, world_size=world_dim)
    ids = _spawn_perceivers(world, positions, perception_area=3)

    elapsed = _median_perception_time(world, ids)

    print(
        f"[world={world_dim:>4}², n={len(ids):>5}] "
        f"{elapsed * 1e3:>8.2f} ms total / "
        f"{(elapsed / len(ids)) * 1e6:>7.1f} µs per entity"
    )
    assert elapsed < 30.0


# ────────────────────────────────────────────────────────────────────────────
# Full cross-product sweep — single test that emits a markdown table. Useful
# to paste into a perf doc. Gated by LIFESIM_PERCEPTION_BENCH.
# ────────────────────────────────────────────────────────────────────────────


@dataclass
class _SweepResult:
    world_dim: int
    perception_area: int
    n_entities: int
    elapsed_s: float


@pytest.mark.skipif(not BENCHMARK_MODE, reason="set LIFESIM_PERCEPTION_BENCH=1 to run")
def test_perception_growth_sweep_table():
    """Cross-product over (world_dim × perception_area × entity_count).

    Prints a markdown table on stdout (run with ``-s``). Each row is a
    fresh world so background state does not bleed between cells.
    """
    world_dims = [32, 64, 128]
    perception_areas = [3, 5, 10]
    entity_counts = [1, 16, 64]

    results: list[_SweepResult] = []
    for world_dim in world_dims:
        for area in perception_areas:
            for count in entity_counts:
                world = _make_world(world_dim, world_dim, 4)
                positions = _grid_positions(count, world_size=world_dim)
                if len(positions) < count:
                    continue  # world too small for this count
                ids = _spawn_perceivers(world, positions, perception_area=area)
                elapsed = _median_perception_time(world, ids, iterations=3, warmup=1)
                results.append(_SweepResult(world_dim, area, count, elapsed))

    print()
    print("| world |  area | entities |    total |   per-entity |")
    print("|------:|------:|---------:|---------:|-------------:|")
    for r in results:
        per = (r.elapsed_s / r.n_entities) if r.n_entities else 0.0
        print(
            f"| {r.world_dim:>5} | {r.perception_area:>5} | {r.n_entities:>8} | "
            f"{r.elapsed_s * 1e3:>6.2f} ms | {per * 1e6:>9.1f} µs |"
        )
