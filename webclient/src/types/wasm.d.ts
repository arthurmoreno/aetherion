declare module '../../dist/aetherion_wasm.mjs' {
  const factory: (opts?: Record<string, unknown>) => Promise<any>;
  export default factory;
}
