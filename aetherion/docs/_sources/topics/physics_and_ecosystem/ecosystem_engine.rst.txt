Ecosystem Engine
================

The ``EcosystemEngine`` simulates complex environmental interactions, focusing on water dynamics and plant life cycles. It employs a parallel processing model to handle large-scale voxel updates efficiently.

Water Simulation
----------------

The water simulation uses a cellular automata approach, processing the world in 3D chunks.

Architecture
~~~~~~~~~~~~

*   **Grid Partitioning**: The world is divided into ``GridBox`` chunks (typically 32x32x32).
*   **GridBoxProcessor**: A dedicated class that processes a single box, calculating water flow, evaporation, and condensation for each voxel.
*   **WaterSimulationManager**: Orchestrates the simulation, managing a thread pool and a priority queue of ``GridBoxTask``s.

Flow Logic
~~~~~~~~~~

1.  **Water Flow**: ``processVoxelWater`` checks neighbors to determine flow direction.
    *   **Downwards**: Primary flow direction due to gravity.
    *   **Sideways**: Secondary flow if the path below is blocked.
2.  **Evaporation**: ``processVoxelEvaporation`` converts water to vapor.
    *   **Triggers**: High ``SunIntensity`` and heat accumulation.
    *   **Action**: Removes water matter and creates vapor entities in the space above.
3.  **Condensation**: Vapor converts back to liquid.
    *   **Triggers**: Hitting a ceiling or reaching saturation.
    *   **Action**: Converts vapor matter to liquid water entities below.

Plant Life Cycle
----------------

The engine simulates biological processes for plant entities.

*   **Photosynthesis**: Calculates energy production based on ``SunIntensity``, water availability, and plant health.
*   **Growth**: Plants consume accumulated energy to:
    *   Grow fruit (``FruitComponent``).
    *   Heal damage (``HealthComponent``).
