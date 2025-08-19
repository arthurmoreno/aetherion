import { WorldView as FBWorldView } from './game-engine/world-view.js';
import { VoxelGridView as FBVoxelGridView } from './game-engine/voxel-grid-view.js';
import { getCachedAetherionWasm, loadAetherionWasm } from './wasm/index.js';

// Debug flag: when true, break early after decoding the first entity to verify decode pipeline
// You can toggle at runtime via: window.LIFE_SIM_EARLY_ENTITY_DECODE = true/false
const EARLY_DECODE_BREAK: boolean = (globalThis as any)?.LIFE_SIM_EARLY_ENTITY_DECODE ?? false;

export const WorldView = {
  async deserialize_flatbuffer(fbWorldView: FBWorldView | null) {
    if (!fbWorldView) return null;

    const voxel = fbWorldView.voxelGrid ? fbWorldView.voxelGrid() : null;
    const width = fbWorldView.width ? fbWorldView.width() : 0;
    const height = fbWorldView.height ? fbWorldView.height() : 0;
    const depth = fbWorldView.depth ? fbWorldView.depth() : 0;

    const voxelWidth = voxel ? voxel.width() : 0;
    const voxelHeight = voxel ? voxel.height() : 0;
    const voxelDepth = voxel ? voxel.depth() : 0;
    const xOffset = voxel ? voxel.xOffset() : 0;
    const yOffset = voxel ? voxel.yOffset() : 0;
    const zOffset = voxel ? voxel.zOffset() : 0;

    const terrainData: Int32Array | null = voxel ? voxel.terrainDataArray() : null;
    const entityData: Int32Array | null = voxel ? voxel.entityDataArray() : null;

  // Build an ID -> EntityInterface (WASM) cache once per WorldView, mirroring C++ WorldView::entities
    const entityById = new Map<number, any>();
  let disposed = false;
    const entitiesLen = fbWorldView.entitiesLength ? fbWorldView.entitiesLength() : 0;
    let wasm = getCachedAetherionWasm();
    if (!wasm) {
      try { wasm = await loadAetherionWasm(); } catch { /* no-op */ }
    }
    for (let i = 0; i < entitiesLen; i++) {
      const e = fbWorldView.entities(i);
      if (!e) continue;
      const id = e.entityId ? e.entityId() : 0;
      const bytes = e.entityDataArray ? e.entityDataArray() : null;
      if (!bytes) continue;
      try {
        if (wasm && typeof (wasm as any).EntityInterface?.deserialize === 'function') {
          // Prefer the real C++ struct_pack path via WASM
          const ent = (wasm as any).EntityInterface.deserialize(bytes);
          if (typeof ent?.set_entity_id === 'function') ent.set_entity_id(id);
          entityById.set(id, ent);
          // console.info(`[WorldView] Entity #${id} decoded via WASM`, ent);
        } else {}

        if (EARLY_DECODE_BREAK) {
          try {
            console.info('[WorldView] Early entity decode OK', {
              id,
              bytesLen: (bytes as any)?.byteLength ?? (bytes as any)?.length,
              hasComponent: typeof (entityById.get(id)?.has_component) === 'function',
            });
          } catch { }
          break; // break early to validate decode path
        }
      } catch (e) {
        // Skip malformed entries; continue (but log when in debug mode)
        try { console.info('[WorldView] Entity decode failed', { id, error: (e as Error)?.message }); } catch { }
      }
    }

    function voxelIndex(x: number, y: number, z: number) {
      const lx = x - xOffset;
      const ly = y - yOffset;
      const lz = z - zOffset;
      if (lx < 0 || ly < 0 || lz < 0) return -1;
      if (!voxel) return -1;
      if (lx >= voxelWidth || ly >= voxelHeight || lz >= voxelDepth) return -1;
      return (lz * voxelHeight + ly) * voxelWidth + lx;
    }

  return {
      width,
      height,
      depth,
      // Bounds of the voxel sub-grid within world coordinates
      get_voxel_bounds() {
        return {
          xOffset,
          yOffset,
          zOffset,
          voxelWidth,
          voxelHeight,
          voxelDepth,
          worldWidth: width,
          worldHeight: height,
          worldDepth: depth,
        } as const;
      },
      check_if_terrain_exist(x: number, y: number, z: number) {
        if (!voxel || !terrainData) return false;
        const idx = voxelIndex(x, y, z);
        // In-bounds empty tiles are 0; only ids > 0 are present. -1 is OOB sentinel.
        return idx >= 0 && idx < terrainData.length && terrainData[idx] > 0;
      },
      get_terrain(x: number, y: number, z: number) {
        if (!terrainData) return null;
        const idx = voxelIndex(x, y, z);
        if (idx < 0 || idx >= terrainData.length) return null;
        const entId = terrainData[idx];
        if (entId <= 0) return null;
        return entityById.get(entId) ?? null;
      },
      check_if_entity_exist(x: number, y: number, z: number) {
        if (!entityData) return false;
        const idx = voxelIndex(x, y, z);
        return idx >= 0 && idx < entityData.length && entityData[idx] > 0;
      },
      get_entity(x: number, y: number, z: number) {
        if (!entityData) return null;
        const idx = voxelIndex(x, y, z);
        if (idx < 0 || idx >= entityData.length) return null;
        const entId = entityData[idx];
        if (entId <= 0) return null;
        return entityById.get(entId) ?? null;
      },
      // Helper to mirror C++ pyGetEntityById
  get_entity_by_id(id: number) {
        return entityById.get(id) ?? null;
      },
      // Helpers to access raw voxel IDs when needed (debugging/perf)
      get_voxel_entity_id(x: number, y: number, z: number) {
        if (!entityData) return -1;
        const idx = voxelIndex(x, y, z);
        if (idx < 0 || idx >= entityData.length) return -1;
        return entityData[idx];
      },
      get_voxel_terrain_id(x: number, y: number, z: number) {
        if (!terrainData) return -1;
        const idx = voxelIndex(x, y, z);
        if (idx < 0 || idx >= terrainData.length) return -1;
        return terrainData[idx];
      },
      // Explicitly free all WASM-backed EntityInterface instances and clear references
      delete() {
        if (disposed) return;
        disposed = true;
        try {
          for (const ent of entityById.values()) {
            try {
              if (ent && typeof (ent as any).delete === 'function') {
                (ent as any).delete();
              }
            } catch { /* ignore individual delete failures */ }
          }
        } finally {
          entityById.clear();
          // Help GC: drop the raw FB object reference on this result object
          try { (this as any)._raw = null; } catch { /* ignore */ }
        }
      },
      // Alias: dispose() for ergonomics
      dispose() { (this as any).delete(); },
  _raw: fbWorldView,
    } as const;
  },
};

export default WorldView;
