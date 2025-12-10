// EcosystemEngine.cpp

#include "EcosystemEngine.hpp"

#include <algorithm>  // For std::min
#include <chrono>
#include <iostream>
#include <random>
#include <ranges>
#include <thread>

/**********************
 *
 * GridBoxProcessor Implementation *
 *
 * ********************
 */

void GridBoxProcessor::initializeAccessors(
    entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher) {
    accessors_ = std::make_unique<ThreadAccessors>();
    registry_ = &registry;
    voxelGrid_ = &voxelGrid;
    dispatcher_ = &dispatcher;

    // Get TerrainStorage from VoxelGrid
    TerrainStorage* storage = voxelGrid.terrainStorage.get();
    if (!storage) {
        throw std::runtime_error("GridBoxProcessor: TerrainStorage not available in VoxelGrid");
    }

    // Create fresh Accessors for this thread - these are internally ValueAccessors
    // providing optimal performance for spatially coherent voxel traversal
    // Each thread gets its own accessor for optimal cache performance
    accessors_->waterAccessor = storage->waterMatterGrid->getAccessor();
    accessors_->vaporAccessor = storage->vaporMatterGrid->getAccessor();
    accessors_->mainTypeAccessor = storage->mainTypeGrid->getAccessor();
    accessors_->subType0Accessor = storage->subType0Grid->getAccessor();
    accessors_->flagsAccessor = storage->flagsGrid->getConstAccessor();
}

std::vector<WaterFlow> GridBoxProcessor::processBox(const GridBox& box, float sunIntensity) {
    std::vector<WaterFlow> pendingFlows;

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> disWaterSpreading(1, 4);

    // Cache-friendly iteration order (Z->Y->X)
    for (int z = box.minZ; z <= box.maxZ; ++z) {
        for (int y = box.minY; y <= box.maxY; ++y) {
            for (int x = box.minX; x <= box.maxX; ++x) {
                processTileWater(x, y, z, *registry_, *voxelGrid_, *dispatcher_, sunIntensity,
                                 rd, gen, disWaterSpreading);
                // processVoxelWater(x, y, z, pendingFlows);
                // processVoxelEvaporation(x, y, z, pendingFlows);
            }
        }
    }

    return pendingFlows;
}

void GridBoxProcessor::processVoxelWater(int x, int y, int z, std::vector<WaterFlow>& flows) {
    if (!accessors_ || !accessors_->waterAccessor) return;  // Safety check

    int water = accessors_->waterAccessor->getValue(openvdb::Coord(x, y, z));
    if (water <= 0) return;

    // Process water flow logic using cached accessors
    // Check if this voxel can flow to neighbors
    int mainType = accessors_->mainTypeAccessor->getValue(openvdb::Coord(x, y, z));
    int subType0 = accessors_->subType0Accessor->getValue(openvdb::Coord(x, y, z));

    // Simple flow downward if space available
    int belowWater = accessors_->waterAccessor->getValue(openvdb::Coord(x, y, z - 1));
    int belowMainType = accessors_->mainTypeAccessor->getValue(openvdb::Coord(x, y, z - 1));

    // If there's space below and we have water to flow
    if (belowMainType == -2 ||
        belowWater < 100) {  // -2 = empty space, or below has room for more water
        int flowAmount = std::min(water / 2, 10);  // Flow half the water, max 10 units
        if (flowAmount > 0) {
            flows.emplace_back(WaterFlowType::WATER_FLOW, x, y, z, flowAmount, x, y, z - 1);
        }
    }
}

void GridBoxProcessor::processVoxelEvaporation(int x, int y, int z, std::vector<WaterFlow>& flows) {
    if (!accessors_ || !accessors_->waterAccessor) return;  // Safety check

    int water = accessors_->waterAccessor->getValue(openvdb::Coord(x, y, z));
    if (water <= 0) return;

    // Simple evaporation logic - convert small amounts of water to vapor
    if (water > 0 && water < 50) {                      // Only evaporate small amounts
        int evaporationRate = std::max(1, water / 10);  // Evaporate 10% minimum 1
        flows.emplace_back(WaterFlowType::EVAPORATION, x, y, z, evaporationRate);
    }
}

/**********************
 *
 * WaterSimulationManager Implementation *
 *
 * ********************
 */

WaterSimulationManager::WaterSimulationManager(int numThreads)
    : numThreads_(numThreads), stopWorkers_(false), activeWorkers_(0), completedTasks_(0) {
    processors_.reserve(numThreads_);
    workerThreads_.reserve(numThreads_);
}

WaterSimulationManager::~WaterSimulationManager() { stopWorkerThreads(); }

void WaterSimulationManager::initializeProcessors(
    entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher) {
    processors_.clear();
    processors_.reserve(numThreads_);

    // Pre-compute grid boxes using minimum box dimensions for optimal cache performance
    GridBox minBoxDimensions(0, 0, 0, DEFAULT_MIN_BOX_SIZE - 1, DEFAULT_MIN_BOX_SIZE - 1,
                             DEFAULT_MIN_BOX_SIZE - 1);
    gridBoxes_ = partitionGridIntoBoxes(voxelGrid, minBoxDimensions);

    for (int i = 0; i < numThreads_; ++i) {
        auto processor = std::make_unique<GridBoxProcessor>();
        processor->initializeAccessors(registry, voxelGrid, dispatcher);
        processors_.push_back(std::move(processor));
    }

    // Start worker threads after processors are initialized
    startWorkerThreads(registry, voxelGrid);
}

void WaterSimulationManager::startWorkerThreads(entt::registry& registry, VoxelGrid& voxelGrid) {
    // Only start threads if they don't already exist
    if (!workerThreads_.empty()) {
        return;  // Threads already running
    }

    stopWorkers_ = false;
    activeWorkers_ = 0;
    completedTasks_ = 0;

    for (int i = 0; i < numThreads_; ++i) {
        workerThreads_.emplace_back(&WaterSimulationManager::workerThreadFunction, this, i,
                                    std::ref(registry), std::ref(voxelGrid));
    }
}

void WaterSimulationManager::stopWorkerThreads() {
    stopWorkers_ = true;
    taskAvailable_.notify_all();

    for (auto& thread : workerThreads_) {
        std::cout << "[WaterSimulationManager] Stopping worker thread " << thread.get_id() << ".\n";
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();
}

void WaterSimulationManager::workerThreadFunction(int threadId, entt::registry& registry,
                                                  VoxelGrid& voxelGrid) {
    while (!stopWorkers_) {
        size_t boxIndex;
        float sunIntensity;
        bool hasTask = scheduler_.getNextTask(boxIndex, sunIntensity);

        if (!hasTask) {
            // No tasks available, wait briefly or until notified
            std::unique_lock<std::mutex> lock(taskMutex_);
            taskAvailable_.wait_for(lock, std::chrono::milliseconds(1));
            continue;
        }

        if (boxIndex >= gridBoxes_.size()) {
            continue;  // Safety check
        }

        activeWorkers_++;

        // Process the box
        auto modifications = processBoxConcurrently(threadId % processors_.size(),
                                                    gridBoxes_[boxIndex], sunIntensity);

        // Push results to concurrent queue
        resultQueue_.push(std::move(modifications));

        activeWorkers_--;
        completedTasks_++;
    }
}

void WaterSimulationManager::populateSchedulerWithSubset(float percentage, float sunIntensity) {
    if (gridBoxes_.empty()) return;

    size_t numBoxesToAdd = static_cast<size_t>(gridBoxes_.size() * percentage);
    numBoxesToAdd = std::max(static_cast<size_t>(1), numBoxesToAdd);  // Ensure at least 1 box

    // Add boxes in round-robin fashion up to the specified percentage
    static size_t startIndex = 0;  // Static to maintain state between calls

    for (size_t i = 0; i < numBoxesToAdd; ++i) {
        size_t boxIndex = (startIndex + i) % gridBoxes_.size();
        scheduler_.addTask(boxIndex, sunIntensity);
    }

    // Update start index for next call to ensure different boxes are processed
    startIndex = (startIndex + numBoxesToAdd) % gridBoxes_.size();
}

std::vector<GridBox> WaterSimulationManager::partitionGridIntoBoxes(
    const VoxelGrid& voxelGrid, const GridBox& minBoxDimensions) {
    std::vector<GridBox> boxes;

    // Get grid dimensions
    int width = voxelGrid.width;
    int height = voxelGrid.height;
    int depth = voxelGrid.depth;

    // Extract minimum box dimensions
    int minBoxWidth = minBoxDimensions.maxX - minBoxDimensions.minX + 1;
    int minBoxHeight = minBoxDimensions.maxY - minBoxDimensions.minY + 1;
    int minBoxDepth = minBoxDimensions.maxZ - minBoxDimensions.minZ + 1;

    // Ensure minimum dimensions are at least 1
    minBoxWidth = std::max(1, minBoxWidth);
    minBoxHeight = std::max(1, minBoxHeight);
    minBoxDepth = std::max(1, minBoxDepth);

    std::cout << "Partitioning grid of size " << width << "x" << height << "x" << depth
              << " into boxes of minimum size " << minBoxWidth << "x" << minBoxHeight << "x"
              << minBoxDepth << std::endl;

    // Partition grid using minimum box dimensions
    // Create boxes that are at least minBox size, with smaller edge boxes if needed
    for (int z = 0; z < depth; z += minBoxDepth) {
        for (int y = 0; y < height; y += minBoxHeight) {
            for (int x = 0; x < width; x += minBoxWidth) {
                // Calculate box boundaries, ensuring we don't exceed grid bounds
                int maxX = std::min(x + minBoxWidth - 1, width - 1);
                int maxY = std::min(y + minBoxHeight - 1, height - 1);
                int maxZ = std::min(z + minBoxDepth - 1, depth - 1);

                // Only create box if it has positive dimensions
                if (x <= maxX && y <= maxY && z <= maxZ) {
                    boxes.emplace_back(x, y, z, maxX, maxY, maxZ);
                }
            }
        }
    }

    std::cout << "Partitioned grid into " << boxes.size() << " boxes using minimum box size of "
              << minBoxWidth << "x" << minBoxHeight << "x" << minBoxDepth << std::endl;

    return boxes;
}

std::vector<WaterFlow> WaterSimulationManager::processBoxConcurrently(int processorIndex,
                                                                      const GridBox& box,
                                                                      float sunIntensity) {
    if (processorIndex >= processors_.size()) {
        return {};  // Safety check
    }

    // Use shared_lock for concurrent reads
    std::shared_lock<std::shared_mutex> readLock(gridWriteMutex_);
    return processors_[processorIndex]->processBox(box, sunIntensity);
}

void WaterSimulationManager::applyModificationsWithLock(
    entt::registry& registry, VoxelGrid& voxelGrid, const std::vector<WaterFlow>& modifications) {
    // Apply modifications sequentially with exclusive write lock
    std::unique_lock<std::shared_mutex> writeLock(gridWriteMutex_);

    // TerrainStorage* storage = voxelGrid.terrainStorage.get();
    // if (!storage) return;

    // for (const auto& flow : modifications) {
    //     switch (flow.type) {
    //         case WaterFlowType::WATER_FLOW: {
    //             // Transfer water from source to target
    //             int currentWater = storage->getTerrainWaterMatter(flow.x, flow.y, flow.z);
    //             int targetWater = storage->getTerrainWaterMatter(flow.targetX, flow.targetY,
    //             flow.targetZ);

    //             int transferAmount = std::min(flow.amount, currentWater);
    //             if (transferAmount > 0) {
    //                 storage->setTerrainWaterMatter(flow.x, flow.y, flow.z, currentWater -
    //                 transferAmount); storage->setTerrainWaterMatter(flow.targetX, flow.targetY,
    //                 flow.targetZ, targetWater + transferAmount);
    //             }
    //             break;
    //         }
    //         case WaterFlowType::EVAPORATION: {
    //             // Convert water to vapor
    //             int currentWater = storage->getTerrainWaterMatter(flow.x, flow.y, flow.z);
    //             int currentVapor = storage->getTerrainVaporMatter(flow.x, flow.y, flow.z);

    //             int evaporateAmount = std::min(flow.amount, currentWater);
    //             if (evaporateAmount > 0) {
    //                 storage->setTerrainWaterMatter(flow.x, flow.y, flow.z, currentWater -
    //                 evaporateAmount); storage->setTerrainVaporMatter(flow.x, flow.y, flow.z,
    //                 currentVapor + evaporateAmount);
    //             }
    //             break;
    //         }
    //         case WaterFlowType::CONDENSATION: {
    //             // Convert vapor to water
    //             int currentVapor = storage->getTerrainVaporMatter(flow.x, flow.y, flow.z);
    //             int currentWater = storage->getTerrainWaterMatter(flow.x, flow.y, flow.z);

    //             int condenseAmount = std::min(flow.amount, currentVapor);
    //             if (condenseAmount > 0) {
    //                 storage->setTerrainVaporMatter(flow.x, flow.y, flow.z, currentVapor -
    //                 condenseAmount); storage->setTerrainWaterMatter(flow.x, flow.y, flow.z,
    //                 currentWater + condenseAmount);
    //             }
    //             break;
    //         }
    //     }
    // }
}

void WaterSimulationManager::processWaterSimulation(entt::registry& registry, VoxelGrid& voxelGrid,
                                                    float sunIntensity) {
    // Clear previous results
    std::vector<WaterFlow> temp;
    while (resultQueue_.try_pop(temp)) {
        // Clear queue
    }

    // Check if scheduler has available slots and add 30% of boxes if needed
    if (scheduler_.empty() || scheduler_.size() < (gridBoxes_.size() * 0.1f)) {
        populateSchedulerWithSubset(0.3f, sunIntensity);

        // Notify workers about new tasks
        taskAvailable_.notify_all();
    }

    // Collect any completed results (non-blocking)
    std::vector<WaterFlow> allModifications;
    std::vector<WaterFlow> modifications;

    while (resultQueue_.try_pop(modifications)) {
        allModifications.insert(allModifications.end(), modifications.begin(), modifications.end());
    }

    // Apply modifications if any were collected
    if (!allModifications.empty()) {
        applyModificationsWithLock(registry, voxelGrid, allModifications);
    }
}

bool isTerrainSoftEmpty(EntityTypeComponent& terrainType) {
    return (terrainType.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            terrainType.subType0 == static_cast<int>(TerrainEnum::EMPTY));
}

bool isTerrainVoxelEmptyOrSoftEmpty(entt::registry& registry, VoxelGrid& voxelGrid, const int x,
                                    const int y, const int z) {
    int terrainId = voxelGrid.getTerrain(x, y, z);
    if (terrainId < static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // Invalid terrain ID
        std::ostringstream ossMessage;
        ossMessage << "[isTerrainVoxelEmptyOrSoftEmpty] Error: Invalid terrain ID " << terrainId
                   << " at (" << x << ", " << y << ", " << z << ")\n";
        spdlog::get("console")->error(ossMessage.str());
        return true;
    } else if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        // This should not happen; means vapor entity is missing in voxel grid
        // std::ostringstream ossMessage;
        // ossMessage << "[isTerrainVoxelEmptyOrSoftEmpty] Error: Vapor entity in ON_GRID_STORAGE at
        // ("
        //             << x << ", " << y << ", " << z << ")\n";
        // spdlog::get("console")->error(ossMessage.str());
        return false;
    } else if (terrainId > 0) {
        // Voxel is completely empty
        auto terrain = static_cast<entt::entity>(terrainId);
        EntityTypeComponent* type = registry.try_get<EntityTypeComponent>(terrain);
        const bool isVoxelEmpty{terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)};
        const bool isSoftEmpty{(type && isTerrainSoftEmpty(*type))};
        const bool isEmpty{isVoxelEmpty || isSoftEmpty};
        return isEmpty;
    }

    // Review this after fixing the current bug.
    return false;
}

// void setVaporSI(entt::registry& registry, entt::entity& terrain) {
void setVaporSI(int x, int y, int z, VoxelGrid& voxelGrid) {
    StructuralIntegrityComponent terrainSI =
        voxelGrid.terrainGridRepository->getTerrainStructuralIntegrity(x, y, z);
    terrainSI.canStackEntities = false;
    terrainSI.maxLoadCapacity = -1;
    terrainSI.matterState = MatterState::GAS;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, terrainSI);
}

void setEmptyWaterComponentsStorage(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId,
                                    int x, int y, int z, MatterState matterState) {
    // Part 1: Set EntityTypeComponent
    EntityTypeComponent* terrainType = new EntityTypeComponent();
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
    terrainType->subType1 = 0;
    voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z, *terrainType);

    // Part 2: Set StructuralIntegrityComponent
    StructuralIntegrityComponent* terrainSI = new StructuralIntegrityComponent();
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = matterState;
    voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(x, y, z, *terrainSI);

    // Part 3: Set MatterContainer
    MatterContainer* terrainMC = new MatterContainer();
    terrainMC->TerrainMatter = 0;
    terrainMC->WaterMatter = 0;
    terrainMC->WaterVapor = 0;
    terrainMC->BioMassMatter = 0;
    voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z, *terrainMC);
}

void setEmptyWaterComponentsEnTT(entt::registry& registry, entt::entity& terrain,
                                 MatterState matterState) {
    EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
    bool shouldEmplaceTerrainType{terrainType == nullptr};
    if (terrainType == nullptr) {
        terrainType = new EntityTypeComponent();
    }
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::WATER);
    terrainType->subType1 = 0;
    if (shouldEmplaceTerrainType) {
        registry.emplace<EntityTypeComponent>(terrain, *terrainType);
    }

    StructuralIntegrityComponent* terrainSI =
        registry.try_get<StructuralIntegrityComponent>(terrain);
    bool shouldEmplaceTerrainSI{terrainSI == nullptr};
    if (terrainSI == nullptr) {
        terrainSI = new StructuralIntegrityComponent();
    }
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = matterState;
    if (shouldEmplaceTerrainSI) {
        registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
    }

    MatterContainer* terrainMC = registry.try_get<MatterContainer>(terrain);
    bool shouldEmplaceTerrainMC{terrainMC == nullptr};
    if (terrainMC == nullptr) {
        terrainMC = new MatterContainer();
    }
    terrainMC->TerrainMatter = 0;
    terrainMC->WaterMatter = 0;
    terrainMC->WaterVapor = 0;
    terrainMC->BioMassMatter = 0;
    if (shouldEmplaceTerrainMC) {
        registry.emplace<MatterContainer>(terrain, *terrainMC);
    }
}

void convertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId, int x,
                               int y, int z) {
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // Create new terrain entity for the empty voxel
    } else if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        setEmptyWaterComponentsStorage(registry, voxelGrid, terrainId, x, y, z,
                                       MatterState::LIQUID);
        // Create new terrain entity for the completely empty voxel
    } else {
        // Convert existing soft empty terrain entity to water
        auto terrain = static_cast<entt::entity>(terrainId);
        setEmptyWaterComponentsEnTT(registry, terrain, MatterState::LIQUID);
    }
}

void convertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId, int x,
                               int y, int z) {
    std::cout << "[convertSoftEmptyIntoVapor] Just marking a checkpoint on logs." << std::endl;
    // setEmptyWaterComponents(registry, terrain, MatterState::GAS);
}

void convertIntoSoftEmpty(entt::registry& registry, entt::entity& terrain) {
    EntityTypeComponent* terrainType = registry.try_get<EntityTypeComponent>(terrain);
    bool shouldEmplaceTerrainType{terrainType == nullptr};
    if (terrainType == nullptr) {
        terrainType = new EntityTypeComponent();
    }
    terrainType->mainType = static_cast<int>(EntityEnum::TERRAIN);
    terrainType->subType0 = static_cast<int>(TerrainEnum::EMPTY);
    terrainType->subType1 = 0;
    if (shouldEmplaceTerrainType) {
        registry.emplace<EntityTypeComponent>(terrain, *terrainType);
    }

    StructuralIntegrityComponent* terrainSI =
        registry.try_get<StructuralIntegrityComponent>(terrain);
    bool shouldEmplaceTerrainSI{terrainSI == nullptr};
    if (terrainSI == nullptr) {
        terrainSI = new StructuralIntegrityComponent();
    }
    terrainSI->canStackEntities = false;
    terrainSI->maxLoadCapacity = -1;
    terrainSI->matterState = MatterState::GAS;
    if (shouldEmplaceTerrainSI) {
        registry.emplace<StructuralIntegrityComponent>(terrain, *terrainSI);
    }
}

bool getTypeAndCheckSoftEmpty(entt::registry& registry, VoxelGrid& voxelGrid, int terrainId, int x,
                              int y, int z) {
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        return false;  // Means terrain voxel is empty.
    } else if (terrainId == -1) {
        return false;  // Means terrain voxel is completely empty (no entity).
        EntityTypeComponent terrainEntityType =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
        const bool isTerrainNeighborSoftEmpty{isTerrainSoftEmpty(terrainEntityType)};
        return isTerrainNeighborSoftEmpty;
    } else {
        // TODO: This is not being handled. It should be handled when active EnTT terrains start to
        // be used.
        std::cout << "[getTypeAndCheckSoftEmpty]: terrainId is neither -1 nor -2. Not handled yet."
                  << std::endl;
        return false;
    }
}

void checkAndConvertSoftEmptyIntoWater(entt::registry& registry, VoxelGrid& voxelGrid,
                                       int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoWater(registry, voxelGrid, terrainId, x, y, z);
    }
}

void checkAndConvertSoftEmptyIntoVapor(entt::registry& registry, VoxelGrid& voxelGrid,
                                       int terrainId, int x, int y, int z) {
    if (getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainId, x, y, z)) {
        convertSoftEmptyIntoVapor(registry, voxelGrid, terrainId, x, y, z);
    }
}

void deleteEntityOrConvertInEmpty(entt::registry& registry, entt::dispatcher& dispatcher,
                                  entt::entity& terrain) {
    TileEffectsList* terrainEffectsList = registry.try_get<TileEffectsList>(terrain);
    if (terrainEffectsList == nullptr ||
        (terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty())) {
        dispatcher.enqueue<KillEntityEvent>(terrain);
    } else {
        // Convert into empty terrain because there are effects being processed
        std::cout << "terrainEffectsList && terrainEffectsList->tileEffectsIDs.empty(): is "
                     "False... converting into soft empty"
                  << std::endl;
        convertIntoSoftEmpty(registry, terrain);
    }
}

/**********************
 *
 * Liquid Water moving logic *
 *
 * ********************
 */

std::tuple<bool, bool> isNeighborWaterOrEmpty(entt::registry& registry, VoxelGrid& voxelGrid,
                                              const int x, const int y, const int z) {
    int terrainNeighborId = voxelGrid.getTerrain(x, y, z);
    bool isNeighborEmpty = (terrainNeighborId == static_cast<int>(TerrainIdTypeEnum::NONE));
    bool isTerrainNeighborSoftEmpty{false};
    bool isNeighborWater = false;
    // TODO: Uncomment and handle this properly when active EnTT terrains start to be used.
    if (terrainNeighborId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // auto terrainNeighbor = static_cast<entt::entity>(terrainNeighborId);
        isTerrainNeighborSoftEmpty =
            getTypeAndCheckSoftEmpty(registry, voxelGrid, terrainNeighborId, x, y, z);
        EntityTypeComponent typeNeighbor =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
        MatterContainer matterContainerNeighbor =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);
        isNeighborWater = (typeNeighbor.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                           typeNeighbor.subType0 == static_cast<int>(TerrainEnum::WATER));
    }
    isNeighborEmpty = isNeighborEmpty || isTerrainNeighborSoftEmpty;
    return std::make_tuple(isNeighborEmpty, isNeighborWater);
}

void makePlantSuckWater(entt::registry& registry, entt::entity& terrainEntity,
                        entt::entity& plantEntity, bool& actionPerformed) {
    PlantResources* plantResourcesPtr = registry.try_get<PlantResources>(plantEntity);

    auto& matterContainer = registry.get<MatterContainer>(terrainEntity);
    if (plantResourcesPtr && plantResourcesPtr->water < 6 && matterContainer.WaterMatter > 0) {
        matterContainer.WaterMatter -= 1;
        plantResourcesPtr->water += 1.0;
        // std::cout << "plant sucking water..." << std::endl;
        actionPerformed = true;
    } else if (plantResourcesPtr == nullptr && matterContainer.WaterMatter > 0) {
        PlantResources plantResources = PlantResources();
        matterContainer.WaterMatter -= 1;
        plantResources.water += 1.0;

        registry.emplace<PlantResources>(plantEntity, plantResources);
        // std::cout << "plant sucking water..." << std::endl;
        actionPerformed = true;
    }
}

void spreadWater(int terrainId, int terrainX, int terrainY, int terrainZ, entt::registry& registry,
                 VoxelGrid& voxelGrid, entt::dispatcher& dispatcher, entt::entity entity,
                 EntityTypeComponent& type, MatterContainer& matterContainer, int x, int y, int z,
                 DirectionEnum direction) {
    auto logger = Logger::getLogger();
    if (matterContainer.WaterMatter <= 0) {
        // No water to spread
        return;
    }

    int terrainNeighborId = voxelGrid.getTerrain(x, y, z);
    bool actionPerformed = false;
    const bool isAboveNeighborEmpty{
        isTerrainVoxelEmptyOrSoftEmpty(registry, voxelGrid, x, y, z + 1)};

    if (terrainNeighborId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        auto terrainNeighbor = static_cast<entt::entity>(terrainNeighborId);
        // checkAndConvertSoftEmptyIntoWater(registry, terrainNeighbor);
        EntityTypeComponent typeNeighbor =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z);
        MatterContainer matterContainerNeighbor =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);

        // if (registry.all_of<EntityTypeComponent, MatterContainer>(terrainNeighbor)) {
        //     // Neighbor tile has terrain
        //     auto&& [typeNeighbor, matterContainerNeighbor] =
        //         registry.get<EntityTypeComponent, MatterContainer>(terrainNeighbor);

        // Water can only be taken from a water terrain and moved to a terrain that is not
        // higher (not full type of terrain).
        const int TERRAIN_MAIN_TYPE = static_cast<int>(EntityEnum::TERRAIN);
        const int GRASS_SUB_TYPE0 = static_cast<int>(TerrainEnum::GRASS);
        const int WATER_SUB_TYPE0 = static_cast<int>(TerrainEnum::WATER);
        const int TERRAIN_SUB_TYPE1_FULL = 0;

        bool canSpredWaterToNotFull =
            (type.mainType == TERRAIN_MAIN_TYPE && type.subType0 == WATER_SUB_TYPE0 &&
             typeNeighbor.mainType == TERRAIN_MAIN_TYPE &&
             typeNeighbor.subType0 == GRASS_SUB_TYPE0 &&
             typeNeighbor.subType1 != TERRAIN_SUB_TYPE1_FULL);

        if (!actionPerformed && canSpredWaterToNotFull && matterContainer.WaterMatter > 0 &&
            matterContainerNeighbor.WaterVapor == 0 && matterContainerNeighbor.WaterMatter < 4 &&
            matterContainer.WaterMatter > matterContainerNeighbor.WaterMatter) {
            // Transfer water to neighbor's MatterContainer
            int transferAmount = 1;
            matterContainerNeighbor.WaterMatter += transferAmount;
            matterContainer.WaterMatter -= transferAmount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                                       matterContainerNeighbor);
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(terrainX, terrainY, terrainZ,
                                                                       matterContainer);
            actionPerformed = true;
        }

        bool canSpredWaterToWater =
            (type.subType0 == WATER_SUB_TYPE0 && typeNeighbor.subType0 == WATER_SUB_TYPE0);

        if (!actionPerformed && canSpredWaterToWater && matterContainer.WaterMatter > 0 &&
            matterContainerNeighbor.WaterVapor == 0 && matterContainerNeighbor.WaterMatter < 14 &&
            matterContainer.WaterMatter > matterContainerNeighbor.WaterMatter) {
            // Transfer water to neighbor's MatterContainer
            int transferAmount = 1;
            matterContainerNeighbor.WaterMatter += transferAmount;
            matterContainer.WaterMatter -= transferAmount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                                       matterContainerNeighbor);
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(terrainX, terrainY, terrainZ,
                                                                       matterContainer);
            actionPerformed = true;
        }

        const bool canSpredGrassToGrass =
            (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
             type.subType0 == static_cast<int>(TerrainEnum::GRASS) &&
             type.subType1 == static_cast<int>(TerrainVariantEnum::FULL) &&
             typeNeighbor.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
             typeNeighbor.subType0 == static_cast<int>(TerrainEnum::GRASS) &&
             (typeNeighbor.subType1 == static_cast<int>(TerrainVariantEnum::FULL) ||
              typeNeighbor.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_EAST) ||
              typeNeighbor.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_NORTH) ||
              typeNeighbor.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_WEST) ||
              typeNeighbor.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_SOUTH)) &&
             isAboveNeighborEmpty);

        if (!actionPerformed && canSpredGrassToGrass && matterContainer.WaterMatter > 0 &&
            matterContainerNeighbor.WaterVapor == 0 && matterContainerNeighbor.WaterMatter < 4) {
            // Transfer water to neighbor's MatterContainer
            int transferAmount = 1;
            matterContainerNeighbor.WaterMatter += transferAmount;
            matterContainer.WaterMatter -= transferAmount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z,
                                                                       matterContainerNeighbor);
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(terrainX, terrainY, terrainZ,
                                                                       matterContainer);
            std::cout << "[spreadWater] Spreading water to grass terrain at (" << x << ", " << y
                      << ", " << z << ")\n";
            actionPerformed = true;
        }
        // }
    }
    // TODO: Test first without spreading to empty tiles. Uncomment.
    // else {
    //     // Neighbor tile has no terrain
    //     bool directionOutOfBounds = (x < 0 || x > voxelGrid.width - 1 || y < 0 ||
    //                                  y > voxelGrid.height - 1 || z < 0 || z > voxelGrid.depth -
    //                                  1);

    //     const bool canSpredGrassRampToEmpty =
    //         (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
    //          type.subType0 == static_cast<int>(TerrainEnum::GRASS) &&
    //          (type.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_EAST) ||
    //           type.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_NORTH) ||
    //           type.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_WEST) ||
    //           type.subType1 == static_cast<int>(TerrainVariantEnum::RAMP_SOUTH)) &&
    //          isAboveNeighborEmpty);

    //     if (!actionPerformed && !directionOutOfBounds && canSpredGrassRampToEmpty &&
    //         matterContainer.WaterMatter > 0) {
    //         const int transferAmount = 1;
    //         Position pos{x, y, z};
    //         WaterFallEntityEvent waterFallEntityEvent{entity, pos, transferAmount};
    //         pendingWaterFall.push(waterFallEntityEvent);
    //         actionPerformed = true;
    //     }

    //     const bool canSpredWaterToEmpty =
    //         (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
    //          type.subType0 == static_cast<int>(TerrainEnum::WATER) && isAboveNeighborEmpty);

    //     if (!actionPerformed && !directionOutOfBounds && canSpredWaterToEmpty &&
    //         matterContainer.WaterMatter > 0) {
    //         const int transferAmount = 1;
    //         Position pos{x, y, z};
    //         WaterFallEntityEvent waterFallEntityEvent{entity, pos, transferAmount};
    //         pendingWaterFall.push(waterFallEntityEvent);

    //         actionPerformed = true;
    //     }
    // }
}

bool moveWater(int terrainEntityId, entt::registry& registry, VoxelGrid& voxelGrid,
               entt::dispatcher& dispatcher, bool& actionPerformed, Position& pos,
               EntityTypeComponent& type, MatterContainer& matterContainer, std::random_device& rd,
               std::mt19937& gen, std::uniform_int_distribution<>& disWaterSpreading) {
    const bool isGrass = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                          type.subType0 == static_cast<int>(TerrainEnum::GRASS));
    const bool isWater = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                          type.subType0 == static_cast<int>(TerrainEnum::WATER));
    int terrainBellowId = voxelGrid.getTerrain(pos.x, pos.y, pos.z - 1);

    bool isBellowGrass = false;
    bool canSpredWaterDown = false;

    bool haveMovement = false;
    entt::entity terrain;
    if (terrainEntityId != -1 && terrainEntityId != -2) {
        terrain = static_cast<entt::entity>(terrainEntityId);
        haveMovement = registry.all_of<MovingComponent>(terrain);
    } else if (terrainEntityId == -1) {
        terrain = entt::null;
        haveMovement = false;
    } else if (terrainEntityId == -2) {
        terrain = entt::null;
        haveMovement = false;
    }

    if (terrainBellowId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        // auto terrainBellow = static_cast<entt::entity>(terrainBellowId);
        checkAndConvertSoftEmptyIntoWater(registry, voxelGrid, terrainBellowId, pos.x, pos.y,
                                          pos.z - 1);
        EntityTypeComponent typeBellow =
            voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z - 1);
        MatterContainer matterContainerBellow =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z - 1);

        // Water can only be taken from a water terrain and moved to a terrain that is not
        // higher (not full type of terrain).
        const bool isBellowWater = (typeBellow.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                                    typeBellow.subType0 == static_cast<int>(TerrainEnum::WATER));
        isBellowGrass = (typeBellow.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                         typeBellow.subType0 == static_cast<int>(TerrainEnum::GRASS));
        canSpredWaterDown =
            (!haveMovement &&
             ((isWater && isBellowWater && matterContainerBellow.WaterMatter < 14 &&
               matterContainerBellow.WaterVapor == 0 && matterContainer.WaterMatter > 0) ||
              (isWater && isBellowGrass && matterContainerBellow.WaterMatter < 4 &&
               matterContainerBellow.WaterVapor == 0 && matterContainer.WaterMatter > 0)));
        if (canSpredWaterDown) {
            // Transfer water to neighbor's MatterContainer
            int transferAmount = 1;
            matterContainerBellow.WaterMatter += transferAmount;
            matterContainer.WaterMatter -= transferAmount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z - 1,
                                                                       matterContainerBellow);
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z,
                                                                       matterContainer);
            actionPerformed = true;
        }
        // }
    }

    // TODO: Uncomment when movingDirection algorithm and plant sucking water is ready.
    if (!actionPerformed && !haveMovement) {
        int movingDirection{};
        if (isWater) {
            if (terrainBellowId != static_cast<int>(TerrainIdTypeEnum::NONE) && isBellowGrass &&
                !canSpredWaterDown) {
                Position positionBellow =
                    voxelGrid.terrainGridRepository->getPosition(pos.x, pos.y, pos.z - 1);
                movingDirection = static_cast<int>(positionBellow.direction);

                bool isNeighborEmpty = false;
                bool isNeighborWater = false;
                if (movingDirection == static_cast<int>(DirectionEnum::UP)) {
                    std::tie(isNeighborEmpty, isNeighborWater) =
                        isNeighborWaterOrEmpty(registry, voxelGrid, pos.x, pos.y - 1, pos.z);
                    if (!isNeighborEmpty && isNeighborWater) {
                        movingDirection = disWaterSpreading(gen);
                    }

                } else if (movingDirection == static_cast<int>(DirectionEnum::LEFT)) {
                    std::tie(isNeighborEmpty, isNeighborWater) =
                        isNeighborWaterOrEmpty(registry, voxelGrid, pos.x - 1, pos.y, pos.z);
                    if (!isNeighborEmpty && isNeighborWater) {
                        movingDirection = disWaterSpreading(gen);
                    }

                } else if (movingDirection == static_cast<int>(DirectionEnum::RIGHT)) {
                    std::tie(isNeighborEmpty, isNeighborWater) =
                        isNeighborWaterOrEmpty(registry, voxelGrid, pos.x + 1, pos.y, pos.z);
                    if (!isNeighborEmpty && isNeighborWater) {
                        movingDirection = disWaterSpreading(gen);
                    }

                } else if (movingDirection == static_cast<int>(DirectionEnum::DOWN)) {
                    std::tie(isNeighborEmpty, isNeighborWater) =
                        isNeighborWaterOrEmpty(registry, voxelGrid, pos.x, pos.y + 1, pos.z);
                    if (!isNeighborEmpty && isNeighborWater) {
                        movingDirection = disWaterSpreading(gen);
                    }
                }

            } else {
                movingDirection = disWaterSpreading(gen);
            }
        } else if (isGrass) {
            EntityTypeComponent entityAboveType =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z + 1);

            bool entityAbovePLant{};
            entityAbovePLant = (entityAboveType.mainType == static_cast<int>(EntityEnum::PLANT));

            if (entityAbovePLant) {
                // TODO: Uncomment when plant sucking water is ready.
                // makePlantSuckWater(registry, entity, entityAbove, actionPerformed);
            }

            movingDirection = static_cast<int>(pos.direction);
        }

        if (!actionPerformed) {
            if (movingDirection == static_cast<int>(DirectionEnum::UP)) {
                spreadWater(terrainEntityId, pos.x, pos.y, pos.z, registry, voxelGrid, dispatcher,
                            terrain, type, matterContainer, pos.x, pos.y - 1, pos.z,
                            static_cast<DirectionEnum>(movingDirection));
            } else if (movingDirection == static_cast<int>(DirectionEnum::LEFT)) {
                spreadWater(terrainEntityId, pos.x, pos.y, pos.z, registry, voxelGrid, dispatcher,
                            terrain, type, matterContainer, pos.x - 1, pos.y, pos.z,
                            static_cast<DirectionEnum>(movingDirection));
            } else if (movingDirection == static_cast<int>(DirectionEnum::RIGHT)) {
                spreadWater(terrainEntityId, pos.x, pos.y, pos.z, registry, voxelGrid, dispatcher,
                            terrain, type, matterContainer, pos.x + 1, pos.y, pos.z,
                            static_cast<DirectionEnum>(movingDirection));
            } else if (movingDirection == static_cast<int>(DirectionEnum::DOWN)) {
                spreadWater(terrainEntityId, pos.x, pos.y, pos.z, registry, voxelGrid, dispatcher,
                            terrain, type, matterContainer, pos.x, pos.y + 1, pos.z,
                            static_cast<DirectionEnum>(movingDirection));
            }
            actionPerformed = true;
        }
    }

    return actionPerformed;
}

/**********************
 *
 * Phase Changes *
 *
 * ********************
 */

// Evaporation

void createOrAddVapor(entt::registry& registry, VoxelGrid& voxelGrid, int x, int y, int z,
                      int amount) {
    int terrainAboveId = voxelGrid.getTerrain(x, y, z + 1);

    if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        entt::entity terrainAbove = entt::null;
        // if (voxelGrid.terrainGridRepository->isTerrainIdOnEnttRegistry(terrainAboveId)) {
        //     terrainAbove = static_cast<entt::entity>(terrainAboveId);
        // } else {
        //     throw std::runtime_error("[createOrAddVapor] Error: terrainAboveId is not on EnTT registry.");
        // }
        EntityTypeComponent typeAbove =
            voxelGrid.terrainGridRepository->getTerrainEntityType(x, y, z + 1);
        MatterContainer matterContainerAbove =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z + 1);
        // checkAndConvertSoftEmptyIntoVapor(registry, terrainAbove);
        checkAndConvertSoftEmptyIntoVapor(registry, voxelGrid, terrainAboveId, x, y, z + 1);
        // if (registry.all_of<EntityTypeComponent, MatterContainer>(terrainAbove)) {
        // There is an entity above
        // auto&& [typeAbove, matterContainerAbove] =
        //     registry.get<EntityTypeComponent, MatterContainer>(terrainAbove);

        // Check if it's vapor based on MatterContainer
        if (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
            typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER) &&
            matterContainerAbove.WaterVapor >= 0 && matterContainerAbove.WaterMatter == 0) {
            matterContainerAbove.WaterVapor += amount;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z + 1,
                                                                       matterContainerAbove);

            // TODO: Uncomment -- This might just be removed entirely after testing. Because SI is now a consistent part of storage.
            // setVaporSI(x, y, z + 1, voxelGrid);

            std::ostringstream ossMessage;
            ossMessage << "[createOrAddVapor] Adding vapor to existing vapor at (" << x << ", " << y << ", "
                        << z + 1 << ")";
            spdlog::get("console")->info(ossMessage.str());
        } else {
            std::ostringstream ossMessage;
            ossMessage << "[createOrAddVapor] Cannot add vapor above; obstruction at (" << x << ", " << y << ", "
                        << z + 1 << ")\n";
            spdlog::get("console")->info(ossMessage.str());
            // Cannot place vapor; perhaps handle obstruction
        }
        // }
    } else {
        // No entity above; create vapor entity

        std::ostringstream ossMessage;
        ossMessage << "[createOrAddVapor] Creating new vapor entity at (" << x << ", " << y << ", " << z + 1 << ")";
        spdlog::get("console")->info(ossMessage.str());

        if (voxelGrid.terrainGridRepository->checkIfTerrainHasEntity(x, y, z + 1)) {
            throw std::runtime_error("[createOrAddVapor] Error: Checkpoint bingo.");
        }

        // ============= NEW CODE =============

        auto newVaporEntity = registry.create();
        Position newPosition = {x, y, z + 1, DirectionEnum::DOWN};

        EntityTypeComponent newType = {};
        newType.mainType = 0;  // Terrain type
        newType.subType0 = 1;  // Water terrain (vapor)
        newType.subType1 = 0;

        MatterContainer newMatterContainer = {};
        newMatterContainer.WaterVapor = amount;
        newMatterContainer.WaterMatter = 0;  // Ensure WaterMatter is zero

        PhysicsStats newPhysicsStats = {};
        newPhysicsStats.mass = 0.1;
        newPhysicsStats.maxSpeed = 10;
        newPhysicsStats.minSpeed = 0.0;

        StructuralIntegrityComponent newStructuralIntegrityComponent = {};
        newStructuralIntegrityComponent.canStackEntities =
            false;  // Start with full structural integrity
        newStructuralIntegrityComponent.canStackEntities = -1;
        newStructuralIntegrityComponent.matterState = MatterState::GAS;

        // Uncomment

        voxelGrid.terrainGridRepository->setPosition(x, y, z + 1, newPosition);
        voxelGrid.terrainGridRepository->setTerrainEntityType(x, y, z + 1, newType);
        voxelGrid.terrainGridRepository->setTerrainMatterContainer(x, y, z + 1, newMatterContainer);
        voxelGrid.terrainGridRepository->setTerrainStructuralIntegrity(
            x, y, z + 1, newStructuralIntegrityComponent);
        voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z + 1, newPhysicsStats);
        int newTerrainId = static_cast<int>(newVaporEntity);
        std::cout << "[createOrAddVapor] Before setTerrainId with terrainId: " << newTerrainId << std::endl;
        voxelGrid.terrainGridRepository->setTerrainId(x, y, z + 1, newTerrainId);




        // ============= OLD CODE USING DIRECT EnTT EMPLACEMENT =============

        // voxelGrid.terrainGridRepository->setTerrainId(x, y, z + 1, -1);

        // registry.emplace<Position>(newVaporEntity, newPosition);
        // registry.emplace<EntityTypeComponent>(newVaporEntity, newType);
        // registry.emplace<MatterContainer>(newVaporEntity, newMatterContainer);
        // registry.emplace<StructuralIntegrityComponent>(newVaporEntity,
        //                                                newStructuralIntegrityComponent);
        // registry.emplace<PhysicsStats>(newVaporEntity, newPhysicsStats);

        // voxelGrid.setTerrain(x, y, z + 1, static_cast<int>(newVaporEntity));
    }
}

// Condensation

void condenseVapor(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                   entt::entity entity, Position& pos, EntityTypeComponent& type,
                   MatterContainer& matterContainer,
                   tbb::concurrent_queue<CondenseWaterEntityEvent>& pendingCondenseWater) {
    auto logger = Logger::getLogger();

    int condensationAmount = 1;

    std::ostringstream ossMessage;
    ossMessage << "Vapor condensing at (" << pos.x << ", " << pos.y << ", " << pos.z << ")";
    spdlog::get("console")->debug(ossMessage.str());
    // Convert vapor back to water
    int terrainBelowId = voxelGrid.getTerrain(pos.x, pos.y, pos.z - 1);
    if (terrainBelowId != -1) {
        auto terrainBelow = static_cast<entt::entity>(terrainBelowId);
        // checkAndConvertSoftEmptyIntoWater(registry, terrainBelow);
        if (registry.all_of<EntityTypeComponent, MatterContainer>(
                static_cast<entt::entity>(terrainBelowId))) {
            // There is a tile below
            auto&& [typeBelow, matterContainerBelow] =
                registry.get<EntityTypeComponent, MatterContainer>(
                    static_cast<entt::entity>(terrainBelowId));

            // Ensure matterContainerBelow only has WaterMatter
            if (typeBelow.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                typeBelow.subType0 == static_cast<int>(TerrainEnum::WATER) &&
                matterContainerBelow.WaterMatter >= 0 && matterContainerBelow.WaterVapor == 0) {
                matterContainerBelow.WaterMatter += condensationAmount;
                matterContainer.WaterVapor -= condensationAmount;

                if (matterContainer.WaterVapor <= 0) {
                    deleteEntityOrConvertInEmpty(registry, dispatcher, entity);
                }
            }
        }
    } else {
        CondenseWaterEntityEvent evaporateWaterEntityEvent{entity, condensationAmount};
        dispatcher.enqueue<CondenseWaterEntityEvent>(evaporateWaterEntityEvent);
        return;
    }
}

/**********************
 *
 * Vapor moving logic *
 *
 * ********************
 */

void moveVaporUp(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                 Position& pos, EntityTypeComponent& type,
                 MatterContainer& matterContainer) {
    voxelGrid.terrainGridRepository->lockTerrainGrid();

    int terrainId = voxelGrid.getTerrain(pos.x, pos.y, pos.z);
    // std::cout << "[moveVaporUp] terrainId at position (" << pos.x << ", " << pos.y << ", " << pos.z << ") is
    // " << terrainId << std::endl;
    if (terrainId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        voxelGrid.terrainGridRepository->unlockTerrainGrid();
        // This should not happen; means vapor entity is missing in voxel grid
        std::ostringstream ossMessage;
        ossMessage << "[moveVaporUp] Error: Vapor entity missing in voxel grid at (" << pos.x << ", "
                   << pos.y << ", " << pos.z << ")\n";
        spdlog::get("console")->error(ossMessage.str());
        return;  // Don't throw, just return
    } else if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
        voxelGrid.terrainGridRepository->unlockTerrainGrid();
        // This should not happen; means vapor entity is missing in voxel grid
        std::ostringstream ossMessage;
        ossMessage << "[moveVaporUp] Error: Vapor entity in ON_GRID_STORAGE at (" << pos.x << ", " << pos.y
                   << ", " << pos.z << ")\n";
        spdlog::get("console")->error(ossMessage.str());
        return;  // Don't throw, just return
    } else {
        // std::cout << "[moveVaporUp] terrainId is valid: " << terrainId << std::endl;
    }
    // THIS IS CRITICAL: converting terrainId to entity. Previous checks must have passed.
    entt::entity entity = static_cast<entt::entity>(terrainId);

    int maxAltitude = voxelGrid.depth - 1;
    float rhoEnv = 1.225f;    // Density of air
    float rhoVapor = 0.597f;  // Density of water vapor

    // Move vapor up
    int terrainAboveId = voxelGrid.getTerrain(pos.x, pos.y, pos.z + 1);
    bool haveMovement = registry.all_of<MovingComponent>(entity);
    if (terrainAboveId == static_cast<int>(TerrainIdTypeEnum::NONE) && pos.z < maxAltitude) {
        // std::cout << "[moveVaporUp (terrainAboveId == static_cast<int>(TerrainIdTypeEnum::NONE)
        // && pos.z < maxAltitude)] terrainAboveId at position (" << pos.x << ", " << pos.y << ", "
        // << pos.z + 1 << ") is " << terrainAboveId << std::endl; if (entity ==
        // entityBeingDebugged) {
        //     std::ostringstream ossMessage;
        //     ossMessage << "Vapor moving up from (" << pos.x << ", " << pos.y << ", " << pos.z
        //                << ")";
        //     std::cout << ossMessage.str() << std::endl;
        // }

        if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            entt::entity newTerrainEntity = registry.create();
            terrainId = static_cast<int>(newTerrainEntity);
            std::cout << "[moveVaporUp] Before setTerrainId with terrainId: " << terrainId << std::endl;
            voxelGrid.terrainGridRepository->setTerrainId(pos.x, pos.y, pos.z, terrainId);
        }
        entt::entity entity = static_cast<entt::entity>(terrainId);
        MoveGasEntityEvent moveGasEntityEvent{
            entity, Position{pos.x, pos.y, pos.z, DirectionEnum::DOWN}, 0.0f, 0.0f, rhoEnv, rhoVapor};
        moveGasEntityEvent.setForceApplyNewVelocity();

        // CRITICAL FIX: Release lock BEFORE dispatching event to avoid deadlock
        voxelGrid.terrainGridRepository->unlockTerrainGrid();
        
        dispatcher.enqueue<MoveGasEntityEvent>(moveGasEntityEvent);
        return;  // Early return after dispatching
        // std::cout << "[moveVaporUp (terrainAboveId == static_cast<int>(TerrainIdTypeEnum::NONE)
        // && pos.z < maxAltitude)] End of block." << std::endl;
    } else if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) && pos.z < maxAltitude) {
        // std::cout << "[moveVaporUp (pos.z < maxAltitude)] terrainAboveId at position (" << pos.x
        // << ", " << pos.y << ", " << pos.z + 1 << ") is " << terrainAboveId << std::endl;
        auto terrainAbove = static_cast<entt::entity>(terrainAboveId);
        // checkAndConvertSoftEmptyIntoVapor(registry, terrainAbove);
        EntityTypeComponent typeAbove =
            voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z + 1);
        MatterContainer matterContainerAbove =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z + 1);
        bool haveMovement;
        if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) ||
            terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            haveMovement = false;
        } else {
            haveMovement = registry.all_of<MovingComponent>(entity);
        }
        if (!haveMovement &&
            (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
             typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER)) &&
            (matterContainerAbove.WaterVapor >= 0 && matterContainerAbove.WaterMatter == 0)) {
            // if (entity == entityBeingDebugged) {
            //     std::ostringstream ossMessage;
            //     ossMessage << "Vapor merging with vapor above at (" << pos.x << ", " << pos.y
            //                << ", " << pos.z + 1 << ")" << " with " <<
            //                matterContainer.WaterVapor
            //                << " units of vapor;";
            //     std::cout << ossMessage.str() << std::endl;
            // }

            // Merge vapor
            std::cout << "[moveVaporUp] Merging vapor at (" << pos.x << ", " << pos.y << ", "
                      << pos.z << ") with vapor above at (" << pos.x << ", " << pos.y << ", "
                      << pos.z + 1 << ")\n";
            matterContainerAbove.WaterVapor += matterContainer.WaterVapor;
            matterContainer.WaterVapor = 0;
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z + 1,
                                                                       matterContainerAbove);
            voxelGrid.terrainGridRepository->setTerrainMatterContainer(pos.x, pos.y, pos.z,
                                                                       matterContainer);

            // CRITICAL FIX: Release lock BEFORE calling deleteEntityOrConvertInEmpty
            // (which may dispatch events)
            voxelGrid.terrainGridRepository->unlockTerrainGrid();
            
            deleteEntityOrConvertInEmpty(registry, dispatcher, entity);
            return;  // Early return after merging
            // } else {
            // if (entity == entityBeingDebugged) {
            //     std::ostringstream ossMessage;
            //     ossMessage << "Vapor cannot move up; obstruction at (" << pos.x << ", " <<
            //     pos.y
            //                << ", " << pos.z + 1 << ")";
            //     std::cout << ossMessage.str() << std::endl;
            // }
            // }
        }
    } else if (pos.z < maxAltitude) {
        std::cout << "[moveVaporUp] REACHED THE CONTROL POINT\n";
    }

    voxelGrid.terrainGridRepository->unlockTerrainGrid();
}

void moveVaporSideways(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                       Position& pos, EntityTypeComponent& type,
                       MatterContainer& matterContainer) {
    int terrainId = voxelGrid.getTerrain(pos.x, pos.y, pos.z);

    float rhoEnv = 1.225f;    // Density of air
    float rhoVapor = 0.597f;  // Density of water vapor

    // Vapor has reached max altitude; move sideways

    std::ostringstream ossMessage;
    ossMessage << "[moveVaporSideways] Vapor moving sideways at max altitude from (" << pos.x << ", " << pos.y << ", "
               << pos.z << ")";
    spdlog::get("console")->debug(ossMessage.str());
    ossMessage.str("");
    ossMessage.clear();

    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dis(1, 4);
    int direction = dis(gen);

    int dx = 0, dy = 0;
    if (direction == 1)
        dx = 1;
    else if (direction == 2)
        dx = -1;
    else if (direction == 3)
        dy = 1;
    else
        dy = -1;

    float forceX = dx * 500;
    float forceY = dy * 500;
    int newX = pos.x + dx;
    int newY = pos.y + dy;

    bool haveMovement = voxelGrid.terrainGridRepository->hasMovingComponent(pos.x, pos.y, pos.z);

    int terrainSideId = voxelGrid.getTerrain(newX, newY, pos.z);
    if (!haveMovement && terrainSideId == static_cast<int>(TerrainIdTypeEnum::NONE)) {
        ossMessage << "[moveVaporSideways] Vapor moving to (" << forceX << ", " << forceY << ", " << pos.z << ")\n";
        spdlog::get("console")->debug(ossMessage.str());
        ossMessage.str("");
        ossMessage.clear();
        // Update position in the registry and voxel grid

        if (terrainId == static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE)) {
            entt::entity newTerrainEntity = registry.create();
            terrainId = static_cast<int>(newTerrainEntity);
        }
        entt::entity entity = static_cast<entt::entity>(terrainId);
        MoveGasEntityEvent moveGasEntityEvent{
            entity, Position{pos.x, pos.y, pos.z, DirectionEnum::DOWN}, 0.0f, 0.0f, rhoEnv, rhoVapor};
        moveGasEntityEvent.setForceApplyNewVelocity();

        dispatcher.enqueue<MoveGasEntityEvent>(moveGasEntityEvent);
    } else {
        auto terrainSide = static_cast<entt::entity>(terrainSideId);
        // checkAndConvertSoftEmptyIntoVapor(registry, terrainSide);
        if (registry.all_of<EntityTypeComponent, MatterContainer>(
                static_cast<entt::entity>(terrainSideId))) {
            ossMessage << "[moveVaporSideways] Vapor cannot move sideways; obstruction at (" << newX << ", " << newY
                       << ", " << pos.z << ")\n";
            spdlog::get("console")->debug(ossMessage.str());
            ossMessage.str("");
            ossMessage.clear();

            // Merge with vapor if possible
            auto&& [typeSide, matterContainerSide] =
                registry.get<EntityTypeComponent, MatterContainer>(
                    static_cast<entt::entity>(terrainSideId));
            bool haveMovement = voxelGrid.terrainGridRepository->hasMovingComponent(pos.x, pos.y, pos.z);

            if (!haveMovement &&
                (typeSide.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                 typeSide.subType0 == static_cast<int>(TerrainEnum::WATER)) &&
                matterContainerSide.WaterVapor >= 0 && matterContainerSide.WaterMatter == 0) {
                ossMessage << "[moveVaporSideways] Vapor merging with vapor at (" << newX << ", " << newY << ", "
                           << pos.z << ")\n";
                spdlog::get("console")->debug(ossMessage.str());
                ossMessage.str("");
                ossMessage.clear();

                // Merge vapor
                matterContainerSide.WaterVapor += matterContainer.WaterVapor;
                matterContainer.WaterVapor = 0;

                if (terrainId != static_cast<int>(TerrainIdTypeEnum::ON_GRID_STORAGE) && terrainId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
                    entt::entity entity = static_cast<entt::entity>(terrainId);
                    deleteEntityOrConvertInEmpty(registry, dispatcher, entity);
                }
            } else {
                ossMessage << "[moveVaporSideways] Vapor Obstructed; cannot move sideways (" << newX << ", " << newY
                           << ", " << pos.z << ")\n";
                spdlog::get("console")->debug(ossMessage.str());
                ossMessage.str("");
                ossMessage.clear();
                // Obstructed; cannot move sideways
                // Optionally handle other directions or stay in place
            }
        }
    }
}

void moveVapor(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher, int x,
               int y, int z, Position& pos, EntityTypeComponent& type,
               MatterContainer& matterContainer) {
    int maxAltitude = voxelGrid.depth - 1;  // Example maximum altitude for vapor to rise

    // Condensation Logic for Vapor
    const int condensationThreshold = 21;
    if (matterContainer.WaterVapor >= condensationThreshold) {
        // TODO: Uncomment when ready.
        // condenseVapor(registry, voxelGrid, dispatcher, entity, pos, type, matterContainer,
        //               pendingCondenseWater);
        return;  // Condensation happened, exit the function
    }

    std::ostringstream ossMessage;
    // if (entity == entityBeingDebugged) {
    //     ossMessage << "Move Vapor maxAltitude(" << maxAltitude << ")";
    //     std::cout << ossMessage.str() << std::endl;
    // }

    int terrainAboveId = voxelGrid.getTerrain(pos.x, pos.y, pos.z + 1);

    bool isTerrainAboveVaporOrEmpty = false;

    bool isTerrainAboveEmpty = false;
    bool isTerrainAboveVapor = false;
    if (terrainAboveId != static_cast<int>(TerrainIdTypeEnum::NONE)) {
        auto terrainAbove = static_cast<entt::entity>(terrainAboveId);
        // checkAndConvertSoftEmptyIntoVapor(registry, terrainAbove);
        if (voxelGrid.terrainGridRepository->isTerrainIdOnEnttRegistry(terrainAboveId)) {
            // std::cout << "[moveVapor] Error: Terrain above is ON_ENTT at (" << pos.x << ", "
            //           << pos.y << ", " << pos.z + 1 << ") --- NOT IMPLEMENTED\n";
            // throw std::runtime_error("Not implemented");

            EntityTypeComponent typeAbove =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z + 1);
            MatterContainer matterContainerAbove =
                voxelGrid.terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z + 1);

            bool haveMovement = voxelGrid.terrainGridRepository->hasMovingComponent(pos.x, pos.y, pos.z + 1);

            bool isWater = (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                            typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER));
            bool isTerrainAboveVapor =
                isWater && (matterContainer.WaterVapor >= 0 && matterContainer.WaterMatter == 0);
        } else {
            EntityTypeComponent typeAbove =
                voxelGrid.terrainGridRepository->getTerrainEntityType(pos.x, pos.y, pos.z + 1);
            MatterContainer matterContainerAbove =
                voxelGrid.terrainGridRepository->getTerrainMatterContainer(pos.x, pos.y, pos.z + 1);

            // if it is on grid storage, it means it is not moving.
            bool haveMovement = false;

            bool isWater = (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                            typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER));
            bool isTerrainAboveVapor =
                isWater && (matterContainer.WaterVapor >= 0 && matterContainer.WaterMatter == 0);
        }
        // entt::entity terrainAboveEntity = static_cast<entt::entity>(terrainAboveId);
        // if (registry.all_of<EntityTypeComponent, MatterContainer>(terrainAboveEntity)) {
        //     // Merge with vapor above if possible
        //     auto&& [typeAbove, matterContainerAbove] =
        //         registry.get<EntityTypeComponent, MatterContainer>(terrainAboveEntity);
        //     bool haveMovement = registry.all_of<MovingComponent>(terrainAboveEntity);

        //     bool isWater = (typeAbove.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
        //                     typeAbove.subType0 == static_cast<int>(TerrainEnum::WATER));
        //     bool isTerrainAboveVapor =
        //         isWater && (matterContainer.WaterVapor >= 0 && matterContainer.WaterMatter == 0);
        // }
    } else {
        isTerrainAboveEmpty = true;
    }

    isTerrainAboveVaporOrEmpty = isTerrainAboveEmpty || isTerrainAboveVapor;

    if (pos.z < maxAltitude && isTerrainAboveVaporOrEmpty) {
        // Move vapor up
        moveVaporUp(registry, voxelGrid, dispatcher, pos, type, matterContainer);
    } else {
        // Vapor has reached max altitude; move sideways
        // TODO: Uncomment when ready.
        // moveVaporSideways(registry, voxelGrid, dispatcher, pos, type, matterContainer);
    }
}

void processTileWater(int x, int y, int z, entt::registry& registry, VoxelGrid& voxelGrid,
                      entt::dispatcher& dispatcher, float sunIntensity,
                      std::random_device& rd, std::mt19937& gen,
                      std::uniform_int_distribution<>& disWaterSpreading) {
    bool terrainExists = voxelGrid.checkIfTerrainExists(x, y, z);

    if (!terrainExists) {
        // std::cout << "[processTileWater] No terrain exists at (" << x << ", " << y << ", " << z
        //           << ")\n";
        return;
    }

    int terrainId = voxelGrid.getTerrain(x, y, z);

    if (terrainId != -2) {  // Assuming -2 means no terrain
        DirectionEnum terrainDirection = voxelGrid.terrainGridRepository->getDirection(x, y, z);
        Position pos = {x, y, z, terrainDirection};
        EntityTypeComponent type = voxelGrid.getTerrainEntityTypeComponent(x, y, z);
        MatterContainer matterContainer =
            voxelGrid.terrainGridRepository->getTerrainMatterContainer(x, y, z);

        auto entity_id_for_print = terrainId;
        // Flag to indicate whether an action has been performed
        bool actionPerformed = false;

        const bool isGrass = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                              type.subType0 == static_cast<int>(TerrainEnum::GRASS));

        const bool isWater = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                              type.subType0 == static_cast<int>(TerrainEnum::WATER));

        const bool isEmptyTerrain = (type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                                     type.subType0 == static_cast<int>(TerrainEnum::EMPTY));

        // Determine if the entity is vapor or liquid water
        bool isVapor =
            isWater && (matterContainer.WaterVapor > 0 && matterContainer.WaterMatter == 0);
        bool isLiquidWater =
            isWater && (matterContainer.WaterMatter > 0 && matterContainer.WaterVapor == 0);
        bool isGrassWithWater =
            isGrass && (matterContainer.WaterMatter > 0 && matterContainer.WaterVapor == 0);

        bool emptyWater =
            isWater && (matterContainer.WaterMatter == 0 && matterContainer.WaterVapor == 0);

        bool emptyWithoutWater =
            isEmptyTerrain && (matterContainer.WaterMatter == 0 && matterContainer.WaterVapor == 0);

        // Vapor Movement Logic
        // TODO: Uncomment vapor logic when ready.
        if (isVapor) {
            // TODO: This needs to be removed for performance reasons.
            // It is only here to guarantee the bugfix to set vapor into GAS.
            setVaporSI(pos.x, pos.y, pos.z, voxelGrid);
            moveVapor(registry, voxelGrid, dispatcher, pos.x, pos.y, pos.z, pos, type,
                      matterContainer);
            actionPerformed = true;
            return;  // Vapor entities don't perform other actions
        }

        // Randomized Action Order for Liquid Water
        if (isLiquidWater || isGrassWithWater) {
            // std::cout << "[processTileWater] Randomized action order for liquid water ("
            //           << "isLiquidWater: " << isLiquidWater << ", isGrassWithWater: " <<
            //           isGrassWithWater << ")"
            //           << " actionPerformed: " << actionPerformed << " -- Entity "
            //           << entity_id_for_print << " at (" << x << ", " << y << ", " << z << ")\n";
            // Create a list of action identifiers
            std::vector<int> actions = {1, 2};  // 1: Movement, 2: Evaporation

            // Shuffle the actions to randomize their order
            std::shuffle(actions.begin(), actions.end(), gen);

            // Iterate through the actions in random order
            for (int action : actions) {
                if (actionPerformed) {
                    break;
                }

                switch (action) {
                    case 1:  // Water Movement Logic
                    {
                        actionPerformed = moveWater(terrainId, registry, voxelGrid, dispatcher,
                                                    actionPerformed, pos, type, matterContainer, rd,
                                                    gen, disWaterSpreading);
                    } break;

                    // case 2:  // Water Evaporation Logic
                    // {
                    //     // std::cout << "[processTileWater] Checking evaporation for entity "
                    //     //           << entity_id_for_print << " at (" << x << ", " << y << ", "
                    //     <<
                    //     //           z
                    //     //           << ") -- This is just fake for testing now\n";
                    // } break;

                    // TODO: Remove comment when ready.
                    case 2:  // Water Evaporation Logic
                        bool canEvaporate =
                            (sunIntensity > 0.0f &&
                             type.mainType == static_cast<int>(EntityEnum::TERRAIN) &&
                             (type.subType0 == static_cast<int>(TerrainEnum::WATER) ||
                              type.subType0 == static_cast<int>(TerrainEnum::GRASS)) &&
                             matterContainer.WaterMatter > 0);
                        if (canEvaporate) {
                            PhysicsStats physicsStats =
                                voxelGrid.terrainGridRepository->getPhysicsStats(x, y, z);

                            // TODO: Maybe I need to ensure that all water entities have
                            // PhysicsStats ??? if (!registry.all_of<PhysicsStats>(entity)) {
                            //     PhysicsStats newPhysicsStats = {};
                            //     newPhysicsStats.mass = 0.1;
                            //     newPhysicsStats.maxSpeed = 10;
                            //     newPhysicsStats.minSpeed = 0.0;
                            //     newPhysicsStats.heat = 0.0f;
                            //     registry.emplace<PhysicsStats>(entity, newPhysicsStats);
                            // }

                            // auto& physicsStats = registry.get<PhysicsStats>(entity);

                            float EVAPORATION_COEFFICIENT =
                                PhysicsManager::Instance()->getEvaporationCoefficient();
                            const float HEAT_TO_WATER_EVAPORATION =
                                PhysicsManager::Instance()->getHeatToWaterEvaporation();
                            float heat = EVAPORATION_COEFFICIENT * sunIntensity;

                            physicsStats.heat += heat;
                            // std::cout << "Water heat: " << physicsStats.heat
                            //           << " (Heat needed to evaporate: " <<
                            //           HEAT_TO_WATER_EVAPORATION << ")"
                            //           << std::endl;
                            if (physicsStats.heat > HEAT_TO_WATER_EVAPORATION) {
                                EvaporateWaterEntityEvent evaporateWaterEntityEvent{entt::null, pos,
                                                                                    sunIntensity};
                                // Dispatch directly to physics system
                                dispatcher.enqueue<EvaporateWaterEntityEvent>(evaporateWaterEntityEvent);
                                // std::cout << "Water evaporating at (" << x << ", " << y << ", "
                                // << z
                                //           << "), with heat: " << physicsStats.heat << "\n";
                                physicsStats.heat = 0.0f;
                            }

                            voxelGrid.terrainGridRepository->setPhysicsStats(x, y, z, physicsStats);

                            actionPerformed = true;
                        }
                        break;
                }
            }
        }

        if (emptyWater || emptyWithoutWater) {
            throw std::runtime_error("Error: Water entity with no water detected.");
            // deleteEntityOrConvertInEmpty(registry, dispatcher, entity);
        }

        // Ensure that both WaterMatter and WaterVapor cannot coexist
        if (isWater && matterContainer.WaterMatter > 0 && matterContainer.WaterVapor > 0) {
            // This should not happen; adjust accordingly
            std::cerr << "Error: Entity has both WaterMatter and WaterVapor\n";
        }
    }
}

/*****************************
 *
 * High level public methods *
 *
 * ***************************
 */

void EcosystemEngine::loopTiles(entt::registry& registry, VoxelGrid& voxelGrid,
                                entt::dispatcher& dispatcher, float sunIntensity) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> disWaterSpreading(1, 4);

    int count = 0;
    int waterUnits = 0;

    auto matter_container_view = registry.view<MatterContainer>();

    // Copy entities into a vector
    std::vector<entt::entity> entities(matter_container_view.begin(), matter_container_view.end());

    // Shuffle the vector
    std::shuffle(entities.begin(), entities.end(),
                 std::default_random_engine{std::random_device{}()});

    for (auto entity : entities) {
        // processTileWater(entity, registry, voxelGrid, dispatcher, sunIntensity,
        //                  pendingEvaporateWater, pendingCondenseWater, pendingWaterFall, rd, gen,
        //                  disWaterSpreading, entityBeingDebugged);

        auto& matterContainer = matter_container_view.get<MatterContainer>(entity);
        waterUnits += matterContainer.WaterMatter;
        waterUnits += matterContainer.WaterVapor;

        count++;
        // Pause execution after every 2000 entities processed
        if (count >= 2'000) {
            count = 0;  // Reset the counter
            std::this_thread::sleep_for(
                std::chrono::milliseconds(10));  // Sleep for 10 milliseconds
        }
    }

    // std::cout << "Total water units: " << waterUnits << std::endl;
    const int waterMinimumUnits = PhysicsManager::Instance()->getWaterMinimumUnits();
    int x, y, z;
    if (waterUnits < waterMinimumUnits) {
        int waterToCreate = waterMinimumUnits - waterUnits;

        int vaporUnits = 0;
        while (waterToCreate > 0) {
            // Create x value from voxelGrid.width and y value from voxelGrid.height (randomly from
            // 0 to voxelGrid.width and height)

            vaporUnits = 0;
            if (waterToCreate > 10) {
                vaporUnits = 10;
            } else {
                vaporUnits = waterToCreate;
            }

            std::uniform_int_distribution<> disX(0, voxelGrid.width - 1);
            std::uniform_int_distribution<> disY(0, voxelGrid.height - 1);
            x = disX(gen);
            y = disY(gen);
            z = voxelGrid.depth - 1;
            createOrAddVapor(registry, voxelGrid, x, y, z, vaporUnits);
            waterToCreate -= vaporUnits;
        }
    }
}

void processPlants(entt::registry& registry, VoxelGrid& voxelGrid, entt::dispatcher& dispatcher,
                   GameClock& clock) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_int_distribution<> disFruitGrowth(1, 6);
    std::uniform_int_distribution<> disWaterSpreading(1, 4);

    auto plantResourcesView = registry.view<PlantResources, HealthComponent>();
    auto fruitGrowthView = registry.view<FruitGrowth>();
    auto inventoryView = registry.view<EntityTypeComponent, Inventory>();

    const float WATER_FOR_PRODUCE_ENERGY = 0.1;
    const float PHOTOSYNTHESIS_BASE_RATE = 6;
    const float sunIntensity = SunIntensity::getIntensity(clock);

    for (auto entity : plantResourcesView) {
        if (!registry.valid(entity)) {
            continue;
        }

        auto& plantResources = plantResourcesView.get<PlantResources>(entity);
        auto& health = plantResourcesView.get<HealthComponent>(entity);

        const float healthPercent = health.healthLevel / health.maxHealth;

        if (plantResources.water >= WATER_FOR_PRODUCE_ENERGY && sunIntensity > 0) {
            plantResources.water -= WATER_FOR_PRODUCE_ENERGY;
            const float energyProduced = PHOTOSYNTHESIS_BASE_RATE * sunIntensity * healthPercent;
            plantResources.currentEnergy += energyProduced;
            // std::cout << "Plant produced " << energyProduced << " amounts of energy\n";
        }

        // TODO: Evaluate if it is better to have another async system for processing plants
        if (registry.all_of<FruitGrowth, EntityTypeComponent, Inventory>(entity)) {
            auto& fruitGrowth = fruitGrowthView.get<FruitGrowth>(entity);

            int dice_throw = disFruitGrowth(gen);
            if (
                // dice_throw > 5 &&
                fruitGrowth.currentEnergy < fruitGrowth.energyNeeded &&
                plantResources.currentEnergy > 1.0) {
                plantResources.currentEnergy -= 1.0;
                fruitGrowth.currentEnergy++;
            }

            auto&& [type, inventory] = inventoryView.get<EntityTypeComponent, Inventory>(entity);
            if (type.mainType == 1 && type.subType0 == 1 &&
                inventory.itemIDs.size() < static_cast<size_t>(inventory.maxItems) &&
                fruitGrowth.currentEnergy >= fruitGrowth.energyNeeded) {
                auto raspberryFruit = registry.create();

                registry.emplace<ItemTypeComponent>(
                    raspberryFruit,
                    ItemTypeComponent{static_cast<int>(ItemEnum::FOOD),
                                      static_cast<int>(ItemFoodEnum::RASPBERRY_FRUIT)});
                registry.emplace<FoodItem>(raspberryFruit, FoodItem{.energyDensity = 0.1,
                                                                    .mass = 60,
                                                                    .volume = 20,
                                                                    .energyHealthRatio = 0.3,
                                                                    .convertionEfficiency = 0.3});

                auto entityId = entt::to_integral(raspberryFruit);
                inventory.itemIDs.push_back(entityId);

                fruitGrowth.currentEnergy = 0;
            }

            int health_dice_throw = disFruitGrowth(gen);
            if (health_dice_throw > 5 && plantResources.currentEnergy > 1.0 &&
                health.healthLevel < health.maxHealth) {
                plantResources.currentEnergy -= 1.0;

                health.healthLevel++;
                if (health.healthLevel > health.maxHealth) {
                    health.healthLevel = health.maxHealth;
                }
            }
        }
    }
}

void EcosystemEngine::processEcosystem(entt::registry& registry, VoxelGrid& voxelGrid,
                                       entt::dispatcher& dispatcher, GameClock& clock) {
    // std::cout << "Processing ecosystem\n";

    float sunIntensity = SunIntensity::getIntensity(clock);
    processPlants(registry, voxelGrid, dispatcher, clock);
}

void EcosystemEngine::processEcosystemAsync(entt::registry& registry, VoxelGrid& voxelGrid,
                                            entt::dispatcher& dispatcher, GameClock& clock) {
    // std::cout << "Processing ecosystem Async\n";
    std::scoped_lock lock(ecosystemMutex);  // Ensure exclusive access

    float sunIntensity = SunIntensity::getIntensity(clock);
    processingComplete = false;

    std::vector<ToBeCreatedWaterTile> pendingEntities;

    // loopTiles(registry, voxelGrid, dispatcher, sunIntensity);

    // std::cout << "[processEcosystemAsync] Before water simulation\n";

    waterSimManager_->processWaterSimulation(registry, voxelGrid, sunIntensity);

    processingComplete = true;
}

bool EcosystemEngine::isProcessingComplete() const { return processingComplete; }

// NOTE: Water event processing methods have been moved to PhysicsEngine
// for better separation of concerns. The ecosystem engine now only detects
// conditions and dispatches events. PhysicsEngine handles all state changes.

void EcosystemEngine::onSetEcoEntityToDebug(const SetEcoEntityToDebug& event) {
    entityBeingDebugged = event.entity;
}

// Register event handlers
void EcosystemEngine::registerEventHandlers(entt::dispatcher& dispatcher) {
    dispatcher.sink<SetEcoEntityToDebug>().connect<&EcosystemEngine::onSetEcoEntityToDebug>(*this);
}

// void EcosystemEngine::processParallelWaterSimulation(entt::registry& registry,
//                                                      VoxelGrid& voxelGrid) {
//     if (!waterSimManager_) {
//         waterSimManager_ = std::make_unique<WaterSimulationManager>();
//     }

//     // Execute parallel water simulation
//     waterSimManager_->processWaterSimulation(registry, voxelGrid);
// }