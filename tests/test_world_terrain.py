"""
Unit tests for aetherion world terrain functionality.

This test module focuses on testing the core world and terrain storage components:
- World initialization and basic properties
- TerrainStorage operations (get/set terrain types, matter, physics)
- World's terrain API integration with TerrainStorage

Tests verify that terrain data can be stored and retrieved correctly through
the public SDK API without needing direct access to internal ECS registry.
"""

import gc

import pytest

# Import aetherion module - if it fails, tests will be skipped
try:
    from aetherion import TerrainStorage, World

    AETHERION_AVAILABLE = True
    SKIP_REASON = ""
except ImportError as e:
    AETHERION_AVAILABLE = False
    SKIP_REASON = f"aetherion module not available: {e}"


# Skip all tests if aetherion is not available
pytestmark = pytest.mark.skipif(not AETHERION_AVAILABLE, reason=SKIP_REASON)


class TestTerrainStorage:
    """Test TerrainStorage directly to verify basic functionality."""

    def test_terrain_storage_initialization(self):
        """Test that TerrainStorage can be created and initialized."""
        storage = TerrainStorage()
        storage.initialize()

        # Verify default values for an unset voxel
        assert storage.get_terrain_main_type(0, 0, 0) == 0
        assert storage.get_terrain_matter(0, 0, 0) == 0
        assert storage.is_active(0, 0, 0) is False

    def test_terrain_storage_set_get_main_type(self):
        """Test setting and getting terrain main type."""
        storage = TerrainStorage()
        storage.initialize()

        storage.set_terrain_main_type(1, 2, 3, 7)
        assert storage.get_terrain_main_type(1, 2, 3) == 7

    def test_terrain_storage_set_get_matter(self):
        """Test setting and getting various matter types."""
        storage = TerrainStorage()
        storage.initialize()

        # Test all matter types
        storage.set_terrain_matter(5, 5, 5, 42)
        storage.set_terrain_water_matter(5, 5, 5, 11)
        storage.set_terrain_vapor_matter(5, 5, 5, 5)
        storage.set_terrain_biomass_matter(5, 5, 5, 3)

        assert storage.get_terrain_matter(5, 5, 5) == 42
        assert storage.get_terrain_water_matter(5, 5, 5) == 11
        assert storage.get_terrain_vapor_matter(5, 5, 5) == 5
        assert storage.get_terrain_biomass_matter(5, 5, 5) == 3

    def test_terrain_storage_physics_properties(self):
        """Test setting and getting physics properties."""
        storage = TerrainStorage()
        storage.initialize()

        x, y, z = 2, 3, 4
        storage.set_terrain_mass(x, y, z, 55)
        storage.set_terrain_max_speed(x, y, z, 12)
        storage.set_terrain_min_speed(x, y, z, 1)

        assert storage.get_terrain_mass(x, y, z) == 55
        assert storage.get_terrain_max_speed(x, y, z) == 12
        assert storage.get_terrain_min_speed(x, y, z) == 1

    def test_terrain_storage_negative_coordinates(self):
        """Test that TerrainStorage handles negative coordinates."""
        storage = TerrainStorage()
        storage.initialize()

        # Test with negative coordinates
        storage.set_terrain_main_type(-10, -20, -5, 3)
        assert storage.get_terrain_main_type(-10, -20, -5) == 3

    def test_terrain_storage_prune_and_activity(self):
        """Test pruning and activity tracking."""
        storage = TerrainStorage()
        storage.initialize()

        # Touch a few voxels
        coords = [(0, 0, 1), (0, 1, 0), (1, 0, 0)]
        for x, y, z in coords:
            storage.set_terrain_matter(x, y, z, 10)

        # Prune should build active mask
        count = storage.prune(60)
        assert count >= 3

        # Check activity
        for x, y, z in coords:
            assert storage.is_active(x, y, z) is True


class TestWorldAbstractions:
    """Test World class abstractions and basic functionality."""

    def test_world_creation(self):
        """Test creating a World instance."""
        world = World(3, 3, 3)
        assert world is not None
        assert world.width == 3
        assert world.height == 3
        assert world.depth == 3

    def test_world_dimensions_modification(self):
        """Test modifying world dimensions."""
        world = World(3, 3, 3)

        world.width = 10
        world.height = 15
        world.depth = 8

        assert world.width == 10
        assert world.height == 15
        assert world.depth == 8

    def test_world_multiple_instances(self):
        """Test creating multiple independent World instances."""
        world1 = World(3, 3, 3)
        world2 = World(5, 5, 5)

        assert world1.width != world2.width
        assert world1.height != world2.height

        # Modify one, ensure other is unchanged
        world1.width = 10
        assert world2.width == 5

    def test_world_get_terrain(self):
        """Test World's get_terrain method returns terrain type."""
        world = World(10, 10, 10)

        # Get terrain for a position (returns -1 for uninitialized/no terrain)
        terrain_type = world.get_terrain(5, 5, 5)
        assert isinstance(terrain_type, int)
        # -1 indicates no terrain/uninitialized voxel
        assert terrain_type >= -1


class TestWorldTerrainIntegration:
    """Test integration between World and TerrainStorage through World's API."""

    def test_world_terrain_lifecycle(self):
        """Test that World and TerrainStorage can coexist and be cleaned up."""
        world = World(5, 5, 5)
        storage = TerrainStorage()
        storage.initialize()

        # Set some data in storage
        storage.set_terrain_main_type(1, 1, 1, 42)
        assert storage.get_terrain_main_type(1, 1, 1) == 42

        # Delete world first
        del world
        gc.collect()

        # Storage should still work
        assert storage.get_terrain_main_type(1, 1, 1) == 42

    def test_storage_outlives_world(self):
        """Test that storage can outlive the world that was created alongside it."""
        storage = TerrainStorage()
        storage.initialize()
        storage.set_terrain_main_type(5, 5, 5, 99)

        # Create world, use it, delete it
        world = World(10, 10, 10)
        _ = world.get_terrain(5, 5, 5)

        del world
        gc.collect()

        # Storage should still function
        result = storage.get_terrain_main_type(5, 5, 5)
        assert result == 99

    def test_multiple_storages_with_world(self):
        """Test that multiple TerrainStorage instances can coexist with World."""
        world = World(5, 5, 5)

        storages = []
        for i in range(3):
            storage = TerrainStorage()
            storage.initialize()
            storage.set_terrain_main_type(i, i, i, i * 10)
            storages.append(storage)

        # Verify each storage maintains independent data
        for i, storage in enumerate(storages):
            assert storage.get_terrain_main_type(i, i, i) == i * 10

        del world
        gc.collect()

        # All storages should still work
        for i, storage in enumerate(storages):
            assert storage.get_terrain_main_type(i, i, i) == i * 10


class TestMemoryAndPerformance:
    """Tests for memory usage and performance characteristics."""

    def test_storage_memory_usage_increases(self):
        """Test that memory usage increases when terrain data is written."""
        storage = TerrainStorage()
        storage.initialize()

        base_mem = storage.mem_usage()

        # Write a bunch of terrain data
        for x in range(10):
            for y in range(10):
                for z in range(3):
                    storage.set_terrain_main_type(x, y, z, 1)
                    storage.set_terrain_matter(x, y, z, 10)

        after_mem = storage.mem_usage()
        assert after_mem > base_mem

    def test_terrain_storage_gc_stress(self):
        """Create and destroy TerrainStorage instances in a loop with aggressive GC."""
        for i in range(10):
            storage = TerrainStorage()
            storage.initialize()
            storage.set_terrain_main_type(i % 3, i % 3, i % 3, i)

            # Aggressive cleanup
            del storage
            gc.collect()

    def test_world_and_storage_gc_stress(self):
        """Create and destroy World and TerrainStorage in a loop."""
        for i in range(10):
            world = World(3, 3, 3)
            storage = TerrainStorage()
            storage.initialize()

            storage.set_terrain_main_type(i, i, i, i * 5)
            _ = world.get_terrain(i % 3, i % 3, i % 3)

            del world
            del storage
            gc.collect()


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
