declare module '*.mjs' {
  const factory: (opts?: Record<string, unknown>) => Promise<any>;
  export default factory;
}
