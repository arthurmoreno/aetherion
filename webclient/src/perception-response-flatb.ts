import * as flatbuffers from 'flatbuffers';
import { PerceptionResponse } from './game-engine/perception-response.js';
import { EntityInterface as FBEntityInterface } from './game-engine/entity-interface.js';
import type { EntityInterface } from './entity-interface.js';
import { getCachedAetherionWasm, loadAetherionWasm } from './wasm/index.js';
import { buildDefaultCodecs } from './codecs.js';

export class PerceptionResponseFlatB {
  private _bb: flatbuffers.ByteBuffer;
  private _fb: PerceptionResponse;
  private _raw: Uint8Array;
  private _validated = false;

  constructor(data: ArrayBuffer | Uint8Array) {
    const u8 = data instanceof Uint8Array ? data : new Uint8Array(data);
    this._raw = u8;
    this._bb = new flatbuffers.ByteBuffer(u8);
    // Optimistically assume size-prefixed (some producers do that)
    try {
      this._fb = PerceptionResponse.getSizePrefixedRootAsPerceptionResponse(this._bb);
    } catch {
      this._bb.setPosition(0);
      this._fb = PerceptionResponse.getRootAsPerceptionResponse(this._bb);
    }
  }

  private ensureValid(): void {
    if (this._validated) return;
    // Heuristic: if both entity and worldView are absent, try alternative root
    const hasEntity = !!this._fb.entity?.();
    const hasWV = !!this._fb.worldView?.();
    if (!hasEntity && !hasWV) {
      try {
        const bb2 = new flatbuffers.ByteBuffer(this._raw);
        const alt = PerceptionResponse.getRootAsPerceptionResponse(bb2);
        const altHasEntity = !!alt.entity?.();
        const altHasWV = !!alt.worldView?.();
        if (altHasEntity || altHasWV) {
          this._bb = bb2;
          this._fb = alt;
        }
      } catch {
        // keep original
      }
    }
    this._validated = true;
  }

  getWorldView() {
    this.ensureValid();
    return this._fb.worldView();
  }

  getEntity(): EntityInterface | null {
    this.ensureValid();
    const ent = this._fb.entity();
    if (!ent) return null;
    const bytes = ent.entityDataArray();
    if (!bytes) return null;
    // Prefer the already-loaded WASM module for performance, but catch failures.
    const wasm = getCachedAetherionWasm();
    if (wasm && wasm.EntityInterface && typeof wasm.EntityInterface.deserialize === 'function') {
      try {
        return wasm.EntityInterface.deserialize(bytes) as unknown as EntityInterface;
      } catch (e) {
        try { console.warn('[embind] Deserialize failed:', (e as Error)?.message || e); } catch {}
        // fall through to headerless fallback
      }
    }
    // Headerless minimal fallback to keep camera and world decoding functional
    try {
      const codecs = buildDefaultCodecs();
      const u8 = bytes as unknown as Uint8Array;
      const et = codecs.ENTITY_TYPE?.decode(u8, 0);
      const pos = codecs.POSITION?.decode(u8, 12);
      const id = typeof ent.entityId === 'function' ? ent.entityId() : 0;
      // Avoid `this`-based accessors so TS doesn’t infer an empty `{}` receiver.
      const fallback = {
        _id: id,
        entity_type: et,
        position: pos,
        get_entity_id: () => id,
        get_entity_type: () => et,
        get_position: () => pos,
      } as const;
      return fallback as unknown as EntityInterface;
    } catch {
      // Kick off loading in the background for future attempts; do not block now
      // eslint-disable-next-line @typescript-eslint/no-floating-promises
      loadAetherionWasm().catch(() => {});
      return null;
    }
  }

  get_item_from_inventory_by_id(id: number): EntityInterface | null {
    const len = (this._fb as any).itemsEntitiesLength ? this._fb.itemsEntitiesLength() : 0;
    for (let i = 0; i < len; i++) {
      const e: FBEntityInterface | null = this._fb.itemsEntities(i);
      if (!e) continue;
      if (e.entityId && e.entityId() === id) {
  const bytes = e.entityDataArray();
  if (!bytes) return null;
  const wasm = getCachedAetherionWasm();
  if (wasm && wasm.EntityInterface && typeof wasm.EntityInterface.deserialize === 'function') {
    return wasm.EntityInterface.deserialize(bytes) as unknown as EntityInterface;
  }
  // Trigger background load for future calls.
  // eslint-disable-next-line @typescript-eslint/no-floating-promises
  loadAetherionWasm().catch(() => {});
  return null;
      }
    }
    return null;
  }

  get_query_response_by_id(id: number): Uint8Array | null {
    const len = (this._fb as any).queryResponsesLength ? this._fb.queryResponsesLength() : 0;
    for (let i = 0; i < len; i++) {
      // @ts-ignore
      const qr: any = this._fb.queryResponses(i);
      if (!qr) continue;
      if (qr.queryId && qr.queryId() === id) {
        const d = qr.queryDataArray();
        return d ?? null;
      }
    }
    return null;
  }

  get_ticks(): number {
    try {
      const v: any = (this._fb as any).gameClockTicks ? (this._fb as any).gameClockTicks() : 0n;
      if (typeof v === 'bigint') return Number(v);
      return Number(v);
    } catch (e) {
      return 0;
    }
  }
}

export default PerceptionResponseFlatB;
