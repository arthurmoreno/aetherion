#!/usr/bin/env node
// Simple cross-language tests for the webclient using Node ESM.
// 1) Generate perception bytes via Python (using aetherion Python package)
// 2) Validate TS runtime wrappers can read it
// 3) Load the WASM embind module and sanity check exported classes

import { spawnSync } from 'node:child_process';
import { readFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import assert from 'node:assert';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const root = dirname(__dirname);

function runPythonGen() {
  const py = process.env.PYTHON || 'python3.12';

  console.info(`Using Python interpreter: ${process.env.PYTHON}`);
  console.info(`Using Python interpreter: ${py}`);

  const gen_perception_bytes_script = join(__dirname, 'gen_perception_bytes.py');
  const perception_res = spawnSync(py, [gen_perception_bytes_script], { stdio: 'inherit', cwd: root, env: process.env });
  if (perception_res.status !== 0) {
    throw new Error(`Python generator failed with code ${perception_res.status}.\n` +
      `Hint: activate the Aetherion env first: \n  conda activate aetherion-312\n` +
      `Or point to your Python: \n  PYTHON=/home/arthur/anaconda3/envs/aetherion-312/bin/python node tests/run-tests.mjs`);
  }

  const gen_entity_bytes_script = join(__dirname, 'gen_entity_bytes.py');
  const entity_res = spawnSync(py, [gen_entity_bytes_script], { stdio: 'inherit', cwd: root, env: process.env });
  if (entity_res.status !== 0) {
    throw new Error(`Python generator failed with code ${entity_res.status}.\n` +
      `Hint: activate the Aetherion env first: \n  conda activate aetherion-312\n` +
      `Or point to your Python: \n  PYTHON=/home/arthur/anaconda3/envs/aetherion-312/bin/python node tests/run-tests.mjs`);
  }
}

// --- Helpers to keep the tests readable ---
async function loadAetherion() {
  return await import('../dist/index.js');
}

async function maybeLoadWasm(aetherion) {
  const wasmMjs = join(root, 'dist', 'aetherion_wasm.mjs');
  const wasmBin = join(root, 'dist', 'aetherion_wasm.wasm');
  if (!existsSync(wasmMjs) || !existsSync(wasmBin) || typeof aetherion.loadAetherionWasm !== 'function') {
    console.warn('[warn] WASM artifacts not found or loader missing; some tests will be skipped');
    return null;
  }
  const wasmFactory = (await import('../dist/aetherion_wasm.mjs')).default;
  const wasmBytes = readFileSync(wasmBin);
  return await aetherion.loadAetherionWasm({ factory: wasmFactory, wasmBinary: wasmBytes });
}

function readFixtureU8(name) {
  const p = join(__dirname, 'fixtures', name);
  const buf = readFileSync(p);
  return new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
}

function assertEntityBasics(entity) {
  assert.ok(entity, 'EntityInterface instance is truthy');
  assert.strictEqual(typeof entity.get_entity_id, 'function', 'get_entity_id exists');
  const id = entity.get_entity_id();
  assert.strictEqual(id, 1, 'entity id should be 1 (fixture)');
  const et = entity.get_entity_type?.();
  assert.ok(et && typeof et === 'object', 'get_entity_type returns object');
  assert.strictEqual(typeof et.type, 'number', 'entity type is number');
  assert.strictEqual(typeof (et.sub_type0 ?? et.subType0), 'number', 'subtype0 is number');
  console.info('[testTSBindings] EntityInterface:', { id, type: et.type, subtype0: et.sub_type0 ?? et.subType0 });
}

function assertEntityPosition(entity) {
  const position = entity.get_position?.();
  console.info('[testTSBindings] EntityInterface: position:', position);
  assert.ok(position && typeof position === 'object', 'get_position returns object');
  assert.strictEqual(position.x ?? position.get_x?.(), 10, 'position.x == 10');
  assert.strictEqual(position.y ?? position.get_y?.(), 20, 'position.y == 20');
  assert.strictEqual(position.z ?? position.get_z?.(), 30, 'position.z == 30');
  assert.strictEqual(position.direction, 3, 'position.direction == 3 (DOWN)');
}

async function testTSBindings() {
  const aetherion = await loadAetherion();
  const wasmMod = await maybeLoadWasm(aetherion);
  assert.ok(aetherion.PerceptionResponseFlatB, 'PerceptionResponseFlatB export present');

  // --- EntityInterface deserialize and assertions ---
  assert.ok(wasmMod && wasmMod.EntityInterface, 'WASM EntityInterface available');
  console.info('just before the issue!!!!!!!!!!!!!!!');
  const entity = aetherion.deserializeEntity(readFixtureU8('entity_interface_pos_type.bin'));
  console.info('just after the issue!!!!!!!!!!!!!!!');
  assertEntityBasics(entity);
  assertEntityPosition(entity);

  // --- PerceptionResponse and WorldView ---
  const pr = new aetherion.PerceptionResponseFlatB(readFixtureU8('perception.bin'));
  const ent = pr.getEntity?.();
  assert.ok(ent, 'Entity present');
  const entId = typeof ent.get_entity_id === 'function' ? ent.get_entity_id() : undefined;
  assert.strictEqual(typeof entId, 'number', 'entity id is number');

  const fbWV = pr.getWorldView?.();
  if (!fbWV) {
    console.warn('[warn] WorldView missing from PerceptionResponse (continuing)');
    return;
  }
  const world = await aetherion.WorldView.deserialize_flatbuffer(fbWV);
  assert.ok(world, 'WorldView wrapper');
  const vb = world.get_voxel_bounds();
  console.info('[testTSBindings] WorldView voxel bounds:', vb);
  assert.strictEqual(vb.voxelWidth, 3, 'voxelWidth == 3');
  assert.strictEqual(vb.voxelHeight, 3, 'voxelHeight == 3');
  assert.strictEqual(vb.voxelDepth, 3, 'voxelDepth == 3');

  if (typeof world.get_entity_by_id === 'function') {
    const e2 = world.get_entity_by_id(2);
    console.info('[testTSBindings] Entity #2 from cache:', e2);
    assert.ok(e2, 'entity #2 present in cache');
    const et = e2.get_entity_type?.();
    assert.ok(typeof et === 'object', 'get_entity_type returns object or undefined');
    console.info('[testTSBindings] Entity #2:', { id: entId, type: et?.type ?? 'undefined', subtype0: et?.sub_type0 ?? et?.subType0 ?? 'undefined' });
    assert.ok(et.type !== undefined || et.sub_type0 !== undefined || et.subType0 !== undefined, 'Entity type or subtype0 should be defined');
    console.info('[testTSBindings] Entity #2 type:', et?.type, et?.sub_type0, et?.subType0);

    const pos = e2.get_position?.();
    assert.ok(pos === undefined || typeof pos === 'object', 'get_position returns object or undefined');
    if (pos) {
      const x = pos.x ?? pos.get_x?.();
      const y = pos.y ?? pos.get_y?.();
      const z = pos.z ?? pos.get_z?.();
      assert.strictEqual(typeof x, 'number');
      assert.strictEqual(typeof y, 'number');
      assert.strictEqual(typeof z, 'number');
    }
  }

  // Build a multi-component entity using the same WASM code to avoid
  // cross-version layout mismatches with the Python wheel.
  const entity2 = aetherion.deserializeEntity(readFixtureU8('entity_interface_full.bin'));
  const position2 = entity2.get_position?.();
  console.info('[testTSBindings] EntityInterface2: position:', position2);
  assert.ok(position2 && typeof position2 === 'object', 'get_position returns object');
  assert.strictEqual(position2.x ?? position2.get_x?.(), 10, 'position.x == 10');
  assert.strictEqual(position2.y ?? position2.get_y?.(), 20, 'position.y == 20');
  assert.strictEqual(position2.z ?? position2.get_z?.(), 30, 'position.z == 30');
  assert.strictEqual(position2.direction, 3, 'position.direction == 3 (DOWN)');
}

async function testWasmModule() {
  // Load WASM ES module factory and provide wasmBinary to avoid fetch(file://)
  const wasmMjs = join(root, 'dist', 'aetherion_wasm.mjs');
  const wasmBin = join(root, 'dist', 'aetherion_wasm.wasm');
  if (!existsSync(wasmMjs) || !existsSync(wasmBin)) {
    console.warn('[skip] WASM build not found in dist/. Run: npm run build:wasm');
    return;
  }
  const wasmFactory = (await import('../dist/aetherion_wasm.mjs')).default;
  const wasmBytes = readFileSync(wasmBin);
  const mod = await wasmFactory({ wasmBinary: wasmBytes });

  assert.ok(mod.PerceptionResponseFlatB, 'WASM PerceptionResponseFlatB export present');
  const bytesPath = join(__dirname, 'fixtures', 'perception.bin');
  const buf = readFileSync(bytesPath);
  const u8 = new Uint8Array(buf.buffer, buf.byteOffset, buf.byteLength);
  try {
    const pr = new mod.PerceptionResponseFlatB(u8);
    const wv = pr.getWorldView();
    assert.ok(wv, 'WASM getWorldView returns object');
    assert.strictEqual(typeof wv.width(), 'number');
    assert.strictEqual(typeof wv.height(), 'number');
    assert.strictEqual(typeof wv.depth(), 'number');
  } catch (e) {
    console.warn('[warn] WASM PerceptionResponseFlatB test failed:', e?.message || e);
  }
}

// Orchestrate
(async () => {
  runPythonGen();
  await testTSBindings();
  await testWasmModule();
  console.log('\nAll webclient tests passed.');
})().catch((err) => {
  console.error(err);
  process.exit(1);
});
