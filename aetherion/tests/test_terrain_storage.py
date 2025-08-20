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
