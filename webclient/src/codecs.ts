// Minimal component codecs for headerless fallback decoding (WorldView/Perception only)

export type ComponentKey = 'ENTITY_TYPE' | 'POSITION';

export interface ComponentCodec<T = any> {
  decode: (u8: Uint8Array, offset: number) => T | undefined;
  byteLength: number;
}

function readI32(u8: Uint8Array, off: number): number | undefined {
  if (off + 4 > u8.byteLength) return undefined;
  return (u8[off] | (u8[off + 1] << 8) | (u8[off + 2] << 16) | (u8[off + 3] << 24)) | 0;
}

const ENTITY_TYPE: ComponentCodec = {
  byteLength: 12,
  decode(u8, offset) {
    const type = readI32(u8, offset);
    const sub_type0 = readI32(u8, offset + 4);
    const sub_type1 = readI32(u8, offset + 8);
    if (type === undefined || sub_type0 === undefined || sub_type1 === undefined) return undefined;
    return { type, sub_type0, sub_type1 };
  },
};

const POSITION: ComponentCodec = {
  byteLength: 16,
  decode(u8, offset) {
    const x = readI32(u8, offset);
    const y = readI32(u8, offset + 4);
    const z = readI32(u8, offset + 8);
    const direction = readI32(u8, offset + 12);
    if (x === undefined || y === undefined || z === undefined || direction === undefined) return undefined;
    return { x, y, z, direction };
  },
};

export function buildDefaultCodecs(): Partial<Record<ComponentKey, ComponentCodec>> {
  return { ENTITY_TYPE, POSITION };
}

export default undefined as unknown as never;

