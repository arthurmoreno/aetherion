#include "World.hpp"

#include "physics/PhysicsManager.hpp"

// #include <pybind11/stl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <execution>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "PerceptionResponse_generated.h"
#include "WorldExceptions.hpp"
#include "components/WaterStressComponent.hpp"
#include "diag/Diag.hpp"
#include "ecosystem/EcosystemEvents.hpp"
#include "flatbuffers/flatbuffers.h"
#include "physics/PhysicsMutators.hpp"
#include "voxelgrid/VoxelGrid.hpp"

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

World::World(int width, int height, int depth)
    : workerSink_(),
      eventSink_(dispatcher, workerSink_, std::this_thread::get_id()),
      voxelGrid(new VoxelGrid(registry)), pyRegistry(registry, dispatcher),
      // Update to use just SQLite file path parameter
      dbHandler(std::make_unique<GameDBHandler>("./data/game.sqlite")),
      physicsEngine(new PhysicsEngine(registry, eventSink_, voxelGrid)),
      lifeEngine(new LifeEngine(registry, eventSink_, voxelGrid)),
      ecosystemEngine(new EcosystemEngine()),
      metabolismSystem(new MetabolismSystem(registry, voxelGrid)),
      combatSystem(new CombatSystem(registry, voxelGrid)),
      effectsSystem(new EffectsSystem(registry, voxelGrid)),
      healthSystem(new HealthSystem(registry, voxelGrid)) {
  // Initialize World dimensions
  this->width = width;
  this->height = height;
  this->depth = depth;

  voxelGrid->initializeGrids();
  voxelGrid->width = width;
  voxelGrid->height = height;
  voxelGrid->depth = depth;

  // Initialise the diag registry against this World's GameDB before any
  // engine constructor registers a Counter / Gauge / EventLogger.
  // reset_for_testing() clears stale handles from a previous World in the
  // same process (pytest constructs and destructs many Worlds).
  aetherion::diag::Registry::instance().reset_for_testing();
  aetherion::diag::Registry::instance().initialize(
      {.gamedb_handler = dbHandler.get()});

  // Register diag counters owned by each engine.
  physicsEngine->registerDiagCounters();

  // Register event handlers
  physicsEngine->registerEventHandlers(dispatcher);
  physicsEngine->registerVoxelGrid(voxelGrid);
  lifeEngine->registerEventHandlers(dispatcher);
  ecosystemEngine->registerEventHandlers(dispatcher);
  ecosystemEngine->waterSimManager_->initializeProcessors(registry, *voxelGrid,
                                                          eventSink_);

  if (!Py_IsInitialized()) {
    std::cout << "Python was not initialized! Starting python interpreter."
              << std::endl;
    // nb::scoped_interpreter guard{};
  }
  Logger::getLogger()->info("World created with an empty voxel grid!");
}

World::~World() {
  // TBB requires task_group::wait() before destruction (the dtor
  // asserts on outstanding work). Block here so any in-flight physics
  // or ecosystem task completes before we tear down the registry,
  // VoxelGrid, or engines they're still touching. Catches and
  // discards any task exception — the destructor must not throw.
  try {
    asyncTasks_.wait();
  } catch (const std::exception &e) {
    Logger::getLogger()->error(std::string("~World: in-flight async task "
                                           "threw during shutdown: ") +
                               e.what());
  } catch (...) {
    Logger::getLogger()->error("~World: in-flight async task threw "
                               "non-std::exception during shutdown");
  }

  releasePythonState();    // Drop Python refs before any other member runs
  delete voxelGrid;        // Clean up the VoxelGrid
  delete physicsEngine;    // Clean up the physics engine
  delete lifeEngine;       // Clean up the physics engine
  delete ecosystemEngine;  // Clean up the ecosystem
  delete metabolismSystem; // Clean up the metabolismSystem
  delete combatSystem;
  delete effectsSystem;
  delete healthSystem;
  // openvdb::uninitialize();
}

void World::removeEntity(entt::entity entity) {
  destroyEntityHandleWithLifecycleLock(entity);
}

// Destroy only the EnTT entity handle. Caller must hold appropriate lifecycle
// locks.
void World::destroyEntityHandle(entt::entity entity) {
  // Delegate to physics mutator which handles validation, grid cleanup and
  // destruction.
  destroyEntityWithGridCleanup(registry, *voxelGrid, eventSink_, entity, true);
}

// Acquire the lifecycle mutex exclusively and destroy the entity handle.
// This helper is useful for callers that don't already hold
// `entityLifecycleMutex`.
void World::destroyEntityHandleWithLifecycleLock(entt::entity entity) {
  std::unique_lock lifecycleLock(entityLifecycleMutex);
  // Delegate to the existing destroyEntityHandle which assumes caller holds
  // lifecycle guarantees
  destroyEntityHandle(entity);
}

void World::initializeVoxelGrid() {
  // Initializes the grid with default GridData
  voxelGrid->initializeGrids();
}

void World::setVoxel(int x, int y, int z, const GridData &data) {
  voxelGrid->setVoxel(x, y, z, data);
}

GridData World::getVoxel(int x, int y, int z) const {
  return voxelGrid->getVoxel(x, y, z);
}

void World::setTerrain(int x, int y, int z,
                       const EntityInterface &entityInterface) {
  // Create a new entity in the EnTT registry
  // entt::entity entity = registry.create();

  // Assign Position and Velocity components from the EntityInterface
  // if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
  //     registry.emplace<Position>(entity,
  //     entityInterface.getComponent<Position>());
  // }

  // int terrainID = static_cast<int>(entity);
  // voxelGrid->setTerrain(x, y, z, terrainID);
  throw std::runtime_error("World::setTerrain not implemented yet");
}

// Create an entity in the EnTT registry with data from EntityInterface
entt::entity World::createEntity(const EntityInterface &entityInterface) {
  // Create a new entity in the EnTT registry
  entt::entity entity = registry.create();

  // Assign Position and Velocity components from the EntityInterface
  if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
    registry.emplace<Position>(entity,
                               entityInterface.getComponent<Position>());
  }
  if (entityInterface.hasComponent(ComponentFlag::VELOCITY)) {
    registry.emplace<Velocity>(entity,
                               entityInterface.getComponent<Velocity>());
  }
  if (entityInterface.hasComponent(ComponentFlag::HEALTH)) {
    registry.emplace<HealthComponent>(
        entity, entityInterface.getComponent<HealthComponent>());
  }

  // Add the entity to the VoxelGrid with its position

  if (entityInterface.hasComponent(ComponentFlag::POSITION)) {
    GridData gridData = {1, static_cast<int>(entity), 0,
                         0.0f}; // Example data, can be extended
    auto pos = entityInterface.getComponent<Position>();
    voxelGrid->setVoxel(static_cast<int>(pos.x), static_cast<int>(pos.y),
                        static_cast<int>(pos.z), gridData);
  }

  return entity; // Return the created entity
}

// Predicate is narrow on purpose: only `inventory` and `tile_effects_list`
// are checked. A TERRAIN-typed object carrying other entity-only components
// (perception, behavior, ...) classifies as OnGridStorage and those attrs
// are silently dropped — widen if a regression surfaces.
EntityStorageKind World::classifyPyEntity(nb::object pyEntity) const {
  const bool isTerrain =
      nb::hasattr(pyEntity, "grid_type") &&
      !pyEntity.attr("grid_type").is_none() &&
      nb::cast<GridType>(pyEntity.attr("grid_type")) == GridType::TERRAIN;
  if (!isTerrain) {
    return EntityStorageKind::EntityOnly;
  }
  const bool hasInventory = nb::hasattr(pyEntity, "inventory") &&
                            !pyEntity.attr("inventory").is_none();
  const bool hasTileEffects = nb::hasattr(pyEntity, "tile_effects_list") &&
                              !pyEntity.attr("tile_effects_list").is_none();
  return (hasInventory || hasTileEffects)
             ? EntityStorageKind::EntityBackedTerrain
             : EntityStorageKind::OnGridStorage;
}

// Emplaces every Python-supplied component on the given entity. Shared by
// the hybrid-terrain and non-terrain-entity paths.
void World::emplaceAllPyComponents(entt::entity newEntity,
                                   nb::object pyEntity) {
  if (nb::hasattr(pyEntity, "entity_type") &&
      !pyEntity.attr("entity_type").is_none()) {
    registry.emplace<EntityTypeComponent>(
        newEntity, nb::cast<EntityTypeComponent>(pyEntity.attr("entity_type")));
  }
  if (nb::hasattr(pyEntity, "physics_stats") &&
      !pyEntity.attr("physics_stats").is_none()) {
    registry.emplace<PhysicsStats>(
        newEntity, nb::cast<PhysicsStats>(pyEntity.attr("physics_stats")));
  }
  if (nb::hasattr(pyEntity, "position") &&
      !pyEntity.attr("position").is_none()) {
    registry.emplace<Position>(newEntity,
                               nb::cast<Position>(pyEntity.attr("position")));
  }
  if (nb::hasattr(pyEntity, "velocity") &&
      !pyEntity.attr("velocity").is_none()) {
    registry.emplace<Velocity>(newEntity,
                               nb::cast<Velocity>(pyEntity.attr("velocity")));
  }
  if (nb::hasattr(pyEntity, "structural_integrity") &&
      !pyEntity.attr("structural_integrity").is_none()) {
    registry.emplace<StructuralIntegrityComponent>(
        newEntity, nb::cast<StructuralIntegrityComponent>(
                       pyEntity.attr("structural_integrity")));
  }
  if (nb::hasattr(pyEntity, "health") && !pyEntity.attr("health").is_none()) {
    registry.emplace<HealthComponent>(
        newEntity, nb::cast<HealthComponent>(pyEntity.attr("health")));
  }
  if (nb::hasattr(pyEntity, "perception") &&
      !pyEntity.attr("perception").is_none()) {
    registry.emplace<PerceptionComponent>(
        newEntity, nb::cast<PerceptionComponent>(pyEntity.attr("perception")));
  }
  if (nb::hasattr(pyEntity, "inventory") &&
      !pyEntity.attr("inventory").is_none()) {
    registry.emplace<Inventory>(
        newEntity, nb::cast<Inventory>(pyEntity.attr("inventory")));
  }
  if (nb::hasattr(pyEntity, "tile_effects_list") &&
      !pyEntity.attr("tile_effects_list").is_none()) {
    registry.emplace<TileEffectsList>(
        newEntity,
        nb::cast<TileEffectsList>(pyEntity.attr("tile_effects_list")));
  }
  if (nb::hasattr(pyEntity, "console_logs") &&
      !pyEntity.attr("console_logs").is_none()) {
    registry.emplace<ConsoleLogsComponent>(
        newEntity,
        nb::cast<ConsoleLogsComponent>(pyEntity.attr("console_logs")));
  }
  if (nb::hasattr(pyEntity, "fruit_growth") &&
      !pyEntity.attr("fruit_growth").is_none()) {
    registry.emplace<FruitGrowth>(
        newEntity, nb::cast<FruitGrowth>(pyEntity.attr("fruit_growth")));
  }
  if (nb::hasattr(pyEntity, "matter_container") &&
      !pyEntity.attr("matter_container").is_none()) {
    registry.emplace<MatterContainer>(
        newEntity,
        nb::cast<MatterContainer>(pyEntity.attr("matter_container")));
  }
  // `behavior` is read but never emplaced — preserved as a no-op for
  // back-compat with the prior body.
  if (nb::hasattr(pyEntity, "behavior") &&
      !pyEntity.attr("behavior").is_none()) {
    (void)pyEntity.attr("behavior");
  }
  if (nb::hasattr(pyEntity, "on_take_item_behavior") &&
      !pyEntity.attr("on_take_item_behavior").is_none()) {
    registry.emplace<OnTakeItemBehavior>(
        newEntity, pyEntity.attr("on_take_item_behavior"));
  }
  if (nb::hasattr(pyEntity, "on_use_item_behavior") &&
      !pyEntity.attr("on_use_item_behavior").is_none()) {
    registry.emplace<OnUseItemBehavior>(newEntity,
                                        pyEntity.attr("on_use_item_behavior"));
  }
  if (nb::hasattr(pyEntity, "digestion_comp") &&
      !pyEntity.attr("digestion_comp").is_none()) {
    registry.emplace<DigestionComponent>(
        newEntity,
        nb::cast<DigestionComponent>(pyEntity.attr("digestion_comp")));
  }
  if (nb::hasattr(pyEntity, "metabolism_comp") &&
      !pyEntity.attr("metabolism_comp").is_none()) {
    registry.emplace<MetabolismComponent>(
        newEntity,
        nb::cast<MetabolismComponent>(pyEntity.attr("metabolism_comp")));
  }
  if (nb::hasattr(pyEntity, "drop_rates") &&
      !pyEntity.attr("drop_rates").is_none()) {
    registry.emplace<DropRates>(
        newEntity, nb::cast<DropRates>(pyEntity.attr("drop_rates")));
  }

  // Auto-emplace WaterStressComponent on plants so the drought stress
  // accumulator (incremented on dry-tile ticks in processPlants,
  // decremented on supplied ticks) has a place to land. Plants without
  // HealthComponent never take drought damage — health is required for
  // the death cascade — but we emplace unconditionally on
  // entity_type=PLANT to keep the loop in processPlants simple
  // (single view filter on <WaterStressComponent, HealthComponent, Position>).
  if (nb::hasattr(pyEntity, "entity_type") &&
      !pyEntity.attr("entity_type").is_none()) {
    const auto et = nb::cast<EntityTypeComponent>(pyEntity.attr("entity_type"));
    if (et.mainType == static_cast<int>(EntityEnum::PLANT)) {
      registry.emplace<WaterStressComponent>(newEntity);
    }
  }
}

// Path 1 — plain terrain: VDB-only, terrain grid stores ON_GRID_STORAGE.
// Mirrors `createVaporTerrainEntity` in PhysicsMutators.hpp.
void World::createPlainTerrainFromPython(nb::object pyEntity) {
  Position pos = nb::cast<Position>(pyEntity.attr("position"));

  // Default-construct any missing terrain-state component so VDB grids hold
  // an explicit value (no stale leftovers).
  EntityTypeComponent type{};
  if (nb::hasattr(pyEntity, "entity_type") &&
      !pyEntity.attr("entity_type").is_none()) {
    type = nb::cast<EntityTypeComponent>(pyEntity.attr("entity_type"));
  }
  MatterContainer mc{};
  if (nb::hasattr(pyEntity, "matter_container") &&
      !pyEntity.attr("matter_container").is_none()) {
    mc = nb::cast<MatterContainer>(pyEntity.attr("matter_container"));
  }
  StructuralIntegrityComponent sic{};
  if (nb::hasattr(pyEntity, "structural_integrity") &&
      !pyEntity.attr("structural_integrity").is_none()) {
    sic = nb::cast<StructuralIntegrityComponent>(
        pyEntity.attr("structural_integrity"));
  }
  PhysicsStats ps{};
  if (nb::hasattr(pyEntity, "physics_stats") &&
      !pyEntity.attr("physics_stats").is_none()) {
    ps = nb::cast<PhysicsStats>(pyEntity.attr("physics_stats"));
  }

  auto *repo = voxelGrid->terrainGridRepository.get();
  repo->setPosition(pos.x, pos.y, pos.z, pos);
  repo->setTerrainEntityType(pos.x, pos.y, pos.z, type);
  repo->setTerrainMatterContainer(pos.x, pos.y, pos.z, mc);
  repo->setTerrainStructuralIntegrity(pos.x, pos.y, pos.z, sic);
  repo->setPhysicsStats(pos.x, pos.y, pos.z, ps);
  repo->setTerrainId(pos.x, pos.y, pos.z,
                     static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE));
}

// Path 2 — hybrid terrain (Inventory or TileEffectsList): entity for the
// entity-only data + dual-write of terrain-state to VDB so coord-keyed
// physics reads see consistent data. Terrain grid stores the entity int.
//
// Post-Task G: VDB is the source of truth for terrain-state; the registry
// copies are kept for back-compat readers but mutators should not maintain
// them going forward.
entt::entity World::createHybridTerrainFromPython(nb::object pyEntity) {
  entt::entity newEntity = registry.create();
  emplaceAllPyComponents(newEntity, pyEntity);

  // Dual-write terrain-state to VDB — same setter set as the plain-terrain
  // path; only the terrain id below differs.
  Position pos = nb::cast<Position>(pyEntity.attr("position"));
  auto *repo = voxelGrid->terrainGridRepository.get();
  repo->setPosition(pos.x, pos.y, pos.z, pos);
  if (nb::hasattr(pyEntity, "entity_type") &&
      !pyEntity.attr("entity_type").is_none()) {
    repo->setTerrainEntityType(
        pos.x, pos.y, pos.z,
        nb::cast<EntityTypeComponent>(pyEntity.attr("entity_type")));
  }
  if (nb::hasattr(pyEntity, "matter_container") &&
      !pyEntity.attr("matter_container").is_none()) {
    repo->setTerrainMatterContainer(
        pos.x, pos.y, pos.z,
        nb::cast<MatterContainer>(pyEntity.attr("matter_container")));
  }
  if (nb::hasattr(pyEntity, "structural_integrity") &&
      !pyEntity.attr("structural_integrity").is_none()) {
    repo->setTerrainStructuralIntegrity(
        pos.x, pos.y, pos.z,
        nb::cast<StructuralIntegrityComponent>(
            pyEntity.attr("structural_integrity")));
  }
  if (nb::hasattr(pyEntity, "physics_stats") &&
      !pyEntity.attr("physics_stats").is_none()) {
    repo->setPhysicsStats(
        pos.x, pos.y, pos.z,
        nb::cast<PhysicsStats>(pyEntity.attr("physics_stats")));
  }

  // Write the terrain id directly via the repository, bypassing
  // `voxelGrid->setTerrain()` → `setTerrainFromEntt`. That legacy path
  // *destroys the entity* and writes ON_GRID_STORAGE when the entity has
  // no Velocity / MovingComponent — Inventory and TileEffectsList don't
  // count, so a hybrid chest would be silently demoted to plain terrain.
  voxelGrid->terrainGridRepository->setTerrainId(pos.x, pos.y, pos.z,
                                                 static_cast<int>(newEntity));

  return newEntity;
}

// Path 3 — non-terrain entity (legacy): create + emplace all components +
// setEntity in the entity grid.
entt::entity World::createNonTerrainEntityFromPython(nb::object pyEntity) {
  entt::entity newEntity = registry.create();
  emplaceAllPyComponents(newEntity, pyEntity);

  if (nb::hasattr(pyEntity, "grid_type") &&
      !pyEntity.attr("grid_type").is_none() &&
      nb::hasattr(pyEntity, "position") &&
      !pyEntity.attr("position").is_none()) {
    GridType gridType = nb::cast<GridType>(pyEntity.attr("grid_type"));
    if (gridType == GridType::ENTITY) {
      Position pos = nb::cast<Position>(pyEntity.attr("position"));
      voxelGrid->setEntity(pos.x, pos.y, pos.z, static_cast<int>(newEntity));
    }
  }
  return newEntity;
}

// Three-way dispatcher keyed on `EntityStorageKind`.
entt::entity World::createEntityFromPython(nb::object pyEntity) {
  switch (classifyPyEntity(pyEntity)) {
  case EntityStorageKind::OnGridStorage:
    createPlainTerrainFromPython(pyEntity);
    return entt::null;

  case EntityStorageKind::EntityBackedTerrain:
    return createHybridTerrainFromPython(pyEntity);

  case EntityStorageKind::EntityOnly:
    return createNonTerrainEntityFromPython(pyEntity);
  }
  return entt::null; // unreachable; satisfies compilers without
                     // -Wcovered-switch
}

// Get entities based on their type
nb::dict World::getEntitiesByType(int entityMainType, int entitySubType0) {
  // Acquire shared lock to prevent entity destruction during entity queries
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  nb::dict entitiesMetadata;

  // Iterate through all entities that have the EntityTypeComponent
  auto view = registry.view<EntityTypeComponent>();
  for (auto entity : view) {
    // Retrieve the entity type component
    EntityTypeComponent *entityTypeComp =
        registry.try_get<EntityTypeComponent>(entity);

    // If the entity's type matches the input type
    if (entityTypeComp && entityTypeComp->mainType == entityMainType &&
        entityTypeComp->subType0 == entitySubType0) {
      // Retrieve additional entity components, such as position or other
      // metadata
      EntityInterface entityInterface = createEntityInterface(registry, entity);

      // Store the entity interface in the dictionary with the entity ID as the
      // key
      entitiesMetadata[nb::int_(static_cast<int>(entity))] =
          nb::cast(entityInterface);
    }
  }

  return entitiesMetadata;
}

// Get entity IDs based on their type
nb::list World::getEntityIdsByType(int entityMainType, int entitySubType0) {
  // Acquire shared lock to prevent entity destruction during entity ID queries
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  nb::list entityIds;

  // Iterate through all entities that have the EntityTypeComponent
  auto view = registry.view<EntityTypeComponent, PerceptionComponent>();
  for (auto entity : view) {
    // Retrieve the entity type component
    EntityTypeComponent *entityTypeComp =
        registry.try_get<EntityTypeComponent>(entity);
    PerceptionComponent *perceptionComp =
        registry.try_get<PerceptionComponent>(entity);

    // If the entity's type matches the input type
    if (entityTypeComp && perceptionComp &&
        entityTypeComp->mainType == entityMainType &&
        entityTypeComp->subType0 == entitySubType0) {
      // Add the entity ID to the list
      entityIds.append(nb::int_(static_cast<int>(entity)));
    }
  }

  return entityIds;
}

nb::dict World::createPerceptionResponses(nb::dict entitiesWithQueries) {
  const size_t BATCH_NUMBER = 16;
  nb::dict perceptionResponses;

  // Acquire shared lock to prevent entity destruction during perception
  // creation
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  // We'll collect a list of (entityId, vectorOfCommands) that we can process in
  // threads
  struct Job {
    int entityId;
    std::vector<QueryCommand> commands;
  };
  std::vector<Job> jobs;
  jobs.reserve(entitiesWithQueries.size());

  {
    // Acquire GIL only while reading Python dict
    nb::gil_scoped_acquire gil;

    for (auto item : entitiesWithQueries) {
      int entityId = nb::cast<int>(item.first);
      nb::list optionalQueries = nb::cast<nb::list>(item.second);

      // Convert to native data
      std::vector<QueryCommand> commands = toCommandList(optionalQueries);

      // Optionally clear the Python list if needed
      optionalQueries.attr("clear")();

      // Store the job in a vector
      jobs.push_back({entityId, std::move(commands)});
    }
  } // GIL is released automatically here when gil_scoped_acquire goes out of
    // scope

  // Per-batch output slots. Pre-allocated so each task_group lambda
  // writes to a disjoint index — no mutex, no future<vector>. After
  // `asyncTasks_.wait()` returns, every slot is populated and we
  // assemble the final `nb::dict` on the main thread under the GIL.
  using BatchResult = std::vector<std::pair<int, std::vector<char>>>;
  std::vector<BatchResult> batchResults(BATCH_NUMBER);

  // Compute the actual number of batches based on jobs.size() — empty
  // tail batches stay default-constructed and are cheap to skip below.
  const size_t numBatches = BATCH_NUMBER;
  const size_t batchSize =
      jobs.empty() ? 0 : (jobs.size() + numBatches - 1) / numBatches;

  {
    // Release GIL so the TBB worker threads can run without blocking on
    // the GIL. The lambdas below are pure C++ — they don't touch
    // Python — so they don't need to re-acquire it. Plan:
    // .claude/docs/epics-plans/2026-05-09-tbb-task-group-migration.md#17
    nb::gil_scoped_release gil;

    // A function-local `task_group` instead of reusing the World-level
    // `asyncTasks_` (shared by physics + ecosystem) — perception runs
    // on a different code path than `World::update()`, and if both run
    // concurrently on different threads a wait on `asyncTasks_` here
    // could block on physics/ecosystem tasks too (TBB's wait drains
    // every task submitted to that instance). A local group sees only
    // its own batches. Construction is sub-microsecond and the
    // underlying TBB worker pool is still the process-wide persistent
    // one — no per-call OS thread creation.
    tbb::task_group perceptionTasks;

    for (size_t batchIndex = 0; batchIndex < numBatches; ++batchIndex) {
      const size_t start = batchIndex * batchSize;
      if (start >= jobs.size()) {
        break; // tail batches stay empty
      }
      const size_t end = std::min(start + batchSize, jobs.size());

      perceptionTasks.run(
          [this, start, end, batchIndex, &jobs, &batchResults]() {
            BatchResult &out = batchResults[batchIndex];
            out.reserve(end - start);

            for (size_t i = start; i < end; ++i) {
              auto &job = jobs[i];
              std::vector<char> serializedResponse;

              try {
                serializedResponse =
                    createPerceptionResponseC(job.entityId, job.commands);
              } catch (const std::exception &e) {
                Logger::getLogger()->error(
                    "Failed to create perception response for entity " +
                    std::to_string(job.entityId) + ": " + e.what());
                // leave serializedResponse empty for the caller to detect
              }

              out.emplace_back(job.entityId, std::move(serializedResponse));
            }
          });
    }

    // Block (without the GIL) until every perception batch finishes.
    // With TBB's persistent worker pool this is sub-millisecond when
    // the workers are warm, vs the ~80-150 µs × 16 = ~1.3-2.4 ms
    // `std::thread` creation+join overhead of the previous
    // `std::async(std::launch::async)` implementation.
    perceptionTasks.wait();
  }

  // Re-acquire GIL to populate `perceptionResponses` from the
  // pre-allocated batch outputs.
  {
    nb::gil_scoped_acquire gil;

    for (auto &batch : batchResults) {
      for (auto &[entityId, serializedResponse] : batch) {
        nb::bytes resp(serializedResponse.data(), serializedResponse.size());
        perceptionResponses[nb::int_(entityId)] = resp;
      }
    }
  }

  return perceptionResponses;
}

EntityInterface World::getEntityById(int entityId) {
  // Convert the entity ID to the entt::entity type
  entt::entity entity = static_cast<entt::entity>(entityId);

  // Acquire shared lock to prevent entity destruction during perception
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  // CRITICAL: Always check entity validity first
  if (!registry.valid(entity)) {
    // Logger::getLogger()->error("[getEntityById] Entity " +
    // std::to_string(entityId) + " is invalid");
    throw std::runtime_error("Entity " + std::to_string(entityId) +
                             " is no longer valid");
  }

  // Logger::getLogger()->debug("[getEntityById] Entity " +
  // std::to_string(entityId) + " is valid, checking components");

  // Check if entity has required components BEFORE accessing them
  if (!registry.all_of<Position>(entity)) {
    // Logger::getLogger()->error("[getEntityById] Entity " +
    // std::to_string(entityId) + " missing Position component");
    throw std::runtime_error("Entity " + std::to_string(entityId) +
                             " does not have Position component");
  }

  // TODO: Make this a more robust check.
  Position position = registry.get<Position>(entity);
  int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
  if (entityIdVoxel != entityId) {
    std::cout << "Warning: Entity " << entityId
              << " is not at its recorded voxel position (" << position.x << ","
              << position.y << "," << position.z
              << "). Actual voxel entity: " << entityIdVoxel << std::endl;
    throw std::runtime_error("Entity Position mismatch with VoxelGrid");
  }

  EntityInterface entityInterface = createEntityInterface(registry, entity);

  return entityInterface;
}

// Helper function to get bounds for perception
int World::getPerceptionBounds(int pos, int perception) const {
  return pos - perception;
}

struct SerializableEntity {
  int entityId;
  float posX, posY, posZ;
  float velX, velY, velZ;
  int health;
  int entityType;
};

int World::getTerrain(int x, int y, int z) {
  return voxelGrid->getTerrain(x, y, z);
}

int World::getEntity(int x, int y, int z) {
  return voxelGrid->getEntity(x, y, z);
}

void World::dispatchMoveSolidEntityEventById(
    int entityId, std::vector<DirectionEnum> directionsToApply) {
  // Acquire shared lock to prevent entity destruction during movement dispatch
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  entt::entity entity = static_cast<entt::entity>(entityId);

  // TODO: Make this a more robust check.
  Position position = registry.get<Position>(entity);
  int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
  if (entityIdVoxel != entityId) {
    std::string errorMessage =
        "Entity id on EntityInterface: " + std::to_string(entityId) +
        " Position on EntityInterface: (" + std::to_string(position.x) + "," +
        std::to_string(position.y) + "," + std::to_string(position.z) + ")" +
        "Entity id on VoxelGrid: " + std::to_string(entityIdVoxel);
    throw std::runtime_error(errorMessage);
  }

  // Safely get PhysicsStats
  if (auto *physicsStats = registry.try_get<PhysicsStats>(entity)) {
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float deltaZ = 0.0f;

    for (const auto &direction : directionsToApply) {
      switch (direction) {
      case DirectionEnum::LEFT:
        deltaX -= physicsStats->forceX;
        break;
      case DirectionEnum::RIGHT:
        deltaX += physicsStats->forceX;
        break;
      case DirectionEnum::UP:
        deltaY -= physicsStats->forceY;
        break;
      case DirectionEnum::DOWN:
        deltaY += physicsStats->forceY;
        break;
      case DirectionEnum::UPWARD:
        deltaZ += physicsStats->forceZ;
        break;
      case DirectionEnum::DOWNWARD:
        deltaZ -= physicsStats->forceZ;
        break;
      default:
        std::cerr << "Unknown direction: " << static_cast<int>(direction)
                  << "\n";
        break;
      }
    }

    dispatcher.enqueue<MoveSolidEntityEvent>(entity, deltaX, deltaY, deltaZ);
  } else {
    std::cout << "Entity does not have PhysicsStats component.\n";
  }
}

void World::dispatchMoveSolidEntityEventByPosition(int x, int y, int z,
                                                   GridType gridType,
                                                   float deltaX, float deltaY,
                                                   float deltaZ) {
  if (gridType == GridType::ENTITY) {
    // Get the entity at the given position in the voxel grid
    int entityID = voxelGrid->getEntity(x, y, z);

    if (entityID != -1) {
      entt::entity entity = static_cast<entt::entity>(entityID);

      dispatcher.enqueue<MoveSolidEntityEvent>(entity, deltaX, deltaY, deltaZ);
    } else {
      std::cout << "No entity found at the given coordinates.\n";
    }
  } else if (gridType == GridType::TERRAIN) {
    // If it's terrain, we might not be moving it, but you could handle other
    // events
    std::cout << "Terrain movement not supported.\n";
  } else {
    std::cout << "Event not dispatched.\n";
  }
}

void World::dispatchWaterFallEvent(Position sourcePos, Position destPos,
                                   int fallingAmount, int entity) {
  entt::entity terrainEntity = static_cast<entt::entity>(entity);
  dispatcher.enqueue<WaterFallEntityEvent>(terrainEntity, sourcePos, destPos,
                                           fallingAmount);
}

void World::dispatchWaterGravityFlowEvent(Position sourcePos,
                                          Position targetPos, int amount,
                                          int targetTerrainId) {
  // Snapshot type and matter at source/target so the event handler sees the
  // same state we observe here. Skip the target reads when target is NONE
  // (no terrain to query).
  EntityTypeComponent sourceType =
      voxelGrid->terrainGridRepository->getTerrainEntityType(
          sourcePos.x, sourcePos.y, sourcePos.z);
  MatterContainer sourceMatter =
      voxelGrid->terrainGridRepository->getTerrainMatterContainer(
          sourcePos.x, sourcePos.y, sourcePos.z);

  EntityTypeComponent targetType = {};
  MatterContainer targetMatter = {};
  if (targetTerrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
    targetType = voxelGrid->terrainGridRepository->getTerrainEntityType(
        targetPos.x, targetPos.y, targetPos.z);
    targetMatter = voxelGrid->terrainGridRepository->getTerrainMatterContainer(
        targetPos.x, targetPos.y, targetPos.z);
  }

  dispatcher.enqueue<WaterGravityFlowEvent>(
      sourcePos, targetPos, amount, targetTerrainId, sourceType, targetType,
      sourceMatter, targetMatter);
}

void World::dispatchCondenseWaterEvent(Position vaporPos,
                                       int condensationAmount,
                                       int terrainBelowId) {
  dispatcher.enqueue<CondenseWaterEntityEvent>(vaporPos, condensationAmount,
                                               terrainBelowId);
}

void World::dispatchMoveGasEntityEvent(Position position, int entity,
                                       float forceX, float forceY, float rhoEnv,
                                       float rhoGas) {
  MoveGasEntityEvent event{static_cast<entt::entity>(entity),
                           position,
                           forceX,
                           forceY,
                           rhoEnv,
                           rhoGas};
  event.setForceApplyNewVelocity();
  dispatcher.enqueue<MoveGasEntityEvent>(event);
}

void World::dispatchVaporCreationEvent(Position position, int amount) {
  // `targetExists = false` matches what `dispatchVaporCreationOrAddition`
  // emits when the destination cell is NONE — drives the create-new-vapor
  // path through `createVaporTerrainEntity`.
  VaporCreationEvent event(position, amount, false);
  dispatcher.enqueue<VaporCreationEvent>(event);
}

void World::dispatchWaterCreationEvent(Position position, int amount) {
  dispatcher.enqueue<WaterCreationEvent>(WaterCreationEvent{position, amount});
}

void World::deleteTerrainAt(int x, int y, int z) {
  voxelGrid->deleteTerrain(eventSink_, x, y, z, true);
}

void World::dispatchTakeItemEventById(int entityId, int hoveredEntityId,
                                      int selectedEntityId) {
  // std::cout << "[TakeItemEvent] entityId=" << entityId
  //           << " hoveredEntityId=" << hoveredEntityId
  //           << " selectedEntityId=" << selectedEntityId << "\n";

  // Acquire shared lock to prevent entity destruction during item take dispatch
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  entt::entity entity = static_cast<entt::entity>(entityId);

  // TODO: Make this a more robust check.
  Position position = registry.get<Position>(entity);
  // std::cout << "[TakeItemEvent] position=(" << position.x << "," <<
  // position.y << "," << position.z << ")"
  //           << " direction=" << static_cast<int>(position.direction) << "\n";
  int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
  if (entityIdVoxel != entityId) {
    std::string errorMessage =
        "Entity id on EntityInterface: " + std::to_string(entityId) +
        " Position on EntityInterface: (" + std::to_string(position.x) + "," +
        std::to_string(position.y) + "," + std::to_string(position.z) + ")" +
        "Entity id on VoxelGrid: " + std::to_string(entityIdVoxel);
    throw std::runtime_error(errorMessage);
  }

  // Safely get PhysicsStats
  if (auto *inventory = registry.try_get<Inventory>(entity)) {
    nb::object pyRegistryObj = nb::cast(&pyRegistry);
    dispatcher.enqueue<TakeItemEvent>(entity, pyRegistryObj, voxelGrid,
                                      hoveredEntityId, selectedEntityId);
  } else {
    std::cout << "Entity does not have Inventory component.\n";
  }
}

void World::dispatchUseItemEventById(int entityId, int itemSlot,
                                     int hoveredEntityId,
                                     int selectedEntityId) {
  // Acquire shared lock to prevent entity destruction during item use dispatch
  std::shared_lock lifecycleLock(entityLifecycleMutex);

  entt::entity entity = static_cast<entt::entity>(entityId);

  // TODO: Make this a more robust check.
  Position position = registry.get<Position>(entity);
  int entityIdVoxel = voxelGrid->getEntity(position.x, position.y, position.z);
  if (entityIdVoxel != entityId) {
    std::cout << "Warning: Entity " << entityId
              << " is not at its recorded voxel position (" << position.x << ","
              << position.y << "," << position.z
              << "). Actual voxel entity: " << entityIdVoxel << std::endl;
    throw std::runtime_error("Entity Position mismatch with VoxelGrid");
  }

  // Safely get PhysicsStats
  if (auto *inventory = registry.try_get<Inventory>(entity)) {
    nb::object pyRegistryObj = nb::cast(&pyRegistry);
    dispatcher.enqueue<UseItemEvent>(entity, pyRegistryObj, voxelGrid, itemSlot,
                                     hoveredEntityId, selectedEntityId);
  } else {
    std::cout << "Entity does not have Inventory component.\n";
  }
}

void World::dispatchSetEntityToDebug(int entityId) {
  entt::entity entity = static_cast<entt::entity>(entityId);
  dispatcher.enqueue<SetEcoEntityToDebug>(entity);
  dispatcher.enqueue<SetPhysicsEntityToDebug>(entity);
}

void World::releasePythonState() {
  pythonEventCallbacks.clear();
  pythonSystems.clear();
  pythonScripts.clear();
}

void World::addPythonSystem(nb::object system) {
  // Optional: Validate that the system has an 'update' method
  if (!nb::hasattr(system, "update")) {
    throw std::runtime_error("Python system must have an 'update' method.");
  }

  // Optionally, perform additional validations or initializations

  pythonSystems.emplace_back(system);
}

nb::object World::getPythonSystem(size_t index) const {
  if (index >= pythonSystems.size()) {
    throw std::out_of_range("Python system index out of range.");
  }
  return pythonSystems[index];
}

void World::addPythonScript(std::string &key, nb::object script) {
  // Optional: Validate that the script has an 'update' method
  if (!nb::hasattr(script, "run")) {
    throw std::runtime_error("Python script must have an 'update' method.");
  }

  // Optionally, perform additional validations or initializations

  pythonScripts[key] = script;
}

void World::runPythonScript(std::string &key) {
  if (pythonScripts.find(key) != pythonScripts.end()) {
    nb::gil_scoped_acquire acquire;
    nb::object pyRegistryObj = nb::cast(&pyRegistry);

    nb::object script = pythonScripts[key];

    try {
      script.attr("run")(pyRegistryObj, voxelGrid);
    } catch (const nb::cast_error &e) {
      std::cerr << "Error in Python script run: " << e.what() << std::endl;
    }
  } else {
    throw std::runtime_error("Python script key not found.");
  }
}

void World::registerPythonEventHandler(const std::string &eventType,
                                       nb::object callback) {
  // Validate that the callback is callable
  if (!nb::hasattr(callback, "__call__")) {
    throw std::runtime_error("Python callback must be callable");
  }

  // Store the callback
  pythonEventCallbacks[eventType].push_back(callback);

  // Register C++ handlers if this is the first callback for this event type
  if (pythonEventCallbacks[eventType].size() == 1) {
    // if (eventType == "MoveSolidEntityEvent") {
    //     dispatcher.sink<MoveSolidEntityEvent>().connect<&World::onMoveSolidEntityEventPython>(*this);
    if (eventType == "TakeItemEvent") {
      dispatcher.sink<TakeItemEvent>().connect<&World::onTakeItemEventPython>(
          *this);
    } else if (eventType == "UseItemEvent") {
      dispatcher.sink<UseItemEvent>().connect<&World::onUseItemEventPython>(
          *this);
    }
    // Add more event types as needed
  }
}

void World::onTakeItemEventPython(const TakeItemEvent &event) {
  nb::gil_scoped_acquire acquire;

  auto it = pythonEventCallbacks.find("TakeItemEvent");
  if (it != pythonEventCallbacks.end()) {
    nb::dict eventData;
    eventData["entity_id"] = nb::int_(static_cast<int>(event.entity));
    eventData["hovered_entity_id"] = nb::int_(event.hoveredEntityId);
    eventData["selected_entity_id"] = nb::int_(event.selectedEntityId);
    eventData["event_type"] = nb::str("TakeItemEvent");

    for (const auto &callback : it->second) {
      try {
        callback(eventData, nb::cast(&pyRegistry));
      } catch (const nb::cast_error &e) {
        Logger::getLogger()->error("Error in Python TakeItemEvent callback: " +
                                   std::string(e.what()));
      }
    }
  }
}

void World::onUseItemEventPython(const UseItemEvent &event) {
  nb::gil_scoped_acquire acquire;

  auto it = pythonEventCallbacks.find("UseItemEvent");
  if (it != pythonEventCallbacks.end()) {
    nb::dict eventData;
    eventData["entity_id"] = nb::int_(static_cast<int>(event.entity));
    eventData["item_slot"] = nb::int_(event.itemSlot);
    eventData["hovered_entity_id"] = nb::int_(event.hoveredEntityId);
    eventData["selected_entity_id"] = nb::int_(event.selectedEntityId);
    eventData["event_type"] = nb::str("UseItemEvent");

    for (const auto &callback : it->second) {
      try {
        callback(eventData, nb::cast(&pyRegistry));
      } catch (const nb::cast_error &e) {
        Logger::getLogger()->error("Error in Python UseItemEvent callback: " +
                                   std::string(e.what()));
      }
    }
  }
}

void safeExecute(const std::function<void()> &func,
                 const std::string &taskName) {
  try {
    func(); // Execute the task
  } catch (const std::exception &e) {
    Logger::getLogger()->error(taskName + " async task crashed: " + e.what());
    // Optionally, implement additional error handling here (e.g., logging,
    // retry mechanisms)
  } catch (...) {
    Logger::getLogger()->error(taskName +
                               " async task crashed with an unknown error.");
    // Handle non-standard exceptions if necessary
  }
}

// Drain any exception the previous ecosystem task captured into
// ecosystemState_.lastException, rethrowing as the same
// EcosystemEngineException the old `ecosystemFuture.get()` path used to.
// Returns silently if no exception is pending.
static void drainEcosystemException(World::AsyncEngineState &state) {
  std::exception_ptr eptr;
  {
    std::lock_guard<std::mutex> lk(state.exceptionMutex);
    eptr = std::move(state.lastException);
    state.lastException = nullptr;
  }
  if (!eptr) {
    return;
  }
  try {
    std::rethrow_exception(eptr);
  } catch (const std::exception &e) {
    std::cerr << "EcosystemEngine async task crashed: " << e.what()
              << std::endl;
    throw aetherion::EcosystemEngineException(
        "[EcosystemEngineException] EcosystemEngine async task crashed: " +
        std::string(e.what()));
  }
}

// Run one ecosystem step, choosing inline or async dispatch.
//
// When PhysicsManager::getRunEcosystemSynchronously() is true, run the full
// ecosystem step inline on the main update thread instead of dispatching it
// via the TBB task group. This is the only way to truly serialize the
// dispatcher and VDB accesses for diagnostic comparisons — the
// populateSchedulerWithSubset bypass alone is not enough because the
// async ecosystem task otherwise runs on a TBB worker.
//
// In sync mode the in-flight task (if any, from a prior async tick) is
// drained first so the inline call cannot race with it. asyncTasks_.wait()
// waits on physics too — that's fine in sync mode where serialised
// execution is the explicit intent.
void World::runEcosystemStep() {
  if (!processEcosystem_) {
    return;
  }

  const bool runEcosystemSynchronously =
      PhysicsManager::Instance()->getRunEcosystemSynchronously();

  if (runEcosystemSynchronously) {
    if (ecosystemState_.running.load(std::memory_order_acquire)) {
      asyncTasks_.wait();
    }
    drainEcosystemException(ecosystemState_);

    if (ecosystemEngine && ecosystemEngine->waterSimManager_ &&
        !ecosystemEngine->waterSimManager_->hasEncounteredCriticalError()) {
      ecosystemEngine->processEcosystemAsync(registry, *voxelGrid, eventSink_,
                                             gameClock);
      ecosystemStarted_ = true;
    }
    return;
  }

  // Async path: bail if the previous task is still in flight. Matches the
  // pre-migration `wait_for(0) != ready` check.
  if (ecosystemState_.running.load(std::memory_order_acquire)) {
    return;
  }

  // Surface any exception the previous (now-completed) task captured.
  drainEcosystemException(ecosystemState_);

  // Only restart ecosystem task if no critical error has occurred.
  if (ecosystemEngine && ecosystemEngine->waterSimManager_ &&
      !ecosystemEngine->waterSimManager_->hasEncounteredCriticalError()) {
    // Atomic gate: exchange returns the *previous* value; if it was
    // already true we lost a race and must not enqueue a duplicate.
    if (ecosystemState_.running.exchange(true, std::memory_order_acq_rel)) {
      return;
    }
    asyncTasks_.run([this]() {
      safeExecute(
          [this]() {
            try {
              ecosystemEngine->processEcosystemAsync(registry, *voxelGrid,
                                                     eventSink_, gameClock);
            } catch (...) {
              std::lock_guard<std::mutex> lk(ecosystemState_.exceptionMutex);
              ecosystemState_.lastException = std::current_exception();
              throw; // safeExecute logs + swallows
            }
          },
          "EcosystemEngine");
      // Reset gate on every exit path (safeExecute swallows exceptions).
      ecosystemState_.running.store(false, std::memory_order_release);
    });
    ecosystemStarted_ = true;
  }
}

void World::update() {
#ifdef TRACY_ENABLE
  ZoneScopedN("World::update");
#endif
  // std::cout << "World update started!" << std::endl;

  gameClock.tick();
  // Placeholder for the world update logic (e.g., physics, AI, etc.)

  std::lock_guard<std::mutex> lock(registryMutex);

  healthSystem->processHealth(registry, *voxelGrid, dispatcher);

  // Walk the diag Registry and flush any counters whose `flush_every`
  // window has elapsed. Replaces the per-engine flush methods that used
  // to live in PhysicsEngine / LifeEngine. The Registry routes samples
  // to GameDB via the GameDBSink configured at registration time.
  aetherion::diag::Registry::instance().tick();

  // Legacy LifeEngine flush — to be migrated in v2 (Task 14).
  if (dbHandler) {
    lifeEngine->flushLifeMetrics(dbHandler.get());
  }

  // Replay events staged by worker threads (ecosystem future, water-sim
  // pool, physics future) before the main thread dispatches. After this
  // point the dispatcher is single-threaded for the duration of update().
  workerSink_.drain(dispatcher);

  dispatcher.update();

  physicsEngine->processPhysics(registry, *voxelGrid, eventSink_, gameClock);

  if (processMetabolism_) {
    metabolismSystem->processMetabolism(registry, *voxelGrid, dispatcher);
  }

  ecosystemEngine->processEcosystem(registry, *voxelGrid, eventSink_,
                                    gameClock);

  effectsSystem->processEffects(registry, *voxelGrid, dispatcher);

  // Acquire GIL before executing Python code
  nb::gil_scoped_acquire acquire;
  nb::object pyRegistryObj = nb::cast(&pyRegistry);

  // Update Python systems
  for (auto &system : pythonSystems) {
    try {
      system.attr("update")(pyRegistryObj, voxelGrid);
    } catch (const nb::cast_error &e) {
      std::cerr << "Error in Python system update: " << e.what() << std::endl;
    }
  }

  bool hasEntitiesToDelete = !lifeEngine->entitiesToDelete.empty();
  bool hasAnyCleanup = hasEntitiesToDelete;

  // Check if any async tasks are still running. Reads the same atomic
  // gates that the dispatch path flips. Metabolism is sync-only now so
  // it has no async state to check. relaxed is fine here — we only need
  // a most-recent-best-effort observation; the worst case is a single
  // extra tick before cleanup proceeds.
  const bool anyAsyncTasksRunning =
      physicsState_.running.load(std::memory_order_relaxed) ||
      (processEcosystem_ &&
       ecosystemState_.running.load(std::memory_order_relaxed));

  // Only perform cleanup if we have cleanup work AND no async tasks are running
  if (hasAnyCleanup && !anyAsyncTasksRunning) {
    // SAFE to do cleanup - no async tasks are accessing the data
    if (hasEntitiesToDelete) {
      processEntityDeletion();
    }
  }

  // Handle async task lifecycle - check for completed tasks and launch new ones
  // GUARDRAIL: Do not launch new async tasks if entities are waiting to be
  // deleted This ensures we get a clean window where no async tasks are running
  // so deletion can proceed
  if (!hasEntitiesToDelete) {
    // Handle Physics Async Task. Skip if the previous task is still
    // in flight; otherwise drain any captured exception (logged here,
    // matching the old future.get() try/catch behaviour) and submit
    // the next task to the persistent TBB worker pool.
    if (!physicsState_.running.load(std::memory_order_acquire)) {
      std::exception_ptr eptr;
      {
        std::lock_guard<std::mutex> lk(physicsState_.exceptionMutex);
        eptr = std::move(physicsState_.lastException);
        physicsState_.lastException = nullptr;
      }
      if (eptr) {
        try {
          std::rethrow_exception(eptr);
        } catch (const std::exception &e) {
          std::cerr << "PhysicsEngine async task crashed: " << e.what()
                    << std::endl;
        }
      }

      // Atomic gate prevents duplicate enqueue if the next tick races us.
      if (!physicsState_.running.exchange(true, std::memory_order_acq_rel)) {
        asyncTasks_.run([this]() {
          safeExecute(
              [this]() {
                try {
                  physicsEngine->processPhysicsAsync(registry, *voxelGrid,
                                                     eventSink_, gameClock);
                } catch (...) {
                  std::lock_guard<std::mutex> lk(physicsState_.exceptionMutex);
                  physicsState_.lastException = std::current_exception();
                  throw; // safeExecute logs + swallows
                }
              },
              "PhysicsEngine");
          physicsState_.running.store(false, std::memory_order_release);
        });
      }
    }

    runEcosystemStep();

    // Note: a metabolism async-dispatch block used to live here, gated
    // on a hardcoded `const bool processMetabolismAsync = false;` which
    // made it dead code. The metabolism path is sync-only now (serviced
    // by the call earlier in this method).
  }
}

void World::putTimeSeries(const std::string &seriesName, long long timestamp,
                          double value) {
  // Logger::getLogger()->debug("[World::putTimeSeries] Called");
  dbHandler->putTimeSeries(seriesName, timestamp, value);
}

std::vector<std::pair<uint64_t, double>>
World::queryTimeSeries(const std::string &seriesName, long long start,
                       long long end) {
  return dbHandler->queryTimeSeries(seriesName, start, end);
}

void World::executeSQL(const std::string &sql) { dbHandler->executeSQL(sql); }

size_t World::peekTimeSeriesSize(const std::string &seriesName) const {
  return dbHandler ? dbHandler->peekInMemorySize(seriesName) : 0;
}

long long
World::countTimeSeriesRowsOnDisk(const std::string &seriesName) const {
  return dbHandler ? dbHandler->countOnDiskRows(seriesName) : -1;
}

std::vector<ThreadError> World::getWaterSimErrors() const {
  if (ecosystemEngine && ecosystemEngine->waterSimManager_) {
    return ecosystemEngine->waterSimManager_->getErrors();
  }
  return {};
}

bool World::hasWaterSimErrors() const {
  if (ecosystemEngine && ecosystemEngine->waterSimManager_) {
    return ecosystemEngine->waterSimManager_->hasEncounteredCriticalError();
  }
  return false;
}

// Water simulation phase toggles
bool World::getSimulateVaporCondensation() const {
  return PhysicsManager::Instance()->getSimulateVaporCondensation();
}
void World::setSimulateVaporCondensation(bool value) {
  PhysicsManager::Instance()->setSimulateVaporCondensation(value);
}

bool World::getSimulateVaporMovement() const {
  return PhysicsManager::Instance()->getSimulateVaporMovement();
}
void World::setSimulateVaporMovement(bool value) {
  PhysicsManager::Instance()->setSimulateVaporMovement(value);
}

bool World::getSimulateWaterMovement() const {
  return PhysicsManager::Instance()->getSimulateWaterMovement();
}
void World::setSimulateWaterMovement(bool value) {
  PhysicsManager::Instance()->setSimulateWaterMovement(value);
}

bool World::getSimulateWaterEvaporation() const {
  return PhysicsManager::Instance()->getSimulateWaterEvaporation();
}
void World::setSimulateWaterEvaporation(bool value) {
  PhysicsManager::Instance()->setSimulateWaterEvaporation(value);
}
bool World::getWaterAutoBalancing() const {
  return PhysicsManager::Instance()->getWaterAutoBalancing();
}
void World::setWaterAutoBalancing(bool value) {
  PhysicsManager::Instance()->setWaterAutoBalancing(value);
}
bool World::getRunEcosystemSynchronously() const {
  return PhysicsManager::Instance()->getRunEcosystemSynchronously();
}
void World::setRunEcosystemSynchronously(bool value) {
  PhysicsManager::Instance()->setRunEcosystemSynchronously(value);
}