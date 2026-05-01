# Aetherion Debugging

Where to look when water or terrain is misbehaving, and how to add structured diagnostics without breaking behaviour.

---

## Canonical "Where Is the Water?" Debug Sequence

When a voxel at `(x, y, z)` behaves unexpectedly, check in this order:

1. **Sentinel value** — `voxel_grid.get_terrain(x, y, z)`. Values: `-2` (NONE, empty), `-1` (ON_GRID_STORAGE, static/vapor), `≥ 0` (live ECS entity). Any value `< -2` is corruption (EnTT version overflow).
2. **Matter counts** — `repo->getTerrainMatterContainer(x, y, z)` for `WaterMatter` and `WaterVapor`. Zero in both with a live entity = cleanup missed.
3. **ECS presence flag** — `repo->readTerrainInfo(x, y, z).active`. More reliable than checking `terrainId >= 0` because it reads the authoritative flag, not the VDB-stored handle.
4. **Cross-check logs** — grep `water_debug.jsonl` for `"x":<x>,"y":<y>,"z":<z>` in the tick window of interest. Compare `entity` and `entity_hex` values across entries to spot recycled handles.

---

## `water_debug.jsonl` — Structured Debug Log

**File:** `water_debug.jsonl` in the project root (append-mode, created at first log call).

**Format:** One self-contained JSON object per line. Common fields:

| Field | Meaning |
|-------|---------|
| `event` | String tag for the log site (`"delete_terrain_enter"`, `"delete_terrain_exit"`, `"invalid_terrain_id"`, `"recovery_triggered"`, `"recovery_new_entity"`) |
| `x`, `y`, `z` | Voxel coordinates |
| `entity` / `entity_hex` | Raw EnTT handle as decimal and hex |
| `thread` | Thread ID as decimal string (from `std::this_thread::get_id()`) |
| `grid_locked` | Whether the terrain lock was held at the log site |
| `take_lock` | The `takeLock` argument passed to `deleteTerrain` |

**Parse with jq:**
```bash
# All events at a specific voxel
jq 'select(.x==89 and .y==50 and .z==1)' water_debug.jsonl

# Find the first invalid terrain ID event
jq 'select(.event=="invalid_terrain_id")' water_debug.jsonl | head -5

# Count events per thread
jq -r '.thread' water_debug.jsonl | sort | uniq -c
```

**Parse with Python:**
```python
import json, pathlib
lines = [json.loads(l) for l in pathlib.Path("water_debug.jsonl").read_text().splitlines() if l]
bad = [e for e in lines if e.get("event") == "invalid_terrain_id"]
```

---

## Activating / Adding Debug Logging

**Guard function** (`src/debug/WaterDebugLog.hpp`):
```cpp
// Watch corridor matches crash coordinates x=88..99, y=49..51, z=0..2
inline bool waterDebugInWatchRegion(int x, int y, int z) {
    return x >= 88 && x <= 99 && y >= 49 && y <= 51 && z >= 0 && z <= 2;
}
```
Edit these bounds to focus on the corridor of interest before rebuilding.

**Log function** (thread-safe, same header):
```cpp
inline void waterDebugLog(const std::string &json_line) { /* appends to water_debug.jsonl */ }
```

**Adding a new log site:**
```cpp
if (waterDebugInWatchRegion(x, y, z)) {
    std::ostringstream jss;
    jss << "{\"event\":\"my_event\""
        << ",\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z
        << ",\"entity\":" << static_cast<int>(entity)
        << ",\"entity_hex\":\"0x" << std::hex << static_cast<unsigned int>(static_cast<int>(entity)) << std::dec << "\""
        << ",\"thread\":\"" << waterDebugThreadId() << "\"}";
    waterDebugLog(jss.str());
}
```

Include the header in any `.cpp` file that needs it:
```cpp
#include "debug/WaterDebugLog.hpp"
```

---

## spdlog Levels Used in This Codebase

| Level | When used |
|-------|-----------|
| `debug` | Per-tick verbose output — disabled in normal runs; use for local investigation only |
| `info` | Normal flow events (entity created, event dispatched) |
| `warn` | Unexpected-but-recoverable state (empty terrain found where water expected, entity missing `Position` component) |
| `error` | Invariant violation (overflow terrain ID detected, invalid entity handle used) |

Logger name throughout the codebase: `spdlog::get("console")`.

---

## Python Invariant Scan Patterns

From `tests/reference/test_water_sim_terrain_invariants.py` — copy these patterns when writing new regression tests.

**Scan a region for corrupted terrain IDs:**
```python
TERRAIN_ID_NONE = -2  # any value < -2 is corruption

def scan_region(voxel_grid, x_range, y_range, z_range):
    corrupted = []
    for x in x_range:
        for y in y_range:
            for z in z_range:
                tid = voxel_grid.get_terrain(x, y, z)
                if tid < TERRAIN_ID_NONE:
                    corrupted.append((x, y, z, tid))
    return corrupted
```

**Step loop with early exit on first corruption:**
```python
for step in range(1000):
    manager.update()
    if (step + 1) % 100 == 0:
        corrupted = scan_region(voxel_grid, range(88, 100), range(49, 52), range(0, 3))
        if corrupted:
            assert False, f"Corruption at step {step + 1}: {corrupted}"
```

**Check worker-thread water errors:**
```python
assert not world.has_water_sim_errors(), world.get_water_sim_errors()
```

---

## Test Commands

```bash
make test                 # run full pytest suite (correct conda env selected automatically)
make build-install-test   # rebuild C++, reinstall Python package, then run tests
```

**Never run `pytest` directly** — the Makefile selects the `aetherion-312` conda environment. Running pytest outside of make silently uses the wrong interpreter.

---

## Known Crash Voxels (from water regression)

Voxels that triggered EnTT version overflow in the mountain-side regression scenario:

```
(89, 50, 1)  (91, 50, 1)  (94, 50, 1)  (96, 50, 1)
```

These are on the mountain-side slope where water flows fastest. High-traffic voxels exhaust the 256-version limit (~700 simulation steps). Check these first when a water crash is reported.
