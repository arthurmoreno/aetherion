#!/usr/bin/env python3
"""
Generate a PerceptionResponse flatbuffer using the Python Aetherion package so the
webclient can validate cross-language deserialization. Writes bytes to:
  tests/fixtures/perception.bin

Run from aetherion/webclient directory:
  python3 tests/gen_perception_bytes.py
"""

import os
import sys
import zipfile


def ensure_repo_paths():
    # Add repo root and aetherion/src to sys.path so `import aetherion` resolves
    here = os.path.dirname(__file__)
    pkg_root = os.path.abspath(os.path.join(here, ".."))
    repo_root = os.path.abspath(os.path.join(pkg_root, "..", ".."))
    aetherion_dir = os.path.join(repo_root, "aetherion")
    aetherion_src = os.path.join(aetherion_dir, "src")
    aetherion_site = os.path.join(aetherion_dir, "site-packages")
    # Try also adding an unpacked wheel, since binary extensions cannot be imported from zip
    whl_path = None
    unpack_dir = None
    try:
        for name in os.listdir(aetherion_dir):
            if name.endswith(".whl") and name.startswith("aetherion-"):
                whl_path = os.path.join(aetherion_dir, name)
                break
        if whl_path:
            # Only attempt unpack if our interpreter matches the wheel's cp tag
            py_tag = f"cp{sys.version_info.major}{sys.version_info.minor}"
            if py_tag in os.path.basename(whl_path):
                unpack_dir = os.path.join(aetherion_dir, ".wheel_unpacked", py_tag)
                os.makedirs(unpack_dir, exist_ok=True)
                # Extract once if directory empty
                if not os.listdir(unpack_dir):
                    with zipfile.ZipFile(whl_path, "r") as zf:
                        zf.extractall(unpack_dir)
    except Exception:
        unpack_dir = None

    for p in filter(None, [repo_root, aetherion_dir, aetherion_src, aetherion_site, unpack_dir]):
        if p not in sys.path:
            sys.path.insert(0, p)


def main():
    try:
        from aetherion import (
            DirectionEnum,
            EntityEnum,
            EntityInterface,
            EntityTypeComponent,
            PerceptionResponse,
            Position,
            TerrainEnum,
            WorldView,
        )
    except Exception as e:  # pragma: no cover
        py_tag = f"cp{sys.version_info.major}{sys.version_info.minor}"
        msg = (
            f"Failed to import aetherion: {e}\n"
            f"- Active Python: {sys.executable} (tag {py_tag})\n"
            f"- Tip: Use Python 3.12 and ensure the wheel is installed or unpacked.\n"
            f"  e.g., 'conda activate aetherion-312' or set PYTHON=/path/to/python3.12\n"
        )
        raise SystemExit(msg)

    # Build a simple world and two entities like in the Python tests
    entity = EntityInterface()
    entity.set_entity_id(2)

    world_view = WorldView()
    world_view.voxelGridView.initVoxelGridView(3, 3, 3, 0, 0, 0)

    entity1 = EntityInterface()
    entity1.set_entity_id(1)
    et1 = EntityTypeComponent()
    et1.main_type = EntityEnum.TERRAIN.value
    et1.sub_type0 = TerrainEnum.GRASS.value
    et1.sub_type1 = 0  # Terrain variation type
    entity1.set_entity_type(et1)

    p1 = Position()
    p1.x = 10
    p1.y = 20
    p1.z = 30
    p1.direction = DirectionEnum.DOWN
    entity1.set_position(p1)
    world_view.entities[1] = entity1

    entity2 = EntityInterface()
    entity2.set_entity_id(2)

    et2 = EntityTypeComponent()
    et2.main_type = EntityEnum.TERRAIN.value
    et2.sub_type0 = TerrainEnum.GRASS.value
    et2.sub_type1 = 0  # Terrain variation type
    entity2.set_entity_type(et2)

    p2 = Position()
    p2.x = 15
    p2.y = 25
    p2.z = 35
    p2.direction = DirectionEnum.UP
    entity2.set_position(p2)
    world_view.entities[2] = entity2

    fb_bytes = PerceptionResponse(entity, world_view).serialize_flatbuffer()

    # Quick sanity check using Python side, to ensure worldView is present
    try:
        from aetherion import PerceptionResponseFlatB as PyPR  # type: ignore

        pypr = PyPR(fb_bytes)
        wv = pypr.getWorldView()
        if wv is None:
            raise RuntimeError("Python sanity check: worldView is None in serialized bytes")
    except Exception as e:
        # Don't abort generation, but print context to stderr
        sys.stderr.write(f"[warn] Python PR sanity check failed: {e}\n")

    out_dir = os.path.join(os.path.dirname(__file__), "fixtures")
    os.makedirs(out_dir, exist_ok=True)
    out_file = os.path.join(out_dir, "perception.bin")
    with open(out_file, "wb") as f:
        f.write(bytes(fb_bytes))
    print(f"Wrote {len(fb_bytes)} bytes to {out_file}")


if __name__ == "__main__":
    main()
