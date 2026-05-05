"""
Strategy C TDD: terrain grid must preserve uint32 entity handles across the int32 sign boundary.

RED before Int64Grid: handles with version >= 256 stored as negative int32, returned as < -2.
GREEN after Int64Grid: full uint32 range stored as int64, returned as positive integers.
Sentinel values -2 and -1 are unchanged in both cases.
"""

import pytest

import aetherion

# EnTT v3: [version: 9 bits][index: 23 bits]
# Version 256 → uint32 = 0x80000000 | index → static_cast<int32_t> = negative
INT32_OVERFLOW_VERSION = 256
INT32_OVERFLOW_HANDLE = (INT32_OVERFLOW_VERSION << 23) | 2  # 0x80000002


@pytest.fixture()
def world():
    w = aetherion.World(5, 5, 5)
    w.initialize_voxel_grid()
    yield w


class TestTerrainIdInt64:
    def test_sentinel_none_is_minus_two(self, world):
        """Uninitialized voxel must read back as NONE (-2). Sentinel unchanged after type widening."""
        vg = world.get_voxel_grid()
        assert vg.get_terrain(0, 0, 0) == -2

    def test_sentinel_on_grid_storage_is_minus_one(self, world):
        """ON_GRID_STORAGE sentinel (-1) must survive a set/get round-trip."""
        vg = world.get_voxel_grid()
        vg.set_terrain_id_raw(2, 2, 2, -1)  # test-only binding
        assert vg.get_terrain(2, 2, 2) == -1

    def test_high_version_handle_is_not_corrupted(self, world):
        """
        Entity handle at version=256 (0x80000002) must not come back as < -2.

        With Int32Grid: stored as -2147483646 → get_terrain returns -2147483646 → FAIL
        With Int64Grid: stored as  2147483650 → get_terrain returns  2147483650 → PASS
        """
        vg = world.get_voxel_grid()
        vg.set_terrain_id_raw(2, 2, 2, INT32_OVERFLOW_HANDLE)
        tid = vg.get_terrain(2, 2, 2)
        assert tid >= -2, (
            f"Entity handle 0x{INT32_OVERFLOW_HANDLE:08X} (version=256) "
            f"corrupted: got {tid}. Expected >= -2. Int64Grid storage required."
        )

    def test_version_sweep_across_int32_boundary(self, world):
        """
        Write entity handles for versions 0–270 at the same voxel.
        Every read-back must be >= -2, including the version=256 sign-flip zone.
        """
        vg = world.get_voxel_grid()
        INDEX = 2
        for version in range(271):
            handle = (version << 23) | INDEX
            vg.set_terrain_id_raw(2, 2, 2, handle)
            tid = vg.get_terrain(2, 2, 2)
            assert tid >= -2, (
                f"Corrupted at version={version}, handle=0x{handle:08X}: got {tid} "
                f"(int32 overflow expected at version=256)"
            )

    def test_max_entt_v3_version_handle_survives(self, world):
        """Handle at max EnTT v3 version (511) must not be corrupted."""
        vg = world.get_voxel_grid()
        max_handle = (511 << 23) | 2  # 0xFF800002
        vg.set_terrain_id_raw(2, 2, 2, max_handle)
        tid = vg.get_terrain(2, 2, 2)
        assert tid >= -2, f"Max-version handle 0x{max_handle:08X} corrupted: {tid}"
