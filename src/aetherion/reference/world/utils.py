def trapezium_column_top_z(x: int, width: int, depth: int) -> int:
    """Highest filled z index for column ``x`` in the left-to-right trapezium mountain.

    Column ``x=0`` is tallest (``depth - 1``); height decreases monotonically toward ``+x``.
    """
    if depth <= 0:
        return -1
    if width <= 1:
        return max(0, depth - 1)
    numer = (depth - 1) * (width - 1 - x)
    den = width - 1
    return max(0, min(depth - 1, numer // den))


def mountain_ridge_source_xyz(width: int, height: int, depth: int) -> tuple[int, int, int]:
    """Ridge cell for the river source: left column, middle ``y``, top surface ``z``."""
    x = 5
    y = (height // 2) if height > 0 else 0
    z = trapezium_column_top_z(x, width, depth)
    return x, y, z
