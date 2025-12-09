.. _world_simulation:

==================
World Simulation
==================

This section provides a narrative introduction to the physical world of the life simulation game, documenting the mathematical models and physical phenomena that govern the behavior of entities, terrain, and matter. These equations are not theoretical—they are extracted from the active simulation code and represent the actual forces shaping the virtual world.

The Simulated World
===================

The game world is a three-dimensional voxel-based environment where every cubic meter of space can contain terrain, water, vapor, biomass, or entities. Unlike static tile-based games, this simulation implements continuous physics where matter flows, heat transfers, objects fall, and energy transforms according to real physical principles.

**Design Philosophy**: The simulation draws inspiration from Dwarf Fortress's emergent complexity while maintaining real-time performance through careful optimization. Static terrain data lives in sparse OpenVDB grids (memory-efficient), while dynamic behavior activates ECS entities on-demand (computation-efficient).

**Spatial Structure**:

- **Voxel Resolution**: 1 voxel = 1 cubic meter
- **Grid Storage**: Sparse OpenVDB grids store only non-default values
- **Activation Model**: Terrain transitions from "cold storage" (VDB-only) to "hot" (ECS entity) when exhibiting dynamic behavior
- **Thread Safety**: Thread-local accessor caches enable O(1) concurrent voxel access

Kinematics & Dynamics
======================

Motion in the simulated world follows classical Newtonian mechanics, implemented through discrete-time integration with fixed timesteps.

Position and Velocity
---------------------

Every mobile entity has a ``Position`` component :math:`\mathbf{r} = (x, y, z)` and ``Velocity`` component :math:`\mathbf{v} = (v_x, v_y, v_z)`.

**Position Update** (:file:`src/movement_system.cpp`):

.. math::

   \mathbf{r}(t + \Delta t) = \mathbf{r}(t) + \mathbf{v}(t) \cdot \Delta t

**Speed Magnitude**:

.. math::

   |\mathbf{v}| = \sqrt{v_x^2 + v_y^2 + v_z^2}

**Implementation Details**:

- Time-to-move calculation: :math:`t_{move} = \frac{100.0}{|\mathbf{v}|}` (time to traverse 100 voxels)
- Movement completes when :math:`t_{elapsed} \geq t_{move}`
- Grid-aligned movement with smooth interpolation for rendering

Acceleration and Force
----------------------

Forces modify velocity through acceleration according to Newton's second law.

**Velocity Update** (:file:`src/physics_manager.cpp`):

.. math::

   \mathbf{v}(t + \Delta t) = \mathbf{v}(t) + \mathbf{a}(t) \cdot \Delta t

**Translation Function** (:file:`src/physics_manager.cpp:154-204`):

The ``translateAcceleration`` function applies force-derived acceleration with speed limits:

.. code-block:: cpp

   // Acceleration magnitude bounded by entity's PhysicsStats
   float accel = std::min(force, physicsStats.maxForce) / physicsStats.mass;
   
   // Speed clamping
   if (speed > maxSpeed) {
       velocity *= (maxSpeed / speed);  // Scale down to limit
   }
   if (speed < minSpeed && speed > 0) {
       velocity = 0;  // Stop if below minimum threshold
   }

**Multi-axis vs Single-axis Movement**:

- **Multi-axis mode** (:file:`settings.py` - ``ALLOW_DIAGONAL_MOVEMENT = True``): 
  
  - All axes receive proportional acceleration
  - Diagonal movement penalty: :math:`v_{effective} = \frac{v}{1.414}` when moving on multiple axes
  
- **Single-axis mode**: 
  
  - Only dominant axis (largest :math:`|v_i|`) receives acceleration
  - Forces entity to align with cardinal directions

Gravity
-------

Gravitational acceleration affects all entities with mass when not supported by solid terrain.

**Gravity Force** (:file:`src/movement_system.cpp:493-521`):

.. math::

   a_{gravity} = -g \hat{\mathbf{z}}

where :math:`g = 5.0 \, \text{m/s}^2` (configurable via ``PhysicsManager::getGravity()``).

**Velocity Update**:

.. math::

   v_z(t + \Delta t) = v_z(t) - g \cdot \Delta t

**Fall Detection** (:file:`src/movement_system.cpp:497`):

.. code-block:: cpp

   // Entity falls if voxel below is empty or water (non-solid)
   bool isFalling = !voxelGrid->hasTerrainAt(x, y, z-1) || 
                    terrainBelow.mainType == WATER;

**Ground Contact**: Velocity is zeroed when the entity contacts solid terrain (:math:`v_z` changes sign), preventing bouncing.

**Design Note**: Default gravity is :math:`5.0 \, \text{m/s}^2` for gameplay balance, though design documents reference Earth's :math:`g = 9.81 \, \text{m/s}^2` (:file:`settings.py:13`).

Friction
--------

Kinetic friction opposes motion when entities are in contact with surfaces.

**Friction Force** (:file:`src/movement_system.cpp:538-567`):

.. math::

   \mathbf{F}_{friction} = -\mu \cdot m \cdot g \cdot \hat{\mathbf{v}}

where:

- :math:`\mu = 1.0` is the coefficient of friction (``PhysicsManager::getFriction()``)
- :math:`m` is entity mass
- :math:`g` is gravitational acceleration
- :math:`\hat{\mathbf{v}} = \frac{\mathbf{v}}{|\mathbf{v}|}` is the velocity direction

**Acceleration due to Friction**:

.. math::

   \mathbf{a}_{friction} = -\mu \cdot g \cdot \hat{\mathbf{v}}

With default values (:math:`\mu = 1.0`, :math:`g = 5.0`):

.. math::

   |\mathbf{a}_{friction}| = 5.0 \, \text{m/s}^2

**Friction Application** (:file:`src/movement_system.cpp:555`):

.. code-block:: cpp

   // Only apply friction if entity is on stable ground
   if (entity.isOnGround && entity.velocity.length() > 0) {
       float frictionAccel = friction * gravity;
       velocity -= frictionAccel * velocityDirection * deltaTime;
       
       // Stop if friction would reverse direction
       if (velocity.dot(oldVelocity) < 0) {
           velocity = {0, 0, 0};
       }
   }

**Stopping Condition**: Entity stops moving when friction would reverse velocity direction, preventing oscillation.

Thermodynamics & Phase Transitions
===================================

The simulation implements a complete water cycle with phase transitions driven by thermal energy transfer.

Matter Containers
-----------------

Each voxel contains a ``MatterContainer`` storing quantities of different matter types (:file:`include/components.hpp:85-90`):

- ``terrainMatter``: Solid terrain (cubic decimeters)
- ``waterMatter``: Liquid water (cubic decimeters)
- ``vaporMatter``: Water vapor/steam (cubic decimeters at STP equivalent)
- ``biomassMatter``: Organic material (cubic decimeters)

**Conservation Law**: Total matter is conserved across phase transitions:

.. math::

   \Delta m_{water} + \Delta m_{vapor} = 0

Evaporation (Endothermic)
-------------------------

Liquid water absorbs thermal energy to transition to vapor when exposed to heat and air.

**Evaporation Conditions** (:file:`src/matter_physics_system.cpp:189-240`):

1. :math:`Q \geq Q_{evap} = 120.0` (heat threshold)
2. :math:`m_{water} \geq m_{min} = 120,000` units (minimum water quantity)
3. Voxel exposed to air (not submerged)

**Evaporation Rate**:

.. math::

   \dot{m}_{evap} = k_{evap} \cdot I_{sun}

where:

- :math:`k_{evap} = 8.0` is the evaporation coefficient (``EVAPORATION_COEFFICIENT``)
- :math:`I_{sun} \in [0, 1]` is solar intensity (time-of-day dependent)

**Matter Transfer**:

.. math::

   m_{water}(t + \Delta t) &= m_{water}(t) - \min(m_{water}, \dot{m}_{evap} \cdot \Delta t) \\
   m_{vapor}(t + \Delta t) &= m_{vapor}(t) + \min(m_{water}, \dot{m}_{evap} \cdot \Delta t)

**Energy Absorption** (:file:`src/matter_physics_system.cpp:234`):

.. math::

   \Delta Q = -\Delta m_{evap} \cdot c_{evap}

where :math:`c_{evap} = 0.000002` is heat absorbed per unit evaporated (``HEAT_PER_EVAPORATION``).

**Physical Interpretation**: Evaporation is endothermic—water absorbs heat from the voxel, cooling it. This implements latent heat of vaporization.

Condensation (Exothermic)
-------------------------

Water vapor releases thermal energy when cooling below saturation capacity, forming liquid water.

**Saturation Model** (:file:`src/matter_physics_system.cpp:242-295`):

Vapor capacity decreases with:

- **Altitude**: Higher elevations hold less vapor (lower pressure)
- **Temperature**: Cooler air holds less vapor

**Condensation Threshold**:

.. math::

   m_{vapor} > m_{sat}(z, T)

where :math:`m_{sat}` is the saturation capacity function.

**Matter Transfer**:

.. math::

   \Delta m_{cond} = m_{vapor} - m_{sat}

.. math::

   m_{vapor}(t + \Delta t) &= m_{sat} \\
   m_{water}(t + \Delta t) &= m_{water}(t) + \Delta m_{cond}

**Energy Release** (:file:`src/matter_physics_system.cpp:289`):

.. math::

   \Delta Q = +\Delta m_{cond} \cdot c_{cond}

where :math:`c_{cond}` is heat released per unit condensed.

**Physical Interpretation**: Condensation is exothermic—vapor releases heat to the voxel, warming it. This implements latent heat of condensation.

**Conservation of Energy**: Over a complete evaporation-condensation cycle:

.. math::

   Q_{evap} + Q_{cond} = 0

Precipitation (Rain)
--------------------

Liquid water at high altitude falls as rain when :math:`z > z_{ground}` and no support exists below.

**Rain Physics** (:file:`src/matter_physics_system.cpp:297-340`):

1. **Free Fall**: Water accelerates downward under gravity
2. **Terminal Velocity**: Drag limits maximum fall speed
3. **Ground Impact**: Kinetic energy converts to:
   
   - Thermal energy (heating ground)
   - Lateral splash (momentum redistribution)

**Simplified Model**:

.. math::

   v_{rain} = \sqrt{2 g h}

capped at terminal velocity :math:`v_{term} \approx 9 \, \text{m/s}` for water droplets.

Fluid Dynamics (Water Flow)
----------------------------

Water flows through terrain following pressure gradients, implemented as cellular automata for real-time performance.

**Pressure Model** (:file:`src/matter_physics_system.cpp:342-450`):

.. math::

   P(x,y,z) = \rho g h + m_{water}

where:

- :math:`\rho` is water density
- :math:`h` is height of water column above
- :math:`m_{water}` is local water quantity

**Flow Direction**: Water moves down pressure gradients:

.. math::

   \mathbf{\nabla} P = \left( \frac{\partial P}{\partial x}, \frac{\partial P}{\partial y}, \frac{\partial P}{\partial z} \right)

**Flow Priority**:

1. **Downward** (gravity): :math:`\Delta z < 0` always preferred
2. **Lateral** (equilibrium): :math:`\Delta x, \Delta y` when heights equalize

**Parallel Implementation** (:file:`src/matter_physics_system.cpp:150-187`):

- World divided into 32³ voxel boxes
- Each box processed in parallel thread
- Thread-local OpenVDB accessors eliminate locking
- Conservative transfer: mass balanced across box boundaries

Structural Mechanics
====================

Entities and terrain possess structural properties that govern stacking, collapse, and matter state behaviors.

Matter States
-------------

The ``MatterState`` enum (:file:`include/components.hpp:60-65`) defines four phases:

.. code-block:: cpp

   enum struct MatterState {
       SOLID = 1,    // Subject to gravity, friction, collision
       LIQUID = 2,   // Flows, gravity, no ground friction
       GAS = 3,      // Diffuses, buoyant, no gravity/friction
       PLASMA = 4    // Not implemented
   };

**Behavioral Differences**:

+----------+----------+----------+------------+-------------+
| State    | Gravity  | Friction | Collision  | Flow        |
+==========+==========+==========+============+=============+
| SOLID    | Yes      | Yes      | Full       | No          |
+----------+----------+----------+------------+-------------+
| LIQUID   | Yes      | No       | Partial    | Yes         |
+----------+----------+----------+------------+-------------+
| GAS      | No*      | No       | No         | Diffusion   |
+----------+----------+----------+------------+-------------+
| PLASMA   | TBD      | TBD      | TBD        | TBD         |
+----------+----------+----------+------------+-------------+

*\*Gas experiences buoyancy (negative effective gravity)*

Structural Integrity
--------------------

The ``StructuralIntegrityComponent`` (:file:`include/components.hpp:113-118`) defines load-bearing properties:

.. code-block:: cpp

   struct StructuralIntegrityComponent {
       bool canStackEntities;        // Can others stand on this?
       int maxLoadCapacity;          // Max weight (-1 = infinite)
       MatterState matterState;      // SOLID/LIQUID/GAS/PLASMA
       GradientVector gradientVector; // Terrain slope (gx, gy, gz)
   };

**Stacking Rules**:

- Entity can stand on voxel if: ``canStackEntities == true`` AND ``currentLoad + entityMass ≤ maxLoadCapacity``
- Terrain collapse occurs when: ``currentLoad > maxLoadCapacity``
- Infinite capacity (``maxLoadCapacity = -1``): Bedrock, stone, most solid terrain

Terrain Slopes and Ramps
-------------------------

Terrain supports non-block shapes through gradient vectors and subtype variants.

**Terrain Variants** (:file:`include/voxel.hpp:48-64`):

- ``FLAT`` (0): Standard cube
- ``RAMP_EAST`` (1): Slope rising eastward (+x)
- ``RAMP_WEST`` (2): Slope rising westward (-x)
- ``RAMP_SOUTH`` (7): Slope rising southward (+y)
- ``RAMP_NORTH`` (8): Slope rising northward (-y)
- ``CORNER_*``: Various corner configurations

**Gradient Vector** (:file:`include/components.hpp:68-72`):

.. code-block:: cpp

   struct GradientVector {
       int8_t gx;  // X-axis slope (-1, 0, +1)
       int8_t gy;  // Y-axis slope (-1, 0, +1)
       int8_t gz;  // Z-axis slope (-1, 0, +1)
   };

**Movement on Slopes** (:file:`src/movement_system.cpp:340-385`):

When entity moves onto a ramp, target Z-coordinate adjusts:

.. math::

   z_{target} = z_{base} + g_z

This enables smooth "step-up" behavior without explicit climbing mechanics.

Physical Constants Reference
=============================

This section catalogs all physical parameters used in the simulation, extracted from source code.

Global Physics Parameters
--------------------------

Defined in :file:`src/physics_manager.cpp` and :file:`include/physics_manager.hpp`:

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Parameter
     - Symbol
     - Value
     - Description
   * - Gravitational Acceleration
     - :math:`g`
     - 5.0 m/s²
     - Default gravity (configurable)
   * - Friction Coefficient
     - :math:`\mu`
     - 1.0
     - Kinetic friction for ground contact
   * - Allow Diagonal Movement
     - —
     - ``true``
     - Permits multi-axis acceleration
   * - Diagonal Speed Penalty
     - —
     - :math:`1/\sqrt{2}`
     - Applied when moving on multiple axes
   * - Energy Cost per Force
     - —
     - 0.000002
     - Metabolism cost for acceleration

Thermodynamic Constants
------------------------

Defined in :file:`src/matter_physics_system.cpp` and :file:`include/matter_physics_system.hpp`:

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Parameter
     - Symbol
     - Value
     - Description
   * - Evaporation Coefficient
     - :math:`k_{evap}`
     - 8.0
     - Rate multiplier for evaporation
   * - Heat Threshold (Evaporation)
     - :math:`Q_{evap}`
     - 120.0
     - Minimum heat for water evaporation
   * - Minimum Water for Evaporation
     - :math:`m_{min}`
     - 120,000 units
     - Prevents trace water evaporation
   * - Heat per Evaporation
     - :math:`c_{evap}`
     - 0.000002
     - Energy absorbed per unit evaporated
   * - Condensation Threshold
     - :math:`m_{sat,min}`
     - 21 units
     - Minimum vapor for condensation
   * - Heat per Condensation
     - :math:`c_{cond}`
     - 0.000002
     - Energy released per unit condensed

Ecosystem Constants
-------------------

Defined in :file:`src/matter_physics_system.cpp` and related files:

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Parameter
     - Symbol
     - Value
     - Description
   * - Grid Box Size
     - —
     - 32³ voxels
     - Parallel processing partition size
   * - Water per Photosynthesis
     - —
     - 0.1
     - Water consumed per biomass created
   * - Energy Production Rate
     - —
     - 6.0
     - Energy generated per photosynthesis
   * - Max Task Priority
     - —
     - 1000
     - Task scheduler cap
   * - Priority Aging
     - —
     - -10
     - Priority reduction per cycle

Rendering Constants
-------------------

Defined in :file:`settings.py`:

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Parameter
     - Symbol
     - Value
     - Description
   * - Target Frame Rate
     - —
     - 60 FPS
     - Rendering target
   * - Voxel Render Size
     - —
     - 32 pixels
     - Screen space per voxel
   * - Design Gravity (Reference)
     - :math:`g_{Earth}`
     - 9.81 m/s²
     - Not used in runtime physics

Game Time Scale
---------------

Defined in :file:`lifesim/time_manager.py`:

.. list-table::
   :header-rows: 1
   :widths: 30 40

   * - Game Time Unit
     - Real-time Equivalent
   * - 1 game minute
     - 10 real seconds
   * - 1 game hour
     - 10 real minutes (debug mode)
   * - 1 game day
     - 24 game hours
   * - 1 game month
     - 28 days
   * - 1 game year
     - 4 months

Connection to Architecture
===========================

This document describes the **physical models** governing the simulation. For implementation details of how these models are encoded in the system architecture, see:

- **Architecture Overview**: :doc:`architecture` - Complete system architecture including storage layers, coordination mechanisms, and data flow patterns
- **Physics Engine**: :doc:`physics_engine` - Implementation details of movement and collision
- **Ecosystem Engine**: :doc:`ecosystem_engine` - Implementation of water cycle and matter flow
- **Concurrency**: :doc:`concurrency` - Parallel processing strategies for physics simulation
- **Performance**: :doc:`performance` - Optimization techniques and benchmarking

The mathematical models here are realized through carefully orchestrated component systems (``VoxelGrid``, ``TerrainGridRepository``, ``TerrainStorage``), spatial indexing (OpenVDB grids), and lazy activation patterns documented in the architecture guide.
