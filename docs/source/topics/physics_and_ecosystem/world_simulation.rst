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

- **Grid Storage**: Sparse OpenVDB grids store only non-default values
- **Activation Model**: Terrain transitions from "cold storage" (VDB-only) to "hot" (ECS entity) when exhibiting dynamic behavior
- **Thread Safety**: Thread-local accessor caches enable O(1) concurrent voxel access

Units and Measurement
======================

The simulation uses a consistent system of units based on the International System (SI) with game-specific adaptations. All physical equations in this document use these units unless otherwise specified.

Base Units
----------

**Spatial Units**

- **Length**: meter (m)
- **Voxel Resolution**: 1 voxel = 1 cubic meter (1 m³)
- **Coordinate System**: Right-handed Cartesian (x=East, y=South, z=Up)
- **Volume**: cubic decimeters (dm³) for matter quantities
  
  - 1 voxel = 1 m³ = 1000 dm³
  - Water/terrain matter stored in dm³ units for finer granularity

**Temporal Units**

- **Real-time**: seconds (s)
- **Game Time Scale**: tracked in ``GameClock`` (:file:`src/GameClock.hpp`) on the C++ side; the Python layer drives the wall-clock to game-clock mapping.

- **Physics Timestep**: Variable :math:`\Delta t` (typically 16.67 ms for 60 FPS)

**Mass Units**

- **Mass**: kilograms (kg)
- **Entity Mass**: Stored in ``PhysicsStats.mass`` (:file:`src/components/PhysicsComponents.hpp`)
- **Density Reference**: Water = 1000 kg/m³ (1 kg/dm³)
- **Matter Quantities**: Dimensionless units (approximately dm³ equivalent)

**Thermal Units**

- **Temperature**: Kelvin (K) or context-dependent scale
- **Heat**: Stored as a per-voxel ``FloatGrid`` (``TerrainStorage::heatGrid``) and as ``PhysicsStats.heat`` on ECS-backed entities (:file:`src/components/PhysicsComponents.hpp`)
- **Heat Thresholds**: 
  
  - Evaporation: :math:`Q \geq 120.0` (arbitrary units)
  - Condensation: Function of altitude and temperature
  
- **Heat Transfer Coefficients**: 
  
  - :math:`c_{evap} = 0.000002` (heat per matter unit evaporated)
  - :math:`c_{cond} = 0.000002` (heat per matter unit condensed)

**Note**: The simulation uses a simplified thermal model where heat is a scalar quantity without explicit temperature calculations. Heat thresholds trigger phase transitions rather than modeling precise thermodynamic states.

Derived Units
-------------

**Kinematic Quantities**

- **Velocity**: meters per second (m/s)

  - For ECS-backed entities: stored in the ``Velocity`` component as :math:`(v_x, v_y, v_z)` (:file:`src/components/PhysicsComponents.hpp`)
  - For terrain voxels (water, vapor, etc.): stored on the VDB ``velXGrid``/``velYGrid``/``velZGrid`` (:file:`src/terrain/TerrainStorage.hpp`) and read through ``TerrainGridRepository::getVelocity``
  - Speed magnitude: :math:`|\mathbf{v}| = \sqrt{v_x^2 + v_y^2 + v_z^2}` (m/s)

- **Acceleration**: meters per second squared (m/s²)
  
  - Gravity: :math:`g = 5.0 \, \text{m/s}^2` (default)
  - Friction: :math:`a_f = \mu g = 5.0 \, \text{m/s}^2` (with :math:`\mu = 1.0`)

**Dynamic Quantities**

- **Force**: Newtons (N = kg⋅m/s²)

  - Applied through the movement mutators in :file:`src/physics/mutators/MovementMutators.cpp` and the math helpers in :file:`src/physics/PhysicalMath.cpp`
  - Per-axis force is stored on ``PhysicsStats.forceX``/``.forceY``/``.forceZ``

- **Momentum**: kilogram-meters per second (kg⋅m/s)
  
  - :math:`\mathbf{p} = m \mathbf{v}`
  - Conserved in collision systems

- **Energy**: Joules (J = kg⋅m²/s²)
  
  - Kinetic energy: :math:`E_k = \frac{1}{2} m v^2`
  - Potential energy: :math:`E_p = m g h`
  - Energy cost per applied force: 0.000002 J (``PhysicsManager::getMetabolismCostToApplyForce``)

**Flow and Matter**

- **Matter Flow Rate**: cubic decimeters per second (dm³/s)
  
  - Evaporation rate: :math:`\dot{m} = k_{evap} \cdot I_{sun}` where :math:`k_{evap} = 8.0 \, \text{dm}^3/\text{s}`
  - Water flow: Pressure-gradient driven (cellular automata)

- **Pressure**: Pascals (Pa = N/m² = kg/(m⋅s²))
  
  - :math:`P = \rho g h + m_{water}` (simplified hydrostatic model)
  - Used for flow direction determination

Special Conversions
-------------------

**Voxel Space ↔ World Space**

.. code-block:: cpp

   // Voxel coordinates (discrete)
   int vx = 10, vy = 20, vz = 5;
   
   // World coordinates (continuous, in meters)
   float wx = vx * 1.0f;  // 10.0 m
   float wy = vy * 1.0f;  // 20.0 m
   float wz = vz * 1.0f;  // 5.0 m

**Matter Quantity Interpretation**

.. code-block:: cpp

   // Water in voxel (cubic decimeters)
   int waterMatter = 500;  // 500 dm³ = 0.5 m³ (half-full voxel)
   
   // Mass of water (assuming density = 1 kg/dm³)
   float mass_kg = waterMatter * 1.0f;  // 500 kg

**Time Dilation**

.. code-block:: cpp

   // Real-time physics timestep
   float dt_real = 0.01667f;  // 16.67 ms (60 FPS)
   
   // Game-time advancement
   float game_minutes = dt_real * 6.0f;  // 6× speedup

Unit Consistency in Equations
------------------------------

All equations in this document maintain dimensional consistency. For example:

**Velocity Update**:

.. math::

   \mathbf{v}(t + \Delta t) = \mathbf{v}(t) + \mathbf{a}(t) \cdot \Delta t

**Dimensional Analysis**:

.. math::

   [\text{m/s}] = [\text{m/s}] + [\text{m/s}^2] \cdot [\text{s}] \checkmark

**Force to Acceleration**:

.. math::

   a = \frac{F}{m}

.. math::

   [\text{m/s}^2] = \frac{[\text{N}]}{[\text{kg}]} = \frac{[\text{kg⋅m/s}^2]}{[\text{kg}]} \checkmark

Readers can verify that all physical equations in subsequent sections are dimensionally correct using these base and derived units.

Kinematics & Dynamics
======================

Motion in the simulated world follows classical Newtonian mechanics, implemented through discrete-time integration with fixed timesteps.

Position and Velocity
---------------------

Every mobile entity has a ``Position`` component :math:`\mathbf{r} = (x, y, z)` and ``Velocity`` component :math:`\mathbf{v} = (v_x, v_y, v_z)`.

**Position Update** (:file:`src/physics/mutators/MovementMutators.cpp`):

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

**Velocity Update** (:file:`src/physics/PhysicalMath.cpp`):

.. math::

   \mathbf{v}(t + \Delta t) = \mathbf{v}(t) + \mathbf{a}(t) \cdot \Delta t

**Translation Function** (:file:`src/physics/PhysicalMath.cpp`):

The translation helper applies force-derived acceleration with speed limits:

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

- **Multi-axis mode** (``PhysicsManager::getAllowMultiDirection() == true``, default):

  - All axes receive proportional acceleration
  - Diagonal movement penalty: :math:`v_{effective} = \frac{v}{1.414}` when moving on multiple axes

- **Single-axis mode**:

  - Only the dominant axis (largest :math:`|v_i|`) receives acceleration
  - Forces entity to align with cardinal directions

Gravity
-------

Gravitational acceleration affects all entities with mass when not supported by solid terrain.

**Gravity Force** (:file:`src/physics/mutators/MovementMutators.cpp` and the gravity-enqueue pass in :file:`src/PhysicsEngine.cpp`):

.. math::

   a_{gravity} = -g \hat{\mathbf{z}}

where :math:`g = 5.0 \, \text{m/s}^2` (configurable via ``PhysicsManager::getGravity()``).

**Velocity Update**:

.. math::

   v_z(t + \Delta t) = v_z(t) - g \cdot \Delta t

**Fall Detection** (in ``PhysicsEngine::applyGravityForceToEntity``, with predicates in :file:`src/physics/Collision.hpp`):

.. code-block:: cpp

   // Entity falls if voxel below is empty or non-solid (e.g. water)
   bool isFalling = !voxelGrid.checkIfTerrainExists(x, y, z - 1) ||
                    terrainBelow.mainType == TERRAIN_WATER;

**Ground Contact**: Velocity is zeroed when the entity contacts solid terrain (:math:`v_z` changes sign), preventing bouncing.

**Design Note**: Default runtime gravity is :math:`5.0 \, \text{m/s}^2` for gameplay balance. The constant ``GRAVITY = 9.81f`` in :file:`src/physics/PhysicsConstants.hpp` is a design-time reference value and is not used by the runtime physics path.

Friction
--------

Kinetic friction opposes motion when entities are in contact with surfaces.

**Friction Force** (:file:`src/physics/mutators/MovementMutators.cpp`):

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

**Friction Application** (:file:`src/physics/mutators/MovementMutators.cpp`):

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

Each voxel exposes a ``MatterContainer`` (:file:`src/components/TerrainComponents.hpp`) storing quantities of different matter types — backed by separate VDB grids in ``TerrainStorage``:

- ``TerrainMatter``: Solid terrain (cubic decimeters)
- ``WaterMatter``: Liquid water (cubic decimeters)
- ``WaterVapor``: Water vapor / steam (cubic decimeters at STP equivalent)
- ``BioMassMatter``: Organic material (cubic decimeters)

**Conservation Law**: Total matter is conserved across phase transitions:

.. math::

   \Delta m_{water} + \Delta m_{vapor} = 0

Evaporation (Endothermic)
-------------------------

Liquid water absorbs thermal energy to transition to vapor when exposed to heat and air.

**Evaporation Conditions** (detection in :file:`src/EcosystemEngine.cpp`, mutation in :file:`src/physics/mutators/WaterPhysicsMutators.cpp`):

1. :math:`Q \geq Q_{evap}` (``PhysicsManager::getHeatToWaterEvaporation``, default ``120.0``)
2. :math:`m_{water} \geq m_{min}` (``PhysicsManager::getWaterMinimumUnits``, default ``60,000``)
3. Voxel exposed to air (not submerged)
4. ``simulateWaterEvaporation`` toggle is on

**Evaporation Rate**:

.. math::

   \dot{m}_{evap} = k_{evap} \cdot I_{sun}

where:

- :math:`k_{evap} = 8.0` is the evaporation coefficient (``PhysicsManager::getEvaporationCoefficient``)
- :math:`I_{sun} \in [0, 1]` is solar intensity (``SunIntensity::getIntensity(clock)``, time-of-day dependent)

**Matter Transfer**:

.. math::

   m_{water}(t + \Delta t) &= m_{water}(t) - \min(m_{water}, \dot{m}_{evap} \cdot \Delta t) \\
   m_{vapor}(t + \Delta t) &= m_{vapor}(t) + \min(m_{water}, \dot{m}_{evap} \cdot \Delta t)

**Energy Absorption** (:file:`src/physics/mutators/WaterPhysicsMutators.cpp`):

.. math::

   \Delta Q = -\Delta m_{evap} \cdot c_{evap}

where :math:`c_{evap} = 0.000002` is heat absorbed per unit evaporated.

**Physical Interpretation**: Evaporation is endothermic—water absorbs heat from the voxel, cooling it. This implements latent heat of vaporization.

Condensation (Exothermic)
-------------------------

Water vapor releases thermal energy when cooling below saturation capacity, forming liquid water.

**Saturation Model** (detection in :file:`src/EcosystemEngine.cpp`, mutation in :file:`src/physics/mutators/WaterPhysicsMutators.cpp` via ``PhysicsEngine::onCondenseWaterEntityEvent``):

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

**Energy Release** (:file:`src/physics/mutators/WaterPhysicsMutators.cpp`):

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

**Rain Physics** (``PhysicsEngine::onWaterFallEntityEvent`` in :file:`src/PhysicsEngine.cpp`, with mutators in :file:`src/physics/mutators/WaterPhysicsMutators.cpp`):

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

**Pressure Model** (``GridBoxProcessor::processVoxelWater`` in :file:`src/EcosystemEngine.cpp`):

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

**Parallel Implementation** (``WaterSimulationManager`` / ``GridBoxProcessor`` in :file:`src/EcosystemEngine.cpp`):

- World divided into 32³ voxel boxes
- Each box processed by a worker thread out of a shared pool
- Thread-local OpenVDB accessors eliminate locking on the read side
- Resulting ``std::vector<WaterFlow>`` is applied under ``WaterSimulationManager::gridWriteMutex_`` so per-box reads stay lock-free
- Conservative transfer: mass balanced across box boundaries

Structural Mechanics
====================

Entities and terrain possess structural properties that govern stacking, collapse, and matter state behaviors.

Matter States
-------------

The ``MatterState`` enum (:file:`src/components/PhysicsComponents.hpp`) defines four phases:

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

The ``StructuralIntegrityComponent`` (:file:`src/components/PhysicsComponents.hpp`) defines load-bearing properties:

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

**Terrain Variants** (``TerrainVariantEnum`` in :file:`src/components/TerrainComponents.hpp`):

- ``FULL`` (0): Standard cube
- ``RAMP_EAST`` (1): Slope rising eastward (+x)
- ``RAMP_WEST`` (2): Slope rising westward (-x)
- ``RAMP_SOUTH`` (7): Slope rising southward (+y)
- ``RAMP_NORTH`` (8): Slope rising northward (-y)
- ``CORNER_SOUTH_EAST`` (3) / ``CORNER_SOUTH_EAST_INV`` (4) / ``CORNER_NORTH_EAST`` (5) / ``CORNER_NORTH_EAST_INV`` (6) / ``CORNER_SOUTH_WEST`` (9) / ``CORNER_NORTH_WEST`` (10): Corner configurations

**Gradient Vector** (:file:`src/components/PhysicsComponents.hpp`):

.. code-block:: cpp

   struct GradientVector {
       float gx;  // X-axis slope
       float gy;  // Y-axis slope
       float gz;  // Z-axis slope
   };

**Movement on Slopes** (:file:`src/physics/mutators/MovementMutators.cpp` and ``hasSpecialCollision`` in :file:`src/physics/Collision.hpp`):

When entity moves onto a ramp, target Z-coordinate adjusts:

.. math::

   z_{target} = z_{base} + g_z

This enables smooth "step-up" behavior without explicit climbing mechanics.

Physical Constants Reference
=============================

This section catalogs all physical parameters used in the simulation, extracted from source code.

Global Physics Parameters
--------------------------

Defined in :file:`src/physics/PhysicsManager.hpp` and :file:`src/physics/PhysicsManager.cpp`:

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

Defined on ``PhysicsManager`` (:file:`src/physics/PhysicsManager.hpp`) and applied by the water-physics mutators (:file:`src/physics/mutators/WaterPhysicsMutators.cpp`):

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
     - Rate multiplier (``getEvaporationCoefficient``)
   * - Heat Threshold (Evaporation)
     - :math:`Q_{evap}`
     - 120.0
     - Minimum heat for water evaporation (``getHeatToWaterEvaporation``)
   * - Minimum Water for Evaporation
     - :math:`m_{min}`
     - 60,000 units
     - Prevents trace water evaporation (``getWaterMinimumUnits``; sized for a 10×100×100 grid)
   * - Heat per Evaporation
     - :math:`c_{evap}`
     - 0.000002
     - Energy absorbed per unit evaporated
   * - Heat per Condensation
     - :math:`c_{cond}`
     - 0.000002
     - Energy released per unit condensed

Ecosystem Constants
-------------------

Defined in :file:`src/EcosystemEngine.hpp`, :file:`src/EcosystemEngine.cpp`, and :file:`src/components/WaterStressComponent.hpp`:

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
     - Parallel processing partition size (``WaterSimulationManager::DEFAULT_MIN_BOX_SIZE``)
   * - Max Task Priority
     - —
     - 1000
     - ``RoundRobinScheduler::MAX_PRIORITY``
   * - Priority Aging Bonus
     - —
     - 10
     - ``RoundRobinScheduler::AGE_BONUS`` (priority reduction per aging cycle)
   * - Stress per Dry Tick
     - —
     - 1
     - ``WaterStressComponent::STRESS_PER_DRY_TICK``
   * - Max Water Stress Ticks
     - —
     - 1000
     - ``WaterStressComponent::MAX_WATER_STRESS_TICKS``
   * - Drought Damage per Cycle
     - —
     - 10 HP
     - ``WaterStressComponent::DROUGHT_DAMAGE_PER_CYCLE``

Reference Values
----------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 15 40

   * - Parameter
     - Symbol
     - Value
     - Description
   * - Design Gravity (reference)
     - :math:`g_{Earth}`
     - 9.81 m/s²
     - ``GRAVITY`` constant in :file:`src/physics/PhysicsConstants.hpp`; informational only — runtime physics uses ``PhysicsManager::getGravity()``

Game Time Scale
---------------

Defined in the simulation layer (Python) — see ``lifesim/time_manager.py`` in the upstream simulation package:

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
