// Thin WASM wrapper: make the Embind EntityInterface the single source of truth.
// This module exposes helpers to access the WASM-bound class from TypeScript.

import loadAetherionWasm, { type AetherionWasmModule } from './wasm/index.js';

// Promise-returning getter for the embind class constructor
export async function getEntityInterfaceCtor(): Promise<
  AetherionWasmModule['EntityInterface']
> {
  const mod = await loadAetherionWasm();
  return mod.EntityInterface;
}

// Convenience factory to construct a new instance
export async function newEntityInterface(): Promise<any> {
  const Ctor = await getEntityInterfaceCtor();
  return new (Ctor as any)();
}

// Convenience static-call wrapper for deserialize(bytes)
export async function deserializeEntity(bytes: Uint8Array): Promise<any> {
  const Ctor = await getEntityInterfaceCtor();
  return (Ctor as any).deserialize(bytes);
}

// Type-only alias for consumers who want a nominal name.
// At runtime, the class lives in WASM, so this is `any`.
export type EntityInterface = any;

export default undefined as unknown as never;
