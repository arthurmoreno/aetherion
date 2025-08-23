from aetherion import TerrainStorage


def test_set_get_main_type():
    ts = TerrainStorage()
    ts.initialize()
    ts.set_terrain_main_type(1, 2, 3, 7)
    assert ts.get_terrain_main_type(1, 2, 3) == 7


def test_set_get_matter():
    ts = TerrainStorage()
    ts.initialize()
    ts.set_terrain_matter(0, 0, 0, 42)
    ts.set_terrain_water_matter(0, 0, 0, 11)
    ts.set_terrain_vapor_matter(0, 0, 0, 5)
    ts.set_terrain_biomass_matter(0, 0, 0, 3)
    assert ts.get_terrain_matter(0, 0, 0) == 42
    assert ts.get_terrain_water_matter(0, 0, 0) == 11
    assert ts.get_terrain_vapor_matter(0, 0, 0) == 5
    assert ts.get_terrain_biomass_matter(0, 0, 0) == 3


def test_set_get_flags_and_capacity():
    ts = TerrainStorage()
    ts.initialize()
    ts.set_flag_bits(2, 2, 2, 0xABCD)
    assert ts.get_flag_bits(2, 2, 2) == 0xABCD
    ts.set_terrain_max_load_capacity(2, 2, 2, 123)
    assert ts.get_terrain_max_load_capacity(2, 2, 2) == 123


def test_defaults_return_backgrounds():
    ts = TerrainStorage()
    ts.initialize()
    # Default backgrounds
    x, y, z = 0, 0, 0
    assert ts.get_terrain_main_type(x, y, z) == 0
    assert ts.get_terrain_sub_type0(x, y, z) == 0
    assert ts.get_terrain_sub_type1(x, y, z) == -1
    assert ts.get_terrain_matter(x, y, z) == 0
    assert ts.get_terrain_water_matter(x, y, z) == 0
    assert ts.get_terrain_vapor_matter(x, y, z) == 0
    assert ts.get_terrain_biomass_matter(x, y, z) == 0
    assert ts.get_terrain_mass(x, y, z) == 0
    assert ts.get_terrain_max_speed(x, y, z) == 0
    assert ts.get_terrain_min_speed(x, y, z) == 0
    assert ts.get_flag_bits(x, y, z) == 0
    assert ts.get_terrain_max_load_capacity(x, y, z) == 0
    # Activity defaults to inactive
    assert ts.is_active(x, y, z) is False


def test_set_get_entity_subtypes():
    ts = TerrainStorage()
    ts.initialize()
    x, y, z = 4, 5, 6
    ts.set_terrain_sub_type0(x, y, z, 21)
    ts.set_terrain_sub_type1(x, y, z, 22)
    assert ts.get_terrain_sub_type0(x, y, z) == 21
    assert ts.get_terrain_sub_type1(x, y, z) == 22


def test_set_get_physics():
    ts = TerrainStorage()
    ts.initialize()
    x, y, z = 2, -3, 4
    ts.set_terrain_mass(x, y, z, 55)
    ts.set_terrain_max_speed(x, y, z, 12)
    ts.set_terrain_min_speed(x, y, z, 1)
    assert ts.get_terrain_mass(x, y, z) == 55
    assert ts.get_terrain_max_speed(x, y, z) == 12
    assert ts.get_terrain_min_speed(x, y, z) == 1


def test_prune_builds_active_mask_and_is_active():
    ts = TerrainStorage()
    ts.initialize()
    # Touch a few different grids to ensure union logic in prune()
    a = (0, 0, 1)
    b = (0, 1, 0)
    c = (1, 0, 0)
    ts.set_flag_bits(*a, 0xABCD)
    ts.set_terrain_matter(*b, 10)
    ts.set_terrain_water_matter(*c, 20)
    # Before prune, no activity expected
    assert ts.is_active(*a) is False
    assert ts.is_active(*b) is False
    assert ts.is_active(*c) is False
    # Force prune rebuild (interval is 60, lastPruneTick starts at 0)
    count = ts.prune(60)
    assert isinstance(count, int)
    assert count >= 3
    # After prune, all touched coords should be active
    assert ts.is_active(*a) is True
    assert ts.is_active(*b) is True
    assert ts.is_active(*c) is True


def test_mem_usage_increases_with_writes():
    ts = TerrainStorage()
    ts.initialize()
    base = ts.mem_usage()
    # Populate a small 3x3x3 region across multiple grids
    for x in range(3):
        for y in range(3):
            for z in range(3):
                v = (x + 1) * (y + 2) + z
                ts.set_terrain_main_type(x, y, z, (v % 3))
                ts.set_terrain_sub_type0(x, y, z, v)
                ts.set_terrain_sub_type1(x, y, z, v - 1)
                ts.set_terrain_matter(x, y, z, v * 10)
                ts.set_terrain_water_matter(x, y, z, v * 11)
                ts.set_terrain_vapor_matter(x, y, z, v * 12)
                ts.set_terrain_biomass_matter(x, y, z, v * 13)
                ts.set_terrain_mass(x, y, z, v)
                ts.set_terrain_max_speed(x, y, z, v + 5)
                ts.set_terrain_min_speed(x, y, z, max(0, v - 5))
                ts.set_terrain_max_load_capacity(x, y, z, v * 2)
    after = ts.mem_usage()
    assert after >= base
    assert after - base > 0


def test_apply_transform_is_callable():
    ts = TerrainStorage()
    ts.initialize()
    ts.apply_transform(0.5)
    ts.apply_transform(2.0)
