import { VoxelGridView as FBVoxelGridView } from './game-engine/voxel-grid-view.js';

export type VoxelBounds = {
  xOffset: number;
  yOffset: number;
  zOffset: number;
  voxelWidth: number;
  voxelHeight: number;
  voxelDepth: number;
};

export class VoxelGridViewFlatB {
  private fb: FBVoxelGridView;

  private constructor(fb: FBVoxelGridView) {
    this.fb = fb;
  }

  static fromFlatBuffer(fb: FBVoxelGridView | null): VoxelGridViewFlatB | null {
    if (!fb) return null;
    return new VoxelGridViewFlatB(fb);
  }

  // Dimensions and offsets
  get width(): number { return this.fb.width?.() ?? 0; }
  get height(): number { return this.fb.height?.() ?? 0; }
  get depth(): number { return this.fb.depth?.() ?? 0; }
  get xOffset(): number { return this.fb.xOffset?.() ?? 0; }
  get yOffset(): number { return this.fb.yOffset?.() ?? 0; }
  get zOffset(): number { return this.fb.zOffset?.() ?? 0; }

  get bounds(): VoxelBounds {
    return {
      xOffset: this.xOffset,
      yOffset: this.yOffset,
      zOffset: this.zOffset,
      voxelWidth: this.fb.width?.() ?? 0,
      voxelHeight: this.fb.height?.() ?? 0,
      voxelDepth: this.fb.depth?.() ?? 0,
    };
  }

  // Raw arrays
  get terrainData(): Int32Array | null { return this.fb.terrainDataArray?.() ?? null; }
  get entityData(): Int32Array | null { return this.fb.entityDataArray?.() ?? null; }

  // Index helpers (world space -> local linear index)
  voxelIndex(x: number, y: number, z: number): number {
    const lx = x - this.xOffset;
    const ly = y - this.yOffset;
    const lz = z - this.zOffset;
    const w = this.width, h = this.height, d = this.depth;
    if (lx < 0 || ly < 0 || lz < 0 || lx >= w || ly >= h || lz >= d) return -1;
    return (lz * h + ly) * w + lx; // equivalent to lx + ly*w + lz*w*h
  }

  // Existence checks: 0 means empty in-bounds, -1 means OOB; only > 0 is present
  check_if_terrain_exist(x: number, y: number, z: number): boolean {
    const arr = this.terrainData;
    if (!arr) return false;
    const idx = this.voxelIndex(x, y, z);
    return idx >= 0 && idx < arr.length && arr[idx] > 0;
  }

  check_if_entity_exist(x: number, y: number, z: number): boolean {
    const arr = this.entityData;
    if (!arr) return false;
    const idx = this.voxelIndex(x, y, z);
    return idx >= 0 && idx < arr.length && arr[idx] > 0;
  }

  get_voxel_terrain_id(x: number, y: number, z: number): number {
    const arr = this.terrainData;
    if (!arr) return -1;
    const idx = this.voxelIndex(x, y, z);
    if (idx < 0 || idx >= arr.length) return -1;
    return arr[idx];
  }

  get_voxel_entity_id(x: number, y: number, z: number): number {
    const arr = this.entityData;
    if (!arr) return -1;
    const idx = this.voxelIndex(x, y, z);
    if (idx < 0 || idx >= arr.length) return -1;
    return arr[idx];
  }
}

export default VoxelGridViewFlatB;

