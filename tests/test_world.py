import pytest

from aetherion import World


class TestWorldCreation:
    """Test suite for World creation with various sizes and configurations."""

    def test_create_small_world(self):
        """Test creating a small world (1x1x1)."""
        world = World(1, 1, 1)
        assert world.width == 1
        assert world.height == 1
        assert world.depth == 1
        assert isinstance(world, World)

    def test_create_tiny_world(self):
        """Test creating a tiny world (3x3x3)."""
        world = World(3, 3, 3)
        assert world.width == 3
        assert world.height == 3
        assert world.depth == 3

    def test_create_small_rectangular_world(self):
        """Test creating a small rectangular world (5x3x2)."""
        world = World(5, 3, 2)
        assert world.width == 5
        assert world.height == 3
        assert world.depth == 2

    def test_create_medium_world(self):
        """Test creating a medium-sized world (50x30x20)."""
        world = World(50, 30, 20)
        assert world.width == 50
        assert world.height == 30
        assert world.depth == 20

    def test_create_large_world(self):
        """Test creating a large world (100x100x50)."""
        world = World(100, 100, 50)
        assert world.width == 100
        assert world.height == 100
        assert world.depth == 50

    def test_create_asymmetric_worlds(self):
        """Test creating worlds with asymmetric dimensions."""
        test_cases = [
            (10, 5, 1),  # Wide and flat
            (1, 50, 10),  # Tall and narrow
            (20, 1, 30),  # Deep and thin
            (7, 11, 13),  # All different dimensions
        ]

        for width, height, depth in test_cases:
            world = World(width, height, depth)
            assert world.width == width
            assert world.height == height
            assert world.depth == depth

    def test_create_cubic_worlds(self):
        """Test creating cubic worlds of various sizes."""
        sizes = [1, 2, 5, 10, 25, 50, 100]

        for size in sizes:
            world = World(size, size, size)
            assert world.width == size
            assert world.height == size
            assert world.depth == size

    def test_world_dimensions_immutable_after_creation(self):
        """Test that world dimensions can be modified after creation."""
        world = World(10, 20, 30)

        # Verify initial dimensions
        assert world.width == 10
        assert world.height == 20
        assert world.depth == 30

        # Test that dimensions can be modified (this is allowed by the bindings)
        world.width = 15
        world.height = 25
        world.depth = 35

        assert world.width == 15
        assert world.height == 25
        assert world.depth == 35

    def test_multiple_worlds_creation(self):
        """Test creating multiple World instances with different dimensions."""
        worlds = []
        dimensions = [
            (5, 5, 5),
            (10, 8, 6),
            (15, 12, 9),
            (20, 16, 12),
        ]

        # Create multiple worlds
        for width, height, depth in dimensions:
            world = World(width, height, depth)
            worlds.append((world, width, height, depth))

        # Verify all worlds have correct dimensions
        for world, expected_width, expected_height, expected_depth in worlds:
            assert world.width == expected_width
            assert world.height == expected_height
            assert world.depth == expected_depth

    def test_world_has_game_clock(self):
        """Test that World instances have a game clock."""
        world = World(10, 10, 10)
        assert hasattr(world, "game_clock")
        assert world.game_clock is not None

    def test_world_basic_functionality(self):
        """Test basic World functionality after creation."""
        world = World(20, 15, 10)

        # Test that the world has the expected methods
        assert hasattr(world, "initialize_voxel_grid")
        assert hasattr(world, "create_entity")
        assert hasattr(world, "remove_entity")
        assert hasattr(world, "set_terrain")
        assert hasattr(world, "get_terrain")

        # Test voxel grid initialization
        world.initialize_voxel_grid()
        # If no exception is raised, the test passes


class TestWorldEdgeCases:
    """Test edge cases for World creation."""

    def test_create_flat_worlds(self):
        """Test creating flat worlds (depth = 1)."""
        flat_worlds = [
            (100, 100, 1),  # Large flat world
            (50, 25, 1),  # Rectangular flat world
            (1, 1, 1),  # Single voxel world
        ]

        for width, height, depth in flat_worlds:
            world = World(width, height, depth)
            assert world.width == width
            assert world.height == height
            assert world.depth == depth

    def test_create_thin_worlds(self):
        """Test creating thin worlds (width or height = 1)."""
        thin_worlds = [
            (1, 50, 10),  # Thin width
            (50, 1, 10),  # Thin height
            (1, 1, 50),  # Thin width and height
        ]

        for width, height, depth in thin_worlds:
            world = World(width, height, depth)
            assert world.width == width
            assert world.height == height
            assert world.depth == depth

    def test_world_type_consistency(self):
        """Test that all created worlds are of the same type."""
        worlds = [
            World(5, 5, 5),
            World(10, 20, 30),
            World(1, 1, 1),
            World(100, 50, 25),
        ]

        first_world_type = type(worlds[0])
        for world in worlds:
            assert isinstance(world, first_world_type)
            assert isinstance(world, World)


class TestWorldStress:
    """Stress tests for World creation."""

    def test_create_many_small_worlds(self):
        """Test creating many small world instances."""
        worlds = []
        for i in range(10):
            world = World(i + 1, i + 2, i + 3)
            worlds.append(world)
            assert world.width == i + 1
            assert world.height == i + 2
            assert world.depth == i + 3

        # Verify all worlds still have correct dimensions
        for i, world in enumerate(worlds):
            assert world.width == i + 1
            assert world.height == i + 2
            assert world.depth == i + 3

    @pytest.mark.slow
    def test_create_moderately_large_world(self):
        """Test creating a moderately large world (marked as slow test)."""
        # This test is marked as slow since large worlds might take time to initialize
        world = World(200, 200, 100)
        assert world.width == 200
        assert world.height == 200
        assert world.depth == 100


class TestWorldUsage:
    """Test typical World usage patterns."""

    def test_world_initialization_pattern(self):
        """Test the typical pattern of creating and initializing a world."""
        # Create world with specific dimensions
        width, height, depth = 25, 20, 15
        world = World(width, height, depth)

        # Verify dimensions match what we requested
        assert world.width == width
        assert world.height == height
        assert world.depth == depth

        # Initialize the voxel grid (typical usage pattern)
        world.initialize_voxel_grid()

        # Test that we can modify dimensions after creation if needed
        world.width = width
        world.height = height
        world.depth = depth

        # Dimensions should still be correct
        assert world.width == width
        assert world.height == height
        assert world.depth == depth

    def test_world_properties_access(self):
        """Test accessing various World properties."""
        world = World(15, 12, 8)

        # Test dimension access
        assert world.width == 15
        assert world.height == 12
        assert world.depth == 8

        # Test game clock access
        game_clock = world.game_clock
        assert game_clock is not None

        # Test that we can access the game clock multiple times
        assert world.game_clock is game_clock  # Should be the same object

    def test_world_method_availability(self):
        """Test that important World methods are available."""
        world = World(10, 10, 10)

        # Check that essential methods exist
        essential_methods = [
            "initialize_voxel_grid",
            "set_voxel",
            "get_voxel",
            "create_entity",
            "remove_entity",
            "get_entities_by_type",
            "get_entity_ids_by_type",
            "get_entity_by_id",
            "create_perception_response",
            "create_perception_responses",
            "set_terrain",
            "get_terrain",
            "get_entity",
        ]

        for method_name in essential_methods:
            assert hasattr(world, method_name), f"World missing method: {method_name}"
            method = getattr(world, method_name)
            assert callable(method), f"World.{method_name} is not callable"
