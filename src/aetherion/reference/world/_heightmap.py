"""Deterministic heightmap and gradient helpers for reference world factories.

The rogue dungeon demo uses Perlin noise (``noise``) and SciPy; the reference
package keeps only NumPy so tests do not require those optional dependencies.
"""

from __future__ import annotations

import numpy as np


def generate_heightmap(world_width: int, world_height: int, world_depth: int) -> np.ndarray:
    """Return a smooth 2D height field in ``[0, world_depth - 1]``.

    Uses a planar ramp so gradient descent is well-defined without external libs.
    """
    max_z = float(max(world_depth - 1, 1))
    xs = np.linspace(0.0, 1.0, world_width, dtype=np.float64)[:, np.newaxis]
    ys = np.linspace(0.0, 1.0, world_height, dtype=np.float64)[np.newaxis, :]
    heightmap = (xs + ys) * 0.5 * max_z
    return np.clip(heightmap, 0.0, max_z)


def compute_gradient_descent(heightmap: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    heightmap = heightmap.astype(np.float64)
    gx, gy = np.gradient(heightmap)
    descent_gx = -gx
    descent_gy = -gy
    magnitude = np.sqrt(descent_gx**2 + descent_gy**2)
    descent_gx_norm = descent_gx / (magnitude + 1e-8)
    descent_gy_norm = descent_gy / (magnitude + 1e-8)
    return descent_gx_norm, descent_gy_norm, magnitude


def fill_missing_vectors(descent_gx: np.ndarray, descent_gy: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    rows, cols = descent_gx.shape
    filled_mask = ~(np.isnan(descent_gx) | (descent_gx == 0.0))

    while not np.all(filled_mask):
        updated = False
        new_gx = descent_gx.copy()
        new_gy = descent_gy.copy()

        for i in range(rows):
            for j in range(cols):
                if not filled_mask[i, j]:
                    neighbors: list[tuple[float, float]] = []
                    for di, dj in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
                        ni, nj = i + di, j + dj
                        if 0 <= ni < rows and 0 <= nj < cols and filled_mask[ni, nj]:
                            neighbors.append((descent_gx[ni, nj], descent_gy[ni, nj]))

                    if neighbors:
                        avg_gx = float(np.mean([vec[0] for vec in neighbors]))
                        avg_gy = float(np.mean([vec[1] for vec in neighbors]))
                        new_gx[i, j] = avg_gx
                        new_gy[i, j] = avg_gy
                        filled_mask[i, j] = True
                        updated = True

        descent_gx, descent_gy = new_gx, new_gy
        filled_mask = ~(np.isnan(descent_gx) | (descent_gx == 0.0))

        if not updated:
            break

    return descent_gx, descent_gy
