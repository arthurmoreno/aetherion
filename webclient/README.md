# @life/aetherion-webclient

Lightweight TypeScript client shims for the Aetherion engine, plus an optional WebAssembly (embind) build that exposes a minimal API surface. This package is designed to be consumed by the `life-sim-web` PixiJS client during development.

The TypeScript implementation provides practical wrappers around flatbuffer payloads (PerceptionResponse, WorldView, EntityInterface). The optional WASM module compiles a small embind wrapper with a compatible API surface to aid future migration to native-backed parsing if needed.

## Features

- PerceptionResponseFlatB: Wraps a flatbuffer `PerceptionResponse` and exposes convenient getters.
- WorldView.deserialize_flatbuffer: Builds a lightweight object from generated flatbuffer classes.
- EntityInterface: Custom serializer/deserializer compatible with the C++ header format (entity id + component mask + payload bytes).
- Optional WASM module (embind) exporting similarly named classes for experimentation/perf.
  - Helper `deserializeEntityInterface(bytes)` that prefers WASM and falls back to TS.
  - `loadAetherionWasm()` loader and `getCachedAetherionWasm()` accessor.

## Folder Layout

- `src/` — TypeScript sources (flatbuffer wrappers, helpers).
- `dist/` — Build output (JS + .d.ts). The WASM build also writes here.
- `wasm/` — Embind sources and build script.
  - `emcc_wrapper.cpp` — Self-contained embind implementation that compiles with Emscripten.
  - `embind_wrapper.cpp` — Placeholder to mirror expected C++ bindings (kept here for reference/expansion).
  - `build.sh` — Compiles the WASM ES module and .wasm binary into `dist/`.

## Prerequisites

- Node.js 18+ and npm.
- Emscripten SDK installed and activated for optional WASM build:
  - Install: https://emscripten.org/docs/getting_started/downloads.html
  - Activate in your shell (example):
    - `source /path/to/emsdk/emsdk_env.sh`
  - Verify: `emcc -v` and/or `em++ -v` prints version info.

## Install Dependencies

This package is intended to be used inside the repository workspace. If you want to build it independently:

```
cd aetherion/webclient
npm install
```

Note: `flatbuffers` is already declared in `devDependencies` for local builds; it is a `peerDependency` for consumers.

## Build (TypeScript)

Builds the TypeScript sources to `dist/` (ES modules + type declarations):

```
cd aetherion/webclient
npm run build
```

Outputs (examples):
- `dist/index.js`, `dist/index.d.ts`
- `dist/perception-response-flatb.js`, `dist/perception-response-flatb.d.ts`
- `dist/world-view.js`, `dist/world-view.d.ts`
- `dist/entity-interface.js`, `dist/entity-interface.d.ts`

## Build (WASM, optional)

Compiles a minimal embind wrapper into `dist/aetherion_wasm.mjs` and `dist/aetherion_wasm.wasm`:

```
cd aetherion/webclient
npm run build:wasm
```

Advanced: select compiler explicitly

```
EMCC=/path/to/em++ npm run build:wasm
```

The build script uses:
- `-s MODULARIZE=1 -s EXPORT_ES6=1` to emit an ES module loader function.
- `-s ENVIRONMENT=web` for browser environments.
- `-s ALLOW_MEMORY_GROWTH=1` to avoid OOM during experiments.
- `-lembind` to enable Embind bindings.

Quick usage in app code:

```ts
import { loadAetherionWasm, deserializeEntityInterface } from '@life/aetherion-webclient';

await loadAetherionWasm(); // optional; speeds up first call
const ent = await deserializeEntityInterface(bytes);
console.log(ent.get_entity_id?.() ?? ent.get_entity_id());
```

## Using the TypeScript API

Example: wrapping a PerceptionResponse flatbuffer returned by your server.

```ts
import { PerceptionResponseFlatB, WorldView, EntityInterface } from '@life/aetherion-webclient';

// `bytes` is a Uint8Array containing the PerceptionResponse flatbuffer
const pr = new PerceptionResponseFlatB(bytes);

// World view wrapper
const fbWorldView = pr.getWorldView?.();
const world = WorldView.deserialize_flatbuffer(fbWorldView);
if (world) {
  const { width, height, depth } = world;
  console.log('World bounds:', width, height, depth);
}

// Entity interface wrapper
const ent = pr.getEntity?.();
if (ent) {
  console.log('Entity ID:', ent.get_entity_id());
}
```

EntityInterface can also deserialize a standalone serialized entity buffer:

```ts
const entityBytes: Uint8Array = getBytesFromSomewhere();
const entity = EntityInterface.deserialize(entityBytes);
console.log(entity.get_entity_id());
```

## Loading the WASM Module (optional)

The WASM module is an ES module factory. Load it dynamically and use the exported classes. This is optional; most flows can use the pure TypeScript wrappers.

```ts
import AetherionWasm from '@life/aetherion-webclient/dist/aetherion_wasm.mjs';

const mod = await AetherionWasm({
  locateFile: (path, dir) => new URL(path, dir).href,
});

const { PerceptionResponseFlatB, EntityInterface, WorldView } = mod;
const bytes = new Uint8Array([ /* ... */ ]);
const pr = new PerceptionResponseFlatB(bytes);
console.log(pr.get_ticks());
```

Notes:
- The WASM classes have the same method names but are a minimal shim. They are safe for experimentation and can be extended later to align with the full C++ implementation.
- Ensure your bundler serves the `.wasm` file. With Vite, the `locateFile` above is sufficient in dev.

## Consuming in a Vite app (dev)

Within this repository, `life-sim-web` consumes `@life/aetherion-webclient` as a workspace dependency. For external usage, you can link the package from a local path while developing:

```
# In your app
npm install flatbuffers
npm install /absolute/path/to/aetherion/webclient
```

Then import the APIs as shown above. If you also want to use the WASM module, run `npm run build:wasm` in this package first.

## Development Tips

- Rebuild TS after changes: `npm run build` (or wire a watcher if you prefer).
- Rebuild WASM when changing any `.cpp` files: `npm run build:wasm`.
- Verify Emscripten is active in your shell (run `emcc -v`).
- If you see CORS issues for the `.wasm` file in dev, confirm the loader `locateFile` resolves to the correct URL.

## API Surface (quick reference)

- `PerceptionResponseFlatB(data: ArrayBuffer | Uint8Array)`
  - `getWorldView(): FBWorldView | null`
  - `getEntity(): EntityInterface | null`
  - `get_item_from_inventory_by_id(id: number): EntityInterface | null`
  - `get_query_response_by_id(id: number): Uint8Array | null`
  - `get_ticks(): number`

- `WorldView.deserialize_flatbuffer(fbWorldView)` → `{ width, height, depth, get_voxel_bounds(), check_if_terrain_exist(x,y,z), get_terrain(x,y,z), check_if_entity_exist(x,y,z), get_entity(x,y,z) } | null`

- `EntityInterface`
  - `static deserialize(bytes: ArrayBuffer | Uint8Array): EntityInterface`
  - `get_entity_id(): number`
  - `serialize(): Uint8Array` (header + payload)

## License

Private development package. See repository-level license if/when published.

## Testing (cross-language)

These tests validate that bytes generated by the Python Aetherion package can be read by this webclient, and that the optional WASM module loads correctly.

Prerequisites:
- Have the Python Aetherion extension importable (the repo includes a prebuilt wheel). The simplest path is to activate the provided conda env:
  - `conda activate aetherion-312`

Steps:
- Build the TypeScript artifacts (and optionally the WASM):
  - `npm run build`
  - `npm run build:wasm` (optional; enables the WASM portion of tests)
- Run the Node-based tests:
  - `npm run test:web`

What it does:
- Runs `tests/gen_perception_bytes.py` with your Python to serialize a small PerceptionResponse flatbuffer under `tests/fixtures/perception.bin`.
  - If needed, you can point to a specific Python via `PYTHON=/path/to/python node tests/run-tests.mjs`.
- Loads `dist/index.js` and deserializes those bytes using `PerceptionResponseFlatB` and `WorldView` wrappers.
- If `dist/aetherion_wasm.mjs/.wasm` exist (built via `npm run build:wasm`), it also loads the WASM module and performs a few sanity checks.

Troubleshooting:
- ImportError `aetherion._aetherion` when running the Python generator:
  - Ensure you activated the correct conda env: `conda activate aetherion-312`.
  - Alternatively set `PYTHON=/path/to/conda/envs/aetherion-312/bin/python` before running the test script.
- WASM tests are skipped if the files are missing. Run `npm run build:wasm` to include them.
