System Architecture
===================

The Physics and Ecosystem engines are the core simulation components of the Aetherion engine. They operate as distinct but interacting systems within the game loop, sharing state through the ``VoxelGrid`` and the Entity Component System (ECS) provided by ``EnTT``.

There are four key data systems that underpin the simulation:

- **VoxelGrid**: The central spatial data structure that integrates all grid-based storage and exposes a unified interface for spatial queries.
- **TerrainStorage**: The low-level, OpenVDB-backed storage for static terrain data, providing sparse and memory-efficient persistence.
- **TerrainGridRepository**: An ECS overlay for terrain voxels that manages the boundary between static terrain data (OpenVDB) and transient runtime behavior (ECS).
- **entt::registry (ECS)**: The entity-component registry that manages all entities and their components, covering both terrain entities (active/dynamic terrain) and non-terrain entities (creatures, items, etc.).

The **PhysicsEngine** is responsible for simulating physical interactions — movement, collision detection, gravity, and forces. While other subsystems may contribute force values, it is the PhysicsEngine that resolves them into position and state changes for entities with velocity and moving components.

The **EcosystemEngine** handles higher-level environmental processes such as the water cycle. It should ideally perform read-only queries against the data systems and queue changes to be applied after its iteration completes, since ecosystem processing (and other systems) may run in parallel, with each spatial partition on its own thread.

The following sections explore each of these areas in detail:

1. **Data Structures** — A breakdown of the storage and state management layers and how they interact.
2. **Physics Movement & Event Flow** — How movement events are created, validated, and resolved until an entity actually moves.
3. **Water Cycle & Ecosystem Simulation** — How water dynamics, evaporation, and other environmental processes are modeled.

High-Level Overview
-------------------

.. graphviz::

   digraph ownership {
       rankdir=TB;
       compound=true;
       node [shape=box, style="rounded,filled", fontsize=10];
       edge [fontsize=9];

       // Root
       World [label="World", fillcolor="#FFD700", style="rounded,filled,bold"];

       // World direct members
       Registry [label="entt::registry\n(Entity Component System)", fillcolor=lightyellow];
       Dispatcher [label="entt::dispatcher\n(Event Dispatcher)", fillcolor=lightyellow];
       VoxelGrid [label="VoxelGrid*\n(Central Spatial Hub)", fillcolor=lightcoral];
       PyRegistry [label="PyRegistry", fillcolor="#E8E8E8"];
       GameClock [label="GameClock", fillcolor="#E8E8E8"];

       // World -> direct members
       World -> Registry [label="owns"];
       World -> Dispatcher [label="owns"];
       World -> VoxelGrid [label="owns (raw ptr)"];
       World -> PyRegistry [label="owns"];
       World -> GameClock [label="owns"];

       // World private engine members (listed in text below diagram)
       Engines [label="Engines\n(private, raw ptrs)", fillcolor="#F0F0F0", style="rounded,filled"];
       World -> Engines [label="owns"];

       // VoxelGrid internal ownership
       TerrainStorage [label="TerrainStorage\n(unique_ptr)", fillcolor=lightcyan];
       TerrainGridRepo [label="TerrainGridRepository\n(unique_ptr)", fillcolor=lavender];
       EntityGrid [label="entityGrid\n(Int32Grid::Ptr)", fillcolor=lightgray];
       EventGrid [label="eventGrid\n(Int32Grid::Ptr)", fillcolor=lightgray];
       LightingGrid [label="lightingGrid\n(FloatGrid::Ptr)", fillcolor=lightgray];

       VoxelGrid -> TerrainStorage [label="owns (unique_ptr)"];
       VoxelGrid -> TerrainGridRepo [label="owns (unique_ptr)"];
       VoxelGrid -> EntityGrid [label="owns (shared_ptr)"];
       VoxelGrid -> EventGrid [label="owns (shared_ptr)"];
       VoxelGrid -> LightingGrid [label="owns (shared_ptr)"];

       // VoxelGrid references
       VoxelGrid -> Registry [label="references (&)", style=dashed, color=blue];

       // TerrainStorage OpenVDB Grids (listed in text below diagram)
       OpenVDBGrids [label="OpenVDB Grids\n(Int32Grid / FloatGrid)", fillcolor="#E0F7FA", style="rounded,filled"];
       TerrainStorage -> OpenVDBGrids [label="owns"];

       // TerrainGridRepository internal data
       subgraph cluster_tgr_maps {
           label="Bidirectional Tracking Maps";
           style=filled;
           fillcolor="#F3E5F5";
           ByCoord [label="byCoord_\n(unordered_map<VoxelCoord, entity>)", fillcolor=white];
           ByEntity [label="byEntity_\n(unordered_map<entity, VoxelCoord>)", fillcolor=white];
       }

       TerrainGridRepo -> ByCoord [label="owns"];
       TerrainGridRepo -> ByEntity [label="owns"];
       TerrainGridRepo -> Registry [label="references (&)", style=dashed, color=blue];
       TerrainGridRepo -> TerrainStorage [label="references (&)", style=dashed, color=blue];
   }

**World → Engines** (private, raw ptrs):

- ``PhysicsEngine*`` — Movement, collision, gravity, forces
- ``EcosystemEngine*`` — Water dynamics, plant cycles
- ``LifeEngine*`` — Life cycle management
- ``MetabolismSystem*`` — Energy, hunger, consumption
- ``HealthSystem*`` — HP, damage, healing
- ``CombatSystem*`` — Attack resolution, combat logic
- ``EffectsSystem*`` — Status effects, buffs, debuffs

**TerrainStorage → OpenVDB Grids** (Int32Grid::Ptr unless noted):

- ``terrainGrid`` — Entity ID source of truth (−2=empty, −1=VDB-only, ≥0=ECS entity)
- ``mainTypeGrid`` — Entity main type (TERRAIN, PLANT, BEAST, etc.)
- ``subType0Grid``, ``subType1Grid`` — Subtype classifications
- ``terrainMatterGrid``, ``waterMatterGrid``, ``vaporMatterGrid``, ``biomassMatterGrid`` — Matter quantities
- ``massGrid``, ``maxSpeedGrid``, ``minSpeedGrid`` — Physical properties
- ``heatGrid`` — Temperature (FloatGrid)
- ``flagsGrid`` — Packed bit flags (direction, canStack, matterState, gradient)
- ``maxLoadCapacityGrid`` — Structural load capacity

.. graphviz::

   digraph architecture {
       rankdir=TB;
       node [shape=box, style=rounded];
       edge [fontsize=9];
       
       // Engines
       PhysicsEngine [label="PhysicsEngine\n(Movement, Collision,\nGravity, Forces)", fillcolor=lightblue, style="rounded,filled"];
       EcosystemEngine [label="EcosystemEngine\n(Water Dynamics,\nPlant Cycles)", fillcolor=lightgreen, style="rounded,filled"];
       
       // ECS Registry
       Registry [label="entt::registry\n(Entity Component System)", fillcolor=lightyellow, style="rounded,filled"];
       
       // Main VoxelGrid
       VoxelGrid [label="VoxelGrid\n(Central Spatial Hub)", fillcolor=lightcoral, style="rounded,filled"];
       
       // TerrainGridRepository subgraph
       subgraph cluster_tgr {
           label="TerrainGridRepository";
           style=filled;
           fillcolor=lavender;
           TGR_MAPS [label="Bidirectional Maps\n(byCoord_, byEntity_)", shape=box, fillcolor=white, style="rounded,filled"];
       }
       
       // TerrainStorage subgraph
       subgraph cluster_ts {
           label="TerrainStorage";
           style=filled;
           fillcolor=lightcyan;
           TS_OPENVDB [label="OpenVDB Grids\n(mainTypeGrid, etc.)", shape=box, fillcolor=white, style="rounded,filled"];
       }
       
       // Other grids
       EG [label="EntityGrid\n(OpenVDB Int32Grid)", fillcolor=lightgray, style="rounded,filled"];
       EvG [label="EventGrid\n(OpenVDB Int32Grid)", fillcolor=lightgray, style="rounded,filled"];
       LG [label="LightingGrid\n(OpenVDB FloatGrid)", fillcolor=lightgray, style="rounded,filled"];
       
       PhysicsManager [label="PhysicsManager\n(Global Constants)", fillcolor=lightyellow, style="rounded,filled"];
       
       // Relationships - Engine to main systems
       PhysicsEngine -> Registry [label="reads/writes"];
       PhysicsEngine -> VoxelGrid [label="reads/writes"];
       EcosystemEngine -> Registry [label="reads/writes"];
       EcosystemEngine -> VoxelGrid [label="reads/writes"];
       
       // VoxelGrid internal structure
       VoxelGrid -> TGR_MAPS [label="manages"];
       VoxelGrid -> EG [label="manages"];
       VoxelGrid -> EvG [label="manages"];
       VoxelGrid -> LG [label="manages"];
       
       // TerrainGridRepository data access
       TGR_MAPS -> Registry [label="syncs", style=dashed, color=blue];
       TGR_MAPS -> TS_OPENVDB [label="manages", color=green];
       
       // References
       PhysicsEngine -> PhysicsManager [style=dotted];
       EcosystemEngine -> PhysicsManager [style=dotted];
   }

Hierarchical Abstraction Levels
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. graphviz::

   digraph abstraction_hierarchy {
       rankdir=TB;
       node [shape=box, style=rounded];
       edge [fontsize=9];
       
       // Layer 1: Application/Engine Layer
       subgraph cluster_app {
           label="Application/Engine Layer\n(Simulation Logic)";
           style=filled;
           fillcolor="#E8F4F8";
           PE [label="PhysicsEngine", fillcolor=lightblue, style="rounded,filled"];
           EE [label="EcosystemEngine", fillcolor=lightgreen, style="rounded,filled"];
       }
       
       // Layer 2: State Management & Coordination
       subgraph cluster_state {
           label="State Management & Coordination Layer\n(Data Orchestration)";
           style=filled;
           fillcolor="#FFF8E8";
           VoxelGrid [label="VoxelGrid\n(Central Hub)", fillcolor=lightcoral, style="rounded,filled"];
           TGR [label="TerrainGridRepository\n(Terrain Mapping)", fillcolor=lavender, style="rounded,filled"];
           PM [label="PhysicsManager\n(Config)", fillcolor=lightyellow, style="rounded,filled"];
       }
       
       // Layer 3: Storage & Data Access
       subgraph cluster_storage {
           label="Storage & Data Access Layer\n(Persistent Data)";
           style=filled;
           fillcolor="#F0F8E8";
           Registry [label="entt::registry\n(ECS)", fillcolor=lightyellow, style="rounded,filled"];
           TS [label="TerrainStorage\n(OpenVDB Grids)", fillcolor=lightcyan, style="rounded,filled"];
           EG [label="EntityGrid\n(Int32 OpenVDB)", fillcolor=lightgray, style="rounded,filled"];
           EvG [label="EventGrid\n(Int32 OpenVDB)", fillcolor=lightgray, style="rounded,filled"];
           LG [label="LightingGrid\n(Float OpenVDB)", fillcolor=lightgray, style="rounded,filled"];
       }
       
       // Application -> State Management
       PE -> Registry [label="reads/writes", color=blue];
       PE -> VoxelGrid [label="reads/writes", color=blue];
       PE -> PM [label="queries", style=dotted, color=purple];
       EE -> Registry [label="reads/writes", color=blue];
       EE -> VoxelGrid [label="reads/writes", color=blue];
       EE -> PM [label="queries", style=dotted, color=purple];
       
       // State Management -> Storage
       VoxelGrid -> TGR [label="manages", color=green];
       VoxelGrid -> EG [label="manages", color=green];
       VoxelGrid -> EvG [label="manages", color=green];
       VoxelGrid -> LG [label="manages", color=green];
       TGR -> Registry [label="syncs", style=dashed, color=blue];
       TGR -> TS [label="manages", color=green];
       
       // Cross-layer synchronization (shown at edges)
       Registry -> TS [style=dotted, color=red, label="indirect sync"];
   }


Components
----------

This section documents each component shown in the architecture diagrams above, organized by abstraction layer from storage to application.

Layer 3: Storage & Data Access
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

entt::registry (ECS)
^^^^^^^^^^^^^^^^^^^^

**Purpose**: Core Entity Component System that manages all game entities and their components.

**Location**: External library (EnTT), integrated throughout the codebase.

**Key Components Managed**:

- ``Position`` - 3D coordinates (x, y, z) and direction
- ``Velocity`` - Movement velocity (vx, vy, vz) for physics simulation
- ``EntityTypeComponent`` - Entity classification (mainType, subType0, subType1)
- ``MovingComponent`` - Animation state for entities in transit
- ``PhysicsStats`` - Mass, speed limits, forces, heat
- ``StructuralIntegrityComponent`` - Stack behavior, load capacity, matter state
- ``MatterContainer`` - Terrain/water/vapor/biomass quantities
- ``HealthComponents``, ``MetabolismComponents`` - Creature state

**Usage Patterns**:

.. code-block:: cpp

   // Iterate entities with specific components
   auto view = registry.view<Position, Velocity, MovingComponent>();
   for (auto entity : view) {
       auto& pos = view.get<Position>(entity);
       auto& vel = view.get<Velocity>(entity);
       // Process movement...
   }

**Architectural Role**: Central data store for all transient entity state. Works in tandem with ``VoxelGrid`` (spatial indexing) and ``TerrainStorage`` (persistent terrain data). Static terrain lives in OpenVDB; dynamic behavior lives in ECS.

TerrainStorage
^^^^^^^^^^^^^^

**Purpose**: Low-level OpenVDB-backed storage for all static terrain data, providing sparse and memory-efficient storage.

**Location**: :file:`include/terrain_storage.hpp`, :file:`src/terrain_storage.cpp`

**Key OpenVDB Grids**:

- ``entityGrid`` - Terrain entity ID (source of truth: -2=empty, -1=VDB-only, ≥0=ECS entity)
- ``mainTypeGrid`` - Entity main type (TERRAIN, PLANT, BEAST, etc.)
- ``subType0Grid``, ``subType1Grid`` - Subtype classifications
- ``terrainMatterGrid``, ``waterMatterGrid``, ``vaporMatterGrid``, ``biomassMatterGrid`` - Matter quantities
- ``massGrid``, ``maxSpeedGrid``, ``minSpeedGrid``, ``heatGrid`` - Physical properties
- ``flagsGrid`` - Packed bit flags (direction, canStack, matterState, gradient)
- ``maxLoadGrid`` - Load capacity for structural integrity

**Key Methods**:

- ``initialize()`` / ``initializeWithTransform()`` - Grid setup
- ``getTotalMemoryUsage()`` - Memory profiling
- ``getEntityId()`` / ``setEntityId()`` - Activity state management
- Individual getters/setters for all terrain attributes
- ``isActive()`` - Check if voxel has dynamic behavior
- ``pruneInactive()`` - Memory cleanup for inactive regions
- ``iterateWaterVoxels()``, ``iterateVaporVoxels()``, ``iterateBiomassVoxels()`` - Efficient matter iteration

**Thread Safety**: Uses ``ThreadLocalAccessorCache`` for thread-local accessor caching, enabling O(1) voxel access without locking.

**Architectural Role**: Foundation layer for terrain data. OpenVDB's sparse storage only stores non-default values, making it extremely memory-efficient for large worlds. All static terrain attributes are stored here; transient runtime behavior is handled by ``TerrainGridRepository`` + ECS.

EntityGrid (Int32Grid)
^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Spatial index for non-terrain entities (creatures, items, projectiles).

**Location**: Member of ``VoxelGrid`` (:file:`include/voxelgrid.hpp`)

**Implementation**: ``openvdb::Int32Grid::Ptr`` with default value ``-1`` (no entity).

**Thread Safety**: Protected by ``entityGridMutex`` (``std::shared_mutex``) for concurrent read access.

**Key Operations**:

- ``setEntityAt(x, y, z, entityId)`` - Place entity
- ``getEntityAt(x, y, z)`` - Query entity (thread-safe)
- ``getEntityAtFast(x, y, z)`` - Fast read without lock
- ``removeEntityAt(x, y, z)`` - Remove entity
- ``moveEntity(oldPos, newPos, entityId)`` - Relocate entity

**Architectural Role**: Enables fast "what entity is at position (x,y,z)?" queries essential for collision detection, interaction, and rendering. Separate from terrain because entities can move, be destroyed, or created dynamically.

EventGrid (Int32Grid)
^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Stores event IDs for tile effects and temporary spatial events (damage zones, buffs, environmental hazards).

**Location**: Member of ``VoxelGrid`` (:file:`include/voxelgrid.hpp`)

**Implementation**: ``openvdb::Int32Grid::Ptr`` with default value ``-1`` (no event).

**Related Components**: Event IDs reference ``TileEffectComponent`` entities containing damage type, value, and duration.

**Usage**: Enables location-based gameplay mechanics where multiple effects can stack at a position.

**Architectural Role**: Spatial index for tile-based events. Sparse storage means only active event locations consume memory, making it efficient for games with localized effects.

LightingGrid (FloatGrid)
^^^^^^^^^^^^^^^^^^^^^^^^^

**Purpose**: Stores lighting levels for each voxel position, enabling dynamic lighting and day/night cycles.

**Location**: Member of ``VoxelGrid`` (:file:`include/voxelgrid.hpp`)

**Implementation**: ``openvdb::FloatGrid::Ptr`` with default value ``0.0f`` (no light).

**Key Operations**:

- ``setLightAt(x, y, z, lightLevel)`` - Set light value
- ``getLightAt(x, y, z)`` - Query light level
- Region queries for lighting propagation

**Architectural Role**: Visual system support for lighting calculations. Used by renderer to determine voxel brightness. Supports features like light propagation, shadows, and day/night cycles.

Layer 2: State Management & Coordination
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VoxelGrid
^^^^^^^^^

**Purpose**: Central spatial data structure that integrates all grid-based storage and provides unified interface for spatial queries.

**Location**: :file:`include/voxelgrid.hpp`, :file:`src/voxelgrid.cpp`

**Key Members**:

- ``width_``, ``height_``, ``depth_`` - Grid dimensions
- ``registry_`` - Reference to ``entt::registry``
- ``terrainStorage_`` - OpenVDB-backed terrain storage
- ``terrainGridRepository_`` - ECS overlay for terrain
- ``entityGrid`` - Non-terrain entity placement
- ``eventGrid`` - Event ID grid
- ``lightingGrid`` - Lighting level grid
- ``entityGridMutex`` - Thread safety for entity grid

**Key Methods**:

**Initialization**:
  - ``initialize()`` - Initializes all OpenVDB grids

**Unified Access**:
  - ``getVoxel()`` / ``setVoxel()`` - Combined voxel data access

**Terrain Management**:
  - ``addTerrain()`` / ``getTerrain()`` / ``removeTerrain()``
  - ``hasTerrainAt()`` - Terrain existence check

**Entity Management**:
  - ``setEntityAt()`` / ``getEntityAt()`` / ``removeEntityAt()``
  - ``moveEntity()`` - Relocate entity to new position

**Spatial Queries**:
  - ``getTerrainVoxelsInBox()`` - Region queries for terrain
  - ``getEntitiesInBox()`` - Region queries for entities

**Persistence**:
  - ``save()`` / ``load()`` - World serialization

**Support Classes**:

- ``VoxelGridView`` - Read-only view for efficient region queries
- ``VoxelGridViewFlatB`` - FlatBuffers-optimized view for serialization

**Architectural Role**: Central spatial database that delegates terrain storage to ``TerrainStorage`` (OpenVDB), manages entity grid directly, and provides unified interface for spatial queries. Acts as the bridge between ECS (``entt::registry``) and spatial storage (OpenVDB).

TerrainGridRepository
^^^^^^^^^^^^^^^^^^^^^

**Purpose**: ECS overlay for terrain voxels, managing the boundary between static terrain data (OpenVDB) and transient behavior (ECS). Implements "activate on demand" pattern where terrain with dynamic behavior gets ECS entities.

**Location**: :file:`include/terrain_grid_repository.hpp`, :file:`src/terrain_grid_repository.cpp`

**Key Members**:

- ``registry_`` - ECS registry reference
- ``storage_`` - OpenVDB storage backend
- ``byCoord_`` - Coordinate → Entity mapping (robin_map)
- ``byEntity_`` - Entity → Coordinate mapping (bidirectional)
- ``terrainGridLocked_`` - Lock tracking flag
- ``mutex_`` - Thread safety

**Static Data Methods** (VDB-backed):

- ``getEntityType()`` / ``setEntityType()``
- ``getMatterQuantities()`` / ``setMatterQuantities()``
- ``getPhysicsStats()`` / ``setPhysicsStats()``
- ``getStructuralIntegrity()`` / ``setStructuralIntegrity()``
- Individual getters/setters for mainType, subType0, mass, speed, etc.

**Transient Data Methods** (ECS-backed):

- ``getVelocity()`` / ``setVelocity()`` - Auto-activates terrain
- ``hasMovingComponent()`` - Check for moving behavior

**Lifecycle Management**:

- ``deactivateTerrain()`` - Migrates terrain entity from ECS to OpenVDB
- ``terrainExists()`` - Existence check
- ``removeTerrain()`` - Removes terrain
- ``moveTerrain()`` - Moves terrain voxel to new location
- ``createEntityForInactiveTerrain()`` - Creates ECS entity
- ``activateTerrain()`` / ``isTerrainActive()`` - Activity state

**Iterator Methods**:

- ``iterateWaterVoxels()`` - Efficient water iteration
- ``iterateVaporVoxels()`` - Efficient vapor iteration
- ``iterateBiomassVoxels()`` - Efficient biomass iteration
- ``iterateActiveTerrains()`` - Iterate terrain with ECS entities

**Helper Methods**:

- ``getTerrainData()`` - Combines static and transient data
- ``tick()`` - Updates transient systems, auto-deactivates when idle
- ``lockTerrainGrid()`` / ``unlockTerrainGrid()`` - External synchronization

**Architectural Role**: Critical arbitration layer between static terrain (OpenVDB) and dynamic behavior (ECS). Implements "cold storage" pattern: inactive terrain is VDB-only (memory efficient), active terrain gets ECS entity (full simulation). The bidirectional maps (``byCoord_``, ``byEntity_``) enable fast lookups in both directions.

PhysicsManager
^^^^^^^^^^^^^^

**Purpose**: Singleton configuration class that stores global physics constants and game balance parameters.

**Location**: :file:`include/physics_manager.hpp`, :file:`src/physics_manager.cpp`

**Key Configuration Parameters**:

- ``gravity_`` - Gravitational acceleration (default 5.0)
- ``friction_`` - Friction coefficient (default 1.0)
- ``allowSimultaneousMovement_`` - Multi-axis movement (default true)
- ``evaporationRate_`` - Water evaporation rate (default 8.0)
- ``evaporationHeatThreshold_`` - Heat required (default 120.0)
- ``evaporationMinQuantity_`` - Minimum water threshold (default 120,000)
- ``energyCostPerMovement_`` - Energy cost (default 0.000002)

**Key Methods**:

- ``getInstance()`` - Singleton accessor
- ``getGravity()`` / ``setGravity()``
- ``getFriction()`` / ``setFriction()``
- ``getAllowSimultaneousMovement()`` / ``setAllowSimultaneousMovement()``
- ``getEnergyCostPerMovement()`` / ``setEnergyCostPerMovement()``
- ``getEvaporationRate()`` / ``setEvaporationRate()``
- ``save()`` / ``load()`` - Persistence (not yet implemented)

**Design Pattern**: Classic Singleton with typedef ``PhysicsManagerPtr`` for convenient access.

**Architectural Role**: Global configuration hub for all physics-related constants. Accessed by ``PhysicsEngine``, ``EcosystemEngine``, and other systems that need physics parameters. Enables runtime tuning of game balance without recompilation.

Layer 1: Application/Engine Layer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PhysicsEngine
^^^^^^^^^^^^^

**Purpose**: Handles physics simulation for all entities including gravity, movement, collision detection, and velocity-based motion.

**Location**: :file:`include/physics_engine.hpp`, :file:`src/physics_engine.cpp`

**Key Members**:

- ``registry_`` - ECS registry reference
- ``eventDispatcher_`` - Event dispatcher for physics events
- ``voxelGrid_`` - Pointer to voxel grid for spatial queries
- ``mutex_`` - Thread safety
- ``processingAsync_`` - Flag for async processing state
- ``debugEntity_`` - Entity for debugging

**Key Methods**:

**Synchronous Updates**:
  - ``update(deltaTime)`` - Main physics update for entities with Velocity and MovingComponent

**Asynchronous Processing**:
  - ``processAsync(deltaTime)`` - Checks for falling entities, runs in background

**Event Handlers**:
  - ``handleSolidEntityMovement()`` - Event handler for solid entity movement
  - ``handleGasEntityMovement()`` - Event handler for gas entity movement

**Internal Systems**:
  - ``velocityToMovement()`` - Converts physics velocities to grid movement
  - ``canJumpCheck()`` - Determines if entity can jump

**Internal Helper Functions** (in .cpp):

- ``checkCollision()`` - Collision detection
- ``applyGravity()`` - Applies gravitational forces
- ``processMovement()`` - Processes entity movement with collision
- ``handleMovementComplete()`` - Handles completion of movement animation

**Architectural Role**: Primary physics system that processes movement, collision, and applies forces. Interacts heavily with ``VoxelGrid`` for spatial queries and ``entt::registry`` for component updates. Queries ``PhysicsManager`` for constants like gravity and friction.

EcosystemEngine
^^^^^^^^^^^^^^^

**Purpose**: Manages ecosystem simulation including water flow, evaporation, condensation, and environmental effects. Features sophisticated parallel water simulation.

**Location**: :file:`include/ecosystem_engine.hpp`, :file:`src/ecosystem_engine.cpp`

**Key Members**:

- ``waterSimulationManager_`` - Parallel water simulation manager
- ``evaporationQueue_`` - Queue for evaporation events
- ``condensationQueue_`` - Queue for condensation events
- ``waterFallingQueue_`` - Queue for water falling events
- ``mutex_`` - Thread safety
- ``processingAsync_`` - Processing state flag
- ``debugEntity_`` - Debug entity

**Key Methods**:

**Main Loop**:
  - ``update(deltaTime)`` - Main ecosystem update loop
  - ``processAsync(deltaTime)`` - Asynchronous ecosystem processing

**Water Simulation**:
  - ``processParallelWaterSimulation()`` - Parallel water simulation using thread pool

**Event Processing**:
  - ``processEvaporation()`` - Handles queued evaporation
  - ``processCondensation()`` - Handles queued condensation
  - ``processWaterFalling()`` - Handles queued water falling

**Iteration**:
  - ``iterateTerrainTiles()`` - Iterates over tiles for ecosystem processing

**Nested Classes**:

- ``WaterSimulationManager`` - Coordinates parallel water simulation with worker threads, grid box partitioning (32x32x32 chunks), and round-robin scheduler
- ``GridBoxProcessor`` - Processes water simulation for specific grid region using thread-local OpenVDB accessors for optimal cache performance
- ``RoundRobinScheduler`` - Priority queue-based task scheduler with aging for fair task distribution

**Architectural Role**: Central system for environmental simulation. Uses OpenVDB-backed terrain storage for efficient water matter queries and sophisticated parallel processing architecture with grid box partitioning for cache-friendly iteration. Queries ``PhysicsManager`` for evaporation rates and thresholds.

Water Cycle Simulation
-----------------------

The ``EcosystemEngine`` implements a complete water cycle with multiple phases that interact to create realistic environmental dynamics. The cycle operates on terrain voxels stored in ``TerrainStorage`` and uses matter quantities (``terrainMatter``, ``waterMatter``, ``vaporMatter``) to track state changes.

Overview State Diagram
~~~~~~~~~~~~~~~~~~~~~~

A high-level view of the water cycle states and phase transitions:

.. graphviz::

   digraph water_cycle_overview {
       rankdir=TB;
       node [shape=box, style="rounded,filled", fontsize=12];
       edge [fontsize=10, penwidth=1.5];
       
       // Main states
       Liquid [label="Liquid Water\n\nwaterMatter > 0\nFlows, Pools, Falls", 
               fillcolor="#4682B4", fontcolor=white, style="rounded,filled"];
       
       Vapor [label="Water Vapor\n\nvaporMatter > 0\nDiffuses, Rises", 
              fillcolor="#87CEEB", fontcolor=black, style="rounded,filled"];
       
       // Phase transitions
       Liquid -> Vapor [label="  Evaporation  \n\nheat ≥ 120.0\nEndothermic", 
                        color="#FF6347", penwidth=2, fontcolor="#FF6347"];
       
       Vapor -> Liquid [label="  Condensation  \n\nvapor > saturation\nExothermic", 
                        color="#32CD32", penwidth=2, fontcolor="#32CD32"];
       
       // Internal state transitions
       Liquid -> Liquid [label="Flow\n(pressure)", color="#1E90FF", style=dashed];
       Liquid -> Liquid [label="Rain\n(gravity)", color="#FF8C00", dir=back];
       
       Vapor -> Vapor [label="Diffusion\n(gradient)", color="#4169E1", style=dotted];
       Vapor -> Vapor [label="Rise\n(buoyancy)", color="#4169E1", dir=back, style=dotted];
       
       // Conservation note
       note [label="Conservation Laws:\n• Matter: Σ(water + vapor) = const\n• Energy: evap (−heat) ⇔ cond (+heat)", 
             shape=note, fillcolor="#FFFACD", fontsize=10];
   }

**Key Properties**:

- **Phase Transitions**: Evaporation (endothermic) ⇔ Condensation (exothermic)
- **Internal Dynamics**: Flow, diffusion, rise, precipitation
- **Conservation**: Matter and energy preserved across all transitions
- **Parallelization**: 32³ voxel grid boxes, thread-local OpenVDB accessors

Phase 1: Liquid Water Movement
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Trigger**: Water voxels with sufficient matter quantity (> 0) seek equilibrium with neighbors.

**Process**:

1. **Neighbor Analysis**: For each water voxel at (x, y, z), check 6 cardinal neighbors (±x, ±y, ±z)
2. **Pressure Calculation**: Calculate water pressure based on matter quantity and vertical position
3. **Flow Direction**: Water flows from high pressure to low pressure (downward priority, then lateral)
4. **Matter Transfer**: Transfer water matter proportional to pressure difference

**Implementation Details**:

.. code-block:: cpp

   // Pseudo-code for water flow
   for (auto coord : waterVoxels) {
       float currentWater = storage.getWaterMatter(coord);
       
       // Check below first (gravity priority)
       Coord below = coord.offset(0, -1, 0);
       if (canFlowTo(below)) {
           float transfer = calculateFlowAmount(currentWater, storage.getWaterMatter(below));
           storage.setWaterMatter(coord, currentWater - transfer);
           storage.setWaterMatter(below, storage.getWaterMatter(below) + transfer);
       }
       
       // Check lateral neighbors for leveling
       for (Coord neighbor : getLateralNeighbors(coord)) {
           if (canFlowTo(neighbor)) {
               float avgLevel = (currentWater + storage.getWaterMatter(neighbor)) / 2.0f;
               // Transfer to reach equilibrium...
           }
       }
   }

**Parallelization**: ``WaterSimulationManager`` divides the world into 32x32x32 grid boxes, processes each box independently using thread-local OpenVDB accessors, preventing race conditions.

**Constraints**:

- Water cannot flow into solid terrain (mainType == TERRAIN with solid subType)
- Flow rate limited by ``PhysicsManager`` constants
- Matter conservation: total water quantity remains constant across transfers

Phase 2: Evaporation
~~~~~~~~~~~~~~~~~~~~~

**Trigger**: Water voxels with sufficient heat and matter quantity enter evaporation queue.

**Conditions** (from ``PhysicsManager``):

- ``heat >= evaporationHeatThreshold_`` (default: 120.0)
- ``waterMatter >= evaporationMinQuantity_`` (default: 120,000 units)
- Exposed to air (has empty neighbor above)

**Process**:

1. **Queue Population**: ``iterateTerrainTiles()`` scans water voxels, adds qualifying voxels to ``evaporationQueue_``
2. **Evaporation Processing**: ``processEvaporation()`` consumes queue:

   - Reduces ``waterMatter`` by ``evaporationRate_`` (default: 8.0 units/tick)
   - Increases ``vaporMatter`` by same amount (matter conservation)
   - Reduces ``heat`` proportionally (evaporation is endothermic)
   - If ``waterMatter`` reaches 0, converts voxel to vapor-only

**Energy Balance**:

.. code-block:: cpp

   float evaporated = std::min(waterMatter, evaporationRate);
   waterMatter -= evaporated;
   vaporMatter += evaporated;
   heat -= evaporated * HEAT_PER_EVAPORATION;  // Cooling effect

**Architectural Notes**: Evaporation queue prevents immediate state changes during iteration, ensuring consistency when using parallel grid box processing.

Phase 3: Vapor Transport
~~~~~~~~~~~~~~~~~~~~~~~~~

**Trigger**: Vapor voxels (``vaporMatter > 0``) diffuse through atmosphere.

**Process**:

1. **Diffusion**: Vapor spreads to neighboring empty voxels following concentration gradient
2. **Vertical Rise**: Vapor has slight upward bias (buoyancy), accumulating at higher altitudes
3. **Wind Effects** (if implemented): Horizontal transport based on wind velocity fields

**Diffusion Algorithm**:

.. code-block:: cpp

   // Simplified vapor diffusion
   for (auto coord : vaporVoxels) {
       float currentVapor = storage.getVaporMatter(coord);
       float avgConcentration = currentVapor;
       
       for (Coord neighbor : getNeighbors(coord)) {
           avgConcentration += storage.getVaporMatter(neighbor);
       }
       avgConcentration /= 7.0f;  // Current + 6 neighbors
       
       // Transfer toward average (diffusion)
       float transfer = (currentVapor - avgConcentration) * DIFFUSION_RATE;
       // Apply transfers...
   }

**Constraints**:

- Vapor cannot enter solid terrain
- Vapor accumulates in enclosed spaces (caves, buildings)
- Diffusion rate slower than liquid water flow (gas vs liquid dynamics)

Phase 4: Condensation
~~~~~~~~~~~~~~~~~~~~~~

**Trigger**: Vapor voxels at high altitude or low temperature become saturated.

**Conditions**:

- ``vaporMatter`` exceeds saturation threshold (altitude-dependent)
- ``heat`` falls below condensation temperature
- Presence of condensation nuclei (terrain, particles)

**Process**:

1. **Saturation Check**: Calculate saturation capacity based on temperature and altitude
2. **Nucleation**: Vapor condenses on solid surfaces or forms droplets in free air
3. **Matter Conversion**: Excess vapor converts to liquid water
4. **Heat Release**: Condensation is exothermic, releases heat to surroundings

.. code-block:: cpp

   float saturationCapacity = calculateSaturation(altitude, heat);
   if (vaporMatter > saturationCapacity) {
       float condensed = vaporMatter - saturationCapacity;
       vaporMatter -= condensed;
       waterMatter += condensed;
       heat += condensed * HEAT_PER_CONDENSATION;  // Warming effect
   }

**Cloud Formation**: High concentrations of vapor at altitude create visible cloud layers (rendered via ``vaporMatter`` density).

Phase 5: Precipitation (Rain)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Trigger**: Water voxels at high altitude with no solid support below fall as rain.

**Conditions**:

- ``waterMatter > 0`` at altitude ``y > RAIN_THRESHOLD``
- Voxel below is empty or also water (no solid support)
- Sufficient accumulation (prevents single-drop rain)

**Process**:

1. **Rain Queue**: ``iterateTerrainTiles()`` identifies falling water, adds to ``waterFallingQueue_``
2. **Falling Physics**: ``processWaterFalling()`` applies:

   - **Vertical Acceleration**: Water accelerates downward at ``gravity`` rate
   - **Terminal Velocity**: Capped by air resistance
   - **Collision Detection**: Checks each voxel during fall path
   - **Splashing**: On impact with solid terrain or water body:
     
     - Transfers ``waterMatter`` to impact location
     - Lateral splash if impact velocity high
     - Kinetic energy converts to heat (impact warming)

**Rain Animation**:

.. code-block:: cpp

   // Rain falling process
   for (auto rainEvent : waterFallingQueue_) {
       Coord current = rainEvent.position;
       float velocity = rainEvent.velocity;
       
       while (current.y > 0) {
           Coord below = current.offset(0, -1, 0);
           
           if (storage.isOccupied(below)) {
               // Impact!
               handleSplash(below, rainEvent.waterAmount, velocity);
               break;
           }
           
           // Continue falling
           current = below;
           velocity = std::min(velocity + gravity, TERMINAL_VELOCITY);
       }
   }

**Weather Patterns**: Combination of evaporation (ocean/lakes), vapor transport (wind), condensation (altitude/cooling), and precipitation (rain) creates emergent weather systems.

Water Cycle State Diagram
~~~~~~~~~~~~~~~~~~~~~~~~~~

The complete water cycle state machine with all transitions, conditions, and attributes:

.. graphviz::

   digraph water_cycle_states {
       rankdir=LR;
       node [shape=box, style="rounded,filled"];
       edge [fontsize=8];
       
       // States with attributes
       Terrain [label="Solid Terrain\n\nAttributes:\n• terrainMatter > 0\n• mainType = TERRAIN\n• subType = solid\n• heat (float)\n• position (x,y,z)\n\nInvariants:\n• Cannot move\n• Blocks flow\n• Condensation nuclei", 
                fillcolor="#8B7355", fontcolor=white, style="rounded,filled"];
       
       WaterStatic [label="Liquid Water (Static)\n\nAttributes:\n• waterMatter > 0\n• heat (float)\n• pressure (calculated)\n• velocity ≈ 0\n\nConditions:\n• Has support below OR\n  at equilibrium\n• y ≤ RAIN_THRESHOLD",
                    fillcolor="#4682B4", fontcolor=white, style="rounded,filled"];
       
       WaterFlowing [label="Liquid Water (Flowing)\n\nAttributes:\n• waterMatter > 0\n• pressure gradient\n• flow_direction (x,y,z)\n• flow_rate (float)\n\nBehavior:\n• Downward priority\n• Lateral equilibrium\n• Rate-limited",
                     fillcolor="#1E90FF", fontcolor=white, style="rounded,filled"];
       
       WaterFalling [label="Liquid Water (Falling)\n\nAttributes:\n• waterMatter > 0\n• velocity (increasing)\n• acceleration = gravity\n• terminal_velocity (max)\n\nPhysics:\n• velocity += gravity\n• Collision detection\n• Kinetic energy",
                     fillcolor="#00BFFF", fontcolor=white, style="rounded,filled"];
       
       VaporDispersed [label="Water Vapor (Dispersed)\n\nAttributes:\n• vaporMatter > 0\n• concentration (float)\n• heat (float)\n• altitude (y)\n\nConditions:\n• vaporMatter < saturation\n• No nucleation\n\nBehavior:\n• Diffusion\n• Cannot enter solids",
                      fillcolor="#E0FFFF", fontcolor=black, style="rounded,filled"];
       
       VaporRising [label="Water Vapor (Rising)\n\nAttributes:\n• vaporMatter > 0\n• vertical_bias (upward)\n• buoyancy (float)\n• altitude (increasing)\n\nBehavior:\n• Accumulates at height\n• Forms cloud layers",
                    fillcolor="#B0E0E6", fontcolor=black, style="rounded,filled"];
       
       VaporSaturated [label="Water Vapor (Saturated)\n\nAttributes:\n• vaporMatter > saturation\n• excess = vapor - capacity\n• saturation = f(altitude, heat)\n\nConditions:\n• Ready to condense\n• Nuclei present",
                      fillcolor="#87CEEB", fontcolor=black, style="rounded,filled"];
       
       // State transitions
       
       // Water flow
       WaterStatic -> WaterFlowing [label="Pressure Gradient\n\nTrigger:\n• Neighbor pressure diff\n• No solid obstacle\n\nRate: ∝ pressure_diff", color="#1E90FF", fontcolor="#1E90FF"];
       WaterFlowing -> WaterStatic [label="Equilibrium Reached\n\nCondition:\n• Pressure balanced\n• Flow complete", color="#4682B4", fontcolor="#4682B4"];
       
       // Evaporation
       WaterStatic -> VaporDispersed [label="EVAPORATION\n\nConditions (ALL):\n• heat ≥ 120.0\n• waterMatter ≥ 120,000\n• Exposed to air\n\nProcess:\n• waterMatter -= rate (8.0)\n• vaporMatter += rate\n• heat -= rate × HEAT_EVAP\n\nEffect: Endothermic (cooling)", 
                                        color="#FF6347", fontsize=9, penwidth=2];
       
       WaterFlowing -> VaporDispersed [label="EVAPORATION", color="#FF6347", style=dashed];
       
       // Vapor diffusion
       VaporDispersed -> VaporDispersed [label="DIFFUSION\n\nProcess:\n• Spread to neighbors\n• Follow gradient\n• Rate: DIFFUSION_RATE\n\nConservation:\n• Total vapor constant", 
                                          color="#87CEEB", style=dotted];
       
       // Vapor rise
       VaporDispersed -> VaporRising [label="Buoyancy\n\nTrigger:\n• Heat buoyancy\n• Space above\n\nEffect:\n• Altitude increase\n• Cloud formation", color="#4169E1"];
       
       // Saturation
       VaporRising -> VaporSaturated [label="Altitude/Cooling\n\nCondition:\n• vapor > saturation OR\n• heat < threshold\n\nSaturation ∝ 1/altitude", color="#4682B4"];
       
       VaporDispersed -> VaporSaturated [label="Cooling", color="#4682B4", style=dashed];
       
       // Condensation
       VaporSaturated -> WaterStatic [label="CONDENSATION\n\nConditions:\n• vapor > capacity\n• Nuclei present\n\nProcess:\n• vaporMatter -= excess\n• waterMatter += excess\n• heat += excess × HEAT_COND\n\nEffect: Exothermic (warming)", 
                                       color="#32CD32", fontsize=9, penwidth=2];
       
       // Rain formation
       WaterStatic -> WaterFalling [label="Rain Formation\n\nConditions (ALL):\n• waterMatter > 0\n• y > RAIN_THRESHOLD\n• No support below\n• Sufficient accumulation\n\nQueued: waterFallingQueue_", color="#FF8C00"];
       
       // Rain descent
       WaterFalling -> WaterFalling [label="Falling\n\nPhysics:\n• vel += gravity\n• vel ≤ TERMINAL_VEL\n• y -= 1 per tick\n\nCollision: Checked", 
                                      color="#FFA500", style=dotted];
       
       // Rain impact
       WaterFalling -> WaterStatic [label="IMPACT/SPLASH\n\nTrigger:\n• Hit terrain/water\n• velocity > 0\n\nEffects:\n• Transfer waterMatter\n• KE → heat\n• Lateral splash if high velocity\n\nConservation:\n• Matter preserved\n• Energy → thermal", 
                                     color="#8B4513", fontsize=9, penwidth=2];
       
       WaterFalling -> WaterFlowing [label="Impact → Flow", color="#8B4513", style=dashed];
       
       // Terrain interactions
       Terrain -> WaterStatic [label="Container/Support", color="#696969", style=dotted];
       Terrain -> VaporSaturated [label="Nucleation Site", color="#696969", style=dotted];
       
       // Conservation note
       {rank=same; WaterStatic; VaporDispersed;}
       {rank=same; WaterFlowing; VaporRising;}
       {rank=same; WaterFalling; VaporSaturated;}
   }

**State Machine Properties**:

- **Matter Conservation**: ``Σ(waterMatter) + Σ(vaporMatter) = CONSTANT`` across all transitions
- **Energy Conservation**: Evaporation (endothermic) ⇔ Condensation (exothermic) + Kinetic energy transfers
- **Parallelization**: Grid box partitioning (32³ voxels) with thread-local OpenVDB accessors
- **Queue-Based**: Evaporation and rain use queues (``evaporationQueue_``, ``waterFallingQueue_``) to prevent race conditions

Cycle Feedback Loops
~~~~~~~~~~~~~~~~~~~~~

The water cycle exhibits several feedback mechanisms:

**Positive Feedback**:

- **Evaporative Cooling → Less Evaporation**: Evaporation cools water, reducing further evaporation rate
- **Condensation Warming → More Evaporation**: Condensation releases heat, increasing nearby evaporation

**Negative Feedback**:

- **Rain Replenishment**: Rain refills water bodies, sustaining evaporation sources
- **Altitude Limit**: Water vapor cannot rise indefinitely, limiting cloud height

**Energy Conservation**: The cycle is thermodynamically consistent:

- Evaporation absorbs heat (endothermic)
- Condensation releases heat (exothermic)
- Net energy transfer from hot regions (surface) to cool regions (altitude)

**Matter Conservation**: Total water across all phases (liquid + vapor) remains constant:

.. code-block:: cpp

   totalWater = Σ(waterMatter) + Σ(vaporMatter)  // Constant per game world

**Implementation Notes**: The parallel processing architecture (``WaterSimulationManager``) ensures all phases process efficiently at scale, handling millions of voxels with minimal latency.

Design Patterns
---------------

The architecture employs several key design patterns to achieve both performance and flexibility:

Hybrid Storage Pattern
~~~~~~~~~~~~~~~~~~~~~~~

The system uses a dual-storage approach combining OpenVDB (sparse, disk-friendly) with ECS (dense, CPU-friendly):

- **Static Data**: Terrain attributes that rarely change (type, mass, structure) live in ``TerrainStorage`` OpenVDB grids
- **Dynamic Data**: Transient runtime state (velocity, animation, AI state) lives in ``entt::registry`` components
- **Benefit**: Memory efficiency for large worlds while maintaining fast access for active simulation

Lazy Activation Pattern
~~~~~~~~~~~~~~~~~~~~~~~~

``TerrainGridRepository`` implements "cold storage" optimization:

- **Inactive Terrain**: Stored only in OpenVDB (no ECS entity) - minimal memory footprint
- **Active Terrain**: When terrain gains dynamic behavior (velocity, movement), ECS entity is created automatically
- **Auto-Deactivation**: ``tick()`` method monitors active terrains and deactivates idle ones
- **Benefit**: Millions of static voxels consume minimal memory; only active regions pay ECS overhead

Bidirectional Mapping Pattern
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``TerrainGridRepository`` maintains two hash maps for O(1) lookups in both directions:

- ``byCoord_`` (robin_map): Coordinate → Entity ID mapping
- ``byEntity_`` (unordered_map): Entity ID → Coordinate mapping
- **Use Cases**: Fast "what entity at (x,y,z)?" and "where is entity E?" queries
- **Benefit**: Enables efficient spatial queries and entity tracking without scanning

Thread-Local Caching Pattern
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``TerrainStorage`` uses ``ThreadLocalAccessorCache`` for OpenVDB access:

.. code-block:: cpp

   // Each thread gets its own accessor (no locking)
   class ThreadLocalAccessorCache {
       thread_local static std::unordered_map<void*, std::shared_ptr<void>> cache;
   };

- **Benefit**: O(1) voxel access without mutex overhead in parallel algorithms
- **Trade-off**: Small per-thread memory cost for massive concurrency gains
- **Usage**: Critical for ``EcosystemEngine``'s parallel water simulation (32x32x32 grid boxes processed concurrently)

Sparse Storage Pattern
~~~~~~~~~~~~~~~~~~~~~~~

OpenVDB's sparse voxel database (SVD) only stores non-default values:

- **Empty Space**: Consumes near-zero memory (only tree structure metadata)
- **Active Voxels**: Stored in 8³ leaf nodes with compression
- **Benefit**: Worlds with vast empty regions (sky, underground) are memory-efficient
- **Trade-off**: Random access slightly slower than dense arrays; sequential iteration via iterators is optimal

Grid Box Partitioning Pattern
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``EcosystemEngine``'s ``WaterSimulationManager`` divides the world into cache-friendly chunks:

- **Grid Boxes**: World divided into 32x32x32 voxel regions
- **Parallel Processing**: Each box processed by separate thread with thread-local OpenVDB accessors
- **Round-Robin Scheduling**: Fair task distribution with priority aging to prevent starvation
- **Benefit**: Cache-friendly access patterns, minimal thread contention, scales with core count

Data Flow
---------

1. **State Sharing**: Both engines read and write to the ``VoxelGrid`` and ``entt::registry``.

2. **Terrain Access Pattern**: 
   - Direct terrain queries use ``TerrainGridRepository::getTerrainIdIfExists()`` for O(1) lookups.
   - Terrain modifications go through the repository to maintain consistency between EnTT and grid storage.
   - For moving entities, ``TerrainGridRepository::moveTerrain()`` updates both coordinate maps and storage grids.

3. **Synchronization**: 

   - The ``PhysicsEngine`` typically runs on the main thread or a dedicated physics thread.
   - The ``EcosystemEngine`` uses a worker thread pool for parallel processing of grid chunks via ``GridBoxProcessor``.
   - Synchronization is achieved through:
   
     - ``entityGridMutex`` (std::shared_mutex) for EntityGrid read/write protection
     - ``terrainGridLocked_`` flag for terrain grid state
     - Explicit locking of TerrainStorage when needed

4. **OpenVDB Integration**:

   - All grids use OpenVDB's sparse data structure for memory efficiency.
   - Grid transforms are applied uniformly (1.0-voxel spacing by default).
   - Iterators (``cbeginValueOn()``) efficiently traverse only active voxels.
