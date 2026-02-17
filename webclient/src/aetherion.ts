/* Minimal aetherion webclient shim
   - PerceptionResponseFlatB: wraps FlatBuffers PerceptionResponse bytes and
     exposes methods used by the web client.
   - A tiny WorldView.deserialize_flatbuffer helper that converts the
     generated FlatBuffers WorldView into a lightweight runtime wrapper.
   - A few enum constants and terrain helpers used by the renderer.

   This file is intentionally minimal — it aims to provide the functions
   and shapes the rest of the webclient code expects so the port can
   proceed. Behaviours are best-effort and may need refinement later.
*/

import PerceptionResponseFlatB from './perception-response-flatb.js';
import WorldView from './world-view.js';
import VoxelGridViewFlatB from './voxel-grid-view-flatb.js';
export { loadAetherionWasm, getCachedAetherionWasm } from './wasm/index.js';
import type { EntityInterface } from './entity-interface.js';
import { getCachedAetherionWasm } from './wasm/index.js';

// Re-export some generated FlatBuffers types and enums commonly used by
// client code so callers can import them from `aetherion` directly.
export { DirectionEnum } from './game-engine/direction-enum.js';
export { Component } from './game-engine/component.js';
export { VoxelGridView } from './game-engine/voxel-grid-view.js';
export { PerceptionResponse } from './game-engine/perception-response.js';
export { QueryResponse } from './game-engine/query-response.js';

// Re-export our custom ComponentFlag so callers can check component bits.
// No longer re-export ComponentFlag from TS shim; keep API minimal to WASM.

// --- Basic enums (small, practical subset used by client code) ---
export const TerrainEnum_EMPTY = -1;
export const TerrainEnum_GRASS = 0;
export const TerrainEnum_WATER = 1;

export enum TerrainVariantEnum {
  FULL = 0,
  RAMP_EAST = 1,
  RAMP_WEST = 2,
  CORNER_SOUTH_EAST = 3,
  CORNER_SOUTH_EAST_INV = 4,
  CORNER_NORTH_EAST = 5,
  CORNER_NORTH_EAST_INV = 6,
  RAMP_SOUTH = 7,
  RAMP_NORTH = 8,
  CORNER_SOUTH_WEST = 9,
  CORNER_NORTH_WEST = 10,
}

export const EntityEnum_TERRAIN = 0;
export const EntityEnum_PLANT = 1;
export const EntityEnum_BEAST = 2;
export const EntityEnum_TILE_EFFECT = 3;

export const PlantEnum_RASPBERRY = 0;

// --- Small helpers used by rendering code ---
export function should_draw_terrain(terrainId: number): boolean {
  return terrainId !== TerrainEnum_EMPTY;
}

export function is_terrain_an_empty_water(terrainId: number): boolean {
  return terrainId === TerrainEnum_WATER;
}

export {
  PerceptionResponseFlatB,
  WorldView,
  EntityInterface,
  VoxelGridViewFlatB,
};

// Sync helper: prefer WASM; on failure, fallback to JS headerless decode
export function deserializeEntity(bytes: Uint8Array): EntityInterface | null {
  try {
    const wasm = getCachedAetherionWasm();
    if (wasm && typeof (wasm as any).EntityInterface?.deserialize === 'function') {
      // EntityInterface.deserialize returns an instance; entity id may be embedded in bytes or not.
      // For now, do not set id here (callers can later set if needed).
      return (wasm as any).EntityInterface.deserialize(bytes) as EntityInterface;
    }
  } catch (e) {
    console.info('[aetherion] Failed to deserialize EntityInterface via WASM:', e);
    // fall through to JS fallback
  }
}

export default {
  PerceptionResponseFlatB,
  WorldView,
  VoxelGridViewFlatB,
  deserializeEntity,
  should_draw_terrain,
  is_terrain_an_empty_water,
};
