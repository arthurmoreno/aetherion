/**
 * WASM loader for the Aetherion webclient.
 *
 * Exposes a tiny helper to load the embind-generated module from dist/.
 * Consumers can optionally pass a pre-fetched wasm binary to avoid fetch.
 */

export type AetherionWasmModule = {
  EntityInterface: {
    // static
    deserialize(bytes: Uint8Array): any;
  };
  PerceptionResponseFlatB?: new (bytes: Uint8Array) => any;
};

export interface LoadWasmOptions {
  // If provided, the loader will use this module factory (ESM default export)
  factory?: (opts: Record<string, unknown>) => Promise<AetherionWasmModule>;
  // If provided, pass wasm bytes to the factory to avoid network fetch.
  wasmBinary?: ArrayBufferView | ArrayBuffer;
}

let cachedModule: AetherionWasmModule | null = null;

export async function loadAetherionWasm(opts: LoadWasmOptions = {}): Promise<AetherionWasmModule> {
  if (cachedModule) return cachedModule;

  // Prefer user-provided factory
  const factory =
    opts.factory || (await import('../aetherion_wasm.mjs')).default;

  const mod = await factory(
    opts.wasmBinary ? { wasmBinary: opts.wasmBinary } : {}
  );
  cachedModule = mod as unknown as AetherionWasmModule;
  try {
    (globalThis as any).__AETHERION_WASM = cachedModule;
  } catch {}
  return cachedModule;
}

export function getCachedAetherionWasm(): AetherionWasmModule | null {
  if (cachedModule) return cachedModule;
  try {
    const g = (globalThis as any).__AETHERION_WASM;
    if (g) return (cachedModule = g as AetherionWasmModule);
  } catch {}
  return null;
}

export function deserializeEntity(bytes: Uint8Array): any {
  return cachedModule?.EntityInterface?.deserialize(bytes);
}

export default loadAetherionWasm;
