# Aetherion TDD

Test-driven development workflow for Aetherion's C++/Python stack. Adapted from Matt Pocock's TDD skill. Use this when writing new features, fixing bugs, or any time tests should guide implementation.

---

## Core Philosophy

Test behaviour through public interfaces, not implementation details. A test that breaks when you rename an internal variable is not testing behaviour — it is testing implementation. Tests that survive refactoring are tests worth writing.

---

## What Counts as "Public Interface" in Aetherion

**Python level (preferred for integration/regression tests):**
- `world.step(N)` — advance the simulation
- `world.get_terrain_id(x, y, z)` — read voxel terrain ID
- `world.get_water_matter(x, y, z)` — read water state
- Any method exposed via `bindings/`

**C++ level (for unit tests only):**
- `VoxelGrid` public methods
- `TerrainGridRepository` public methods

**Never test through these implementation details:**
- Raw EnTT entity IDs — they recycle and change version; asserting on a specific ID is meaningless
- `TerrainStorage` grid accessors (these bypass `TerrainGridRepository`)
- Internal ECS component state via `registry.get<Component>(entity)` in Python tests
- Whether a specific entity handle is valid — test observable terrain state instead

---

## Horizontal vs Vertical Slices

**Horizontal (wrong approach):**
```
RED   [--- test A ---]  [--- test B ---]  [--- test C ---]
GREEN [--- test A ---]  [--- test B ---]  [--- test C ---]
```
Write all tests first, then implement everything. You spend a long time in the RED phase with no working system to validate against.

**Vertical (correct approach):**
```
       Slice 1          Slice 2          Slice 3
RED    [test A]
GREEN  [test A]
RED                     [test B]
GREEN                   [test B]
RED                                      [test C]
GREEN                                    [test C]
```
One thin vertical slice at a time. Each slice is a complete working behaviour. You always have something that works.

---

## 4-Phase Workflow

### Phase 1 — Planning
Before writing any code, identify the slices:
- What is the observable end-state the feature produces?
- What is the thinnest first slice that proves the core path works?
- List remaining slices in order of risk (highest first).

### Phase 2 — Tracer Bullet
Write one failing test for the first slice and make it pass with minimal code. This validates the architecture before you build on it.

**Example (new water behaviour):**
1. Write one failing test in `tests/reference/` asserting the end-state (e.g., water reaches the bottom of a slope in N steps)
2. Run `make test` → confirm RED
3. Add the minimal C++ change in `EcosystemEngine.cpp` or `PhysicsEngine.cpp`
4. Run `make build-install-test` → confirm GREEN
5. Repeat for the next behaviour slice

### Phase 3 — Incremental Loop
Per-cycle checklist:
1. Write one test that captures the next slice of behaviour
2. Run `make test` and confirm it is RED (if not, the test is wrong or the behaviour already exists)
3. Write the minimal implementation to make it pass
4. Run `make build-install-test` and confirm GREEN
5. Check: does anything need refactoring before the next cycle?

### Phase 4 — Refactor
Refactor after green, never during red. Triggers:
- Duplication: the same logic appears in two or more places
- Long methods: a function does more than one thing
- Shallow modules: a class with a wide interface but trivial implementation
- Feature envy: a method that uses another class's data more than its own

---

## Deep Modules Principle

Prefer small interfaces with substantial implementations. A module is "deep" when there is a lot of complexity hiding behind a simple surface. `TerrainGridRepository` is a good example — callers say `setWaterMatter(x, y, z, v)` and the lock/VDB/ECS consistency is invisible. A shallow module exposes the internal complexity: `getStorage()->getWaterGrid()->setValue(coord, v)`.

When writing new systems, ask: what is the simplest interface that hides the most complexity?

---

## Mocking Rules

Mock only at true system boundaries. In Aetherion there are almost no external I/O boundaries, so avoid mocks almost entirely.

**Rare cases where mocking is acceptable:**
- `GameClock` — for deterministic time in tests
- File-based logging (`water_debug.jsonl`) — if you need to assert on log output

**Never mock:**
- `VoxelGrid`
- `TerrainGridRepository`
- `PhysicsEngine`
- `EcosystemEngine`

If you feel the urge to mock one of these, the problem is usually that the test is reaching too deep into implementation detail. Back up and test observable state instead.

---

## Dependency Injection

`World` accepts `VoxelGrid` by ref; `PhysicsEngine` accepts `EcosystemEngine` by ref. New systems must follow the same pattern: receive collaborators through the constructor, not by calling singletons (the only established exception is `PhysicsManager::Instance()`).

---

## Test Command Rule

Always use `make test` or `make build-install-test`. Never run `pytest` directly. The Makefile selects the correct conda env (`aetherion-312`) and flags.

| When | Command |
|------|---------|
| C++ changed | `make build-install-test` |
| Python/test only changed | `make test` |

---

## Canonical Examples

**Good test — tests observable water-flow behaviour:**
```python
def test_water_flows_downhill_one_step(world):
    world.set_water_matter(5, 50, 8, 100)
    world.step(1)
    assert world.get_water_matter(5, 50, 7) > 0   # water moved down
    assert world.get_water_matter(5, 50, 8) < 100  # source decreased
```

**Bad test — coupled to ECS internals, breaks on entity overflow:**
```python
def test_water_entity_created(world):
    world.set_water_matter(5, 50, 8, 100)
    world.step(1)
    entity_id = world.get_terrain_id(5, 50, 7)
    assert entity_id > 0  # BAD: asserts on entity handle, breaks on EnTT version overflow
```

The bad test will fail after ~256 recycles of the same voxel index even though the simulation is correct. Assert on water matter, not on entity handles.
