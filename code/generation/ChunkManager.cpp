#include "ChunkManager.h"
#include "Chunk.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <omp.h>
#include <queue>
#include <numeric>  // For std::iota
#include <FastNoise\FastNoise.h>

ChunkManager::ChunkManager()
{
    // Pre-allocate chunk pool for better memory locality
    chunkPool.chunks.reserve(INITIAL_POOL_SIZE);
    chunkPool.allocated.reserve(INITIAL_POOL_SIZE);

    std::cout << "ChunkManager initialized with optimized memory pool ("
        << INITIAL_POOL_SIZE << " slots)" << std::endl;
    std::cout << "Using " << GENERATION_THREAD_COUNT << " generation threads and "
        << MESHING_THREAD_COUNT << " meshing threads" << std::endl;
}

ChunkManager::~ChunkManager()
{
    // Signal shutdown to all worker threads
    shutdownRequested.store(true);

    // Wait for all workers to finish
    for (auto& worker : generationWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    for (auto& worker : meshingWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    chunkPool.clear();
    std::cout << "ChunkManager destroyed" << std::endl;
}

// ============================================================================
// CHUNK POOL IMPLEMENTATION (SIMPLIFIED)
// ============================================================================

size_t ChunkManager::ChunkPool::allocateChunk(int chunkX, int chunkY, int chunkZ)
{
    std::lock_guard<std::mutex> lock(poolMutex);

    size_t index;

    if (!freeSlots.empty())
    {
        // Reuse a free slot for better memory efficiency
        index = freeSlots.back();
        freeSlots.pop_back();

        // Create new chunk in the existing slot
        chunks[index] = std::make_unique<Chunk>(chunkX, chunkY, chunkZ);
        allocated[index] = true;
    }
    else
    {
        // Allocate new slot
        index = nextSlot.fetch_add(1);

        // Grow the pool if needed
        if (index >= chunks.size())
        {
            size_t newSize = chunks.empty() ? 64 : chunks.size() * 2;
            chunks.resize(newSize);
            allocated.resize(newSize, false);
        }

        chunks[index] = std::make_unique<Chunk>(chunkX, chunkY, chunkZ);
        allocated[index] = true;
    }

    return index;
}

void ChunkManager::ChunkPool::processPendingDeallocations()
{
    std::lock_guard<std::mutex> lock(deallocationMutex);

    size_t processed = 0;

    while (!pendingDeallocations.empty() && processed < MAX_DEALLOCATIONS_PER_FRAME)
    {
        const auto& pending = pendingDeallocations.front();

        // Actually deallocate the chunk (expensive operation)
        {
            std::lock_guard<std::mutex> poolLock(poolMutex);
            if (pending < chunks.size())
            {
                chunks[pending].reset(); // THE EXPENSIVE OPERATION
                allocated[pending] = false;
                freeSlots.push_back(pending);
            }
        }

        pendingDeallocations.pop();
        processed++;
    }
}

Chunk* ChunkManager::ChunkPool::getChunk(size_t index)
{
    if (index < chunks.size() && allocated[index])
    {
        return chunks[index].get();
    }
    return nullptr;
}

const Chunk* ChunkManager::ChunkPool::getChunk(size_t index) const
{
    if (index < chunks.size() && allocated[index])
    {
        return chunks[index].get();
    }
    return nullptr;
}

void ChunkManager::ChunkPool::clear()
{
    std::lock_guard<std::mutex> lock(poolMutex);
    chunks.clear();
    allocated.clear();
    freeSlots.clear();
    nextSlot.store(0);
}

// ============================================================================
// WORK QUEUE IMPLEMENTATION (SIMPLIFIED)
// ============================================================================

void ChunkManager::WorkQueue::push(std::tuple<int, int, int> item)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    items.push_back(item);
}

bool ChunkManager::WorkQueue::pop(std::tuple<int, int, int>& item)
{
    std::lock_guard<std::mutex> lock(queueMutex);

    if (items.empty())
    {
        return false;
    }

    item = items.back();
    items.pop_back();
    return true;
}

bool ChunkManager::WorkQueue::empty() const
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return items.empty();
}

size_t ChunkManager::WorkQueue::size() const
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return items.size();
}

void ChunkManager::WorkQueue::clear()
{
    std::lock_guard<std::mutex> lock(queueMutex);
    items.clear();
}

bool ChunkManager::WorkQueue::contains(std::tuple<int, int, int> item) const
{
    std::lock_guard<std::mutex> lock(queueMutex);
    return std::find(items.begin(), items.end(), item) != items.end();
}

bool ChunkManager::WorkQueue::remove(std::tuple<int, int, int> item)
{
    std::lock_guard<std::mutex> lock(queueMutex);
    auto it = std::find(items.begin(), items.end(), item);
    if (it != items.end())
    {
        items.erase(it);
        return true;
    }
    return false;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void ChunkManager::worldToChunkAndLocal(int worldX, int worldY, int worldZ,
    int& chunkX, int& chunkY, int& chunkZ,
    int& localX, int& localY, int& localZ) const
{
    chunkX = worldX < 0 ? (worldX - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldX / Chunk::CHUNK_SIZE;
    chunkY = worldY < 0 ? (worldY - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldY / Chunk::CHUNK_SIZE;
    chunkZ = worldZ < 0 ? (worldZ - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldZ / Chunk::CHUNK_SIZE;

    localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    localY = worldY - chunkY * Chunk::CHUNK_SIZE;
    localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;

    if (localX < 0) localX += Chunk::CHUNK_SIZE;
    if (localY < 0) localY += Chunk::CHUNK_SIZE;
    if (localZ < 0) localZ += Chunk::CHUNK_SIZE;
}

int ChunkManager::calculateChunkDistance(int x1, int y1, int z1, int x2, int y2, int z2) const
{
    return std::max({ std::abs(x1 - x2), std::abs(y1 - y2), std::abs(z1 - z2) });
}

void ChunkManager::sortChunksByDistance(std::vector<std::tuple<int, int, int>>& chunks, int centerX, int centerY, int centerZ)
{
    // Sort chunks by distance for better spatial locality
    std::sort(chunks.begin(), chunks.end(),
        [this, centerX, centerY, centerZ](const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
            int distA = calculateChunkDistance(std::get<0>(a), std::get<1>(a), std::get<2>(a), centerX, centerY, centerZ);
            int distB = calculateChunkDistance(std::get<0>(b), std::get<1>(b), std::get<2>(b), centerX, centerY, centerZ);
            return distA < distB;
        });
}

// ============================================================================
// CHUNK MANAGEMENT INTERFACE
// ============================================================================

void ChunkManager::markChunksForGeneration(std::vector<std::tuple<int, int, int>> chunks)
{
    for (const auto& coords : chunks)
    {
        markChunkForGeneration(std::get<0>(coords), std::get<1>(coords), std::get<2>(coords));
    }
}

bool ChunkManager::markChunkForGeneration(int chunkX, int chunkY, int chunkZ)
{
    auto key = std::make_tuple(chunkX, chunkY, chunkZ);

    // Check if chunk already exists
    {
        std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
        if (chunkKeyToPoolIndex.find(key) != chunkKeyToPoolIndex.end())
        {
            return false;  // Chunk already exists
        }
    }

    // Check if already in generation queue
    if (!generationQueue.contains(key))
    {
        generationQueue.push(key);
        return true;
    }

    return false;
}

void ChunkManager::markChunksForMeshing(std::vector<std::tuple<int, int, int>> chunks)
{
    for (const auto& coords : chunks)
    {
        markChunkForMeshing(std::get<0>(coords), std::get<1>(coords), std::get<2>(coords));
    }
}

bool ChunkManager::markChunkForMeshing(int chunkX, int chunkY, int chunkZ)
{
    std::tuple<int, int, int> key = std::make_tuple(chunkX, chunkY, chunkZ);

    if (!meshingQueue.contains(key))
    {
        meshingQueue.push(key);
        return true;
    }

    return false;
}

// ============================================================================
// GENERATION SYSTEM
// ============================================================================

void ChunkManager::generateChunks()
{
    isDoneGenerating.store(false);
    activeGenerationThreads.store(GENERATION_THREAD_COUNT);

    // Clear any existing workers
    for (auto& worker : generationWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    generationWorkers.clear();

    // Start generation workers
    for (int i = 0; i < GENERATION_THREAD_COUNT; ++i)
    {

        generationWorkers.emplace_back([this, i]() {
            generateChunksWorker(i);
        });
    }
}

void ChunkManager::processChunkDeallocations()
{
    chunkPool.processPendingDeallocations();
}

void ChunkManager::generateChunksWorker(int threadId)
{
    ThreadWorkBatch batch;

    std::cout << "[FAST] Generation worker " << threadId << " | " << std::this_thread::get_id() << " started" << std::endl;

    while (!shutdownRequested.load())
    {
        batch.clear();

        // Collect a batch of work items
        for (size_t i = 0; i < CHUNKS_PER_BATCH; ++i)
        {
            std::tuple<int, int, int> chunkCoords;
            if (!generationQueue.pop(chunkCoords))
            {
                break;  // No more work
            }

            batch.coordTuples.push_back(chunkCoords);
        }

        if (batch.emptyCoords())
        {
            break;  // No work to do
        }

        // Process the batch
        processGenerationBatch(batch, threadId);
    }

    // Mark thread as finished
    int remaining = activeGenerationThreads.fetch_sub(1);
    if (remaining == 1)
    {
        isDoneGenerating.store(true);
        usedNewGenerationValue.store(false);
    }
}

void ChunkManager::processGenerationBatch(ThreadWorkBatch& batch, int threadId)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    for (const auto& coords : batch.coordTuples)
    {
        int chunkX = std::get<0>(coords);
        int chunkY = std::get<1>(coords);
        int chunkZ = std::get<2>(coords);

        // Double-check chunk doesn't exist
        {
            std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
            if (chunkKeyToPoolIndex.find(coords) != chunkKeyToPoolIndex.end())
            {
                continue;  // Chunk was created by another thread
            }
        }

        // Allocate and generate chunk (now uses stack allocation internally)

        size_t poolIndex = chunkPool.allocateChunk(chunkX, chunkY, chunkZ);

        chunkPool.chunks[poolIndex]->generateTerrain();

        // Add to index mapping
        {
            std::unique_lock<std::shared_mutex> lock(poolIndexMutex);
            chunkKeyToPoolIndex[coords] = poolIndex;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    //std::cout << "[FAST " << threadId << "] Processed " << batch.coordTuples.size() << " chunks in "
	//	<< duration.count() << "ms using " << std::this_thread::get_id() << std::endl;
}

// ============================================================================
// MESHING SYSTEM (SIMPLIFIED)
// ============================================================================

void ChunkManager::updateMeshes()
{
    isDoneMeshing.store(false);
    activeMeshingThreads.store(MESHING_THREAD_COUNT);

    // Clear any existing workers
    for (auto& worker : meshingWorkers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    meshingWorkers.clear();

    // Start meshing workers
    for (int i = 0; i < MESHING_THREAD_COUNT; ++i)
    {
        meshingWorkers.emplace_back([this, i]() {
            meshChunksWorker(i);
            });
    }
}

void ChunkManager::meshChunksWorker(int threadId)
{
    ThreadWorkBatch batch;

    std::cout << "[FAST] Meshing worker " << threadId << " started" << std::endl;

    while (!shutdownRequested.load())
    {
        batch.clear();

        {
            std::shared_lock lock(poolIndexMutex);

            // Collect batch of work
            for (size_t i = 0; i < CHUNKS_PER_BATCH; ++i)
            {
                std::tuple<int, int, int> chunkCoords;
                if (meshingQueue.pop(chunkCoords))
                {
                    auto it = chunkKeyToPoolIndex.find(chunkCoords);
                    if (it != chunkKeyToPoolIndex.end())
                    {
                        batch.chunkIndices.push_back(it->second);
                        batch.coordTuples.push_back(chunkCoords);
                    }
                }
            }
        }

        if (batch.empty())
        {
            break;
        }

        // Process batch with spatial locality
        processMeshBatch(batch, threadId);
    }

    // Mark thread as finished
    int remaining = activeMeshingThreads.fetch_sub(1);
    if (remaining == 1)
    {
        isDoneMeshing.store(true);
        usedNewMeshingValue.store(false);
    }
}

void ChunkManager::processMeshBatch(ThreadWorkBatch& batch, int threadId)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Thread-local neighbor cache
    thread_local NeighborCache neighborCache;

    for (size_t i = 0; i < batch.size(); ++i)
    {
        size_t chunkIndex = batch.chunkIndices[i];
        auto& coordTuple = batch.coordTuples[i];

        Chunk* chunk = chunkPool.getChunk(chunkIndex);
        if (!chunk || chunk->isMeshGenerated())
        {
            continue;
        }

        int chunkX = std::get<0>(coordTuple);
        int chunkY = std::get<1>(coordTuple);
        int chunkZ = std::get<2>(coordTuple);

        // Populate neighbor cache for this chunk
        neighborCache.populateFromManager(this, chunkX, chunkY, chunkZ);

        // Mesh using the cache - no locks during meshing!
        chunk->generateMeshWithManager(this, &neighborCache, false);

        usedNewMeshingValue.store(true);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    //std::cout << "[FAST " << threadId << "] Meshed " << batch.coordTuples.size() << " chunks in "
    //    << duration.count() << "ms using " << std::this_thread::get_id() << std::endl;
}

void ChunkManager::markChunksAsDisplayed(const std::vector<Chunk*>& chunks)
{
    for (Chunk* chunk : chunks)
    {
        if (chunk)
        {
            chunk->markAsDisplayed();
        }
    }
}

// ============================================================================
// CHUNK REMOVAL AND CLEANUP
// ============================================================================

int ChunkManager::removeDistantChunks(int centerX, int centerY, int centerZ, int maxDataDistance, int maxMeshDistance, BgfxRenderer& renderer)
{
    std::vector<std::tuple<int, int, int>> chunkMeshToRemove;
    std::vector<std::tuple<int, int, int>> chunkDataToRemove;

    // Find chunks to remove
    {
        std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
        for (const auto& pair : chunkKeyToPoolIndex)
        {
            auto& coords = pair.first;
            const Chunk* chunk = chunkPool.getChunk(pair.second);

            if (!chunk) continue;

            int chunkX = std::get<0>(coords);
            int chunkY = std::get<1>(coords);
            int chunkZ = std::get<2>(coords);

            int distance = calculateChunkDistance(chunkX, chunkY, chunkZ, centerX, centerY, centerZ);

            if (distance > maxDataDistance)
            {
                chunkDataToRemove.push_back(pair.first);
            }
            else if (distance > maxMeshDistance)
            {
                chunkMeshToRemove.push_back(pair.first);
            }
        }
    }

    // Remove distant chunks
    {
        auto startTime = std::chrono::high_resolution_clock::now();

        std::unique_lock<std::shared_mutex> lock(poolIndexMutex);
        for (auto& key : chunkDataToRemove)
        {
            auto it = chunkKeyToPoolIndex.find(key);
            if (it != chunkKeyToPoolIndex.end())
            {
                // Clean up bgfx resources before deallocating

                Chunk* chunk = chunkPool.getChunk(it->second);
                if (chunk)
                {
                    chunk->cleanupFromBufferPool(renderer.getBufferPool());
                }

                chunkKeyToPoolIndex.erase(it);
                generationQueue.remove(key);
                meshingQueue.remove(key);

                chunkPool.pendingDeallocations.emplace(it->second);
                
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - startTime);

        std::cout << "======== [FAST] Removed chunk data in " << duration << std::endl;

        startTime = std::chrono::high_resolution_clock::now();

        for (auto& key : chunkMeshToRemove)
        {
            auto it = chunkKeyToPoolIndex.find(key);
            if (it != chunkKeyToPoolIndex.end())
            {
                Chunk* chunk = chunkPool.getChunk(it->second);
                if (chunk)
                {
                    // Clean up bgfx resources before clearing mesh
                    chunk->cleanupFromBufferPool(renderer.getBufferPool());  // TODO: pass renderer to this function
                    chunk->clearMesh();
                }
                meshingQueue.remove(key);
            }
        }
    }

    return static_cast<int>(chunkDataToRemove.size());
}

// ============================================================================
// QUERY METHODS
// ============================================================================

Chunk* ChunkManager::getChunk(int chunkX, int chunkY, int chunkZ)
{
    std::tuple<int, int, int> key = std::make_tuple(chunkX, chunkY, chunkZ);

    std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
    auto it = chunkKeyToPoolIndex.find(key);

    if (it != chunkKeyToPoolIndex.end())
    {
        return chunkPool.getChunk(it->second);
    }

    return nullptr;
}

bool ChunkManager::hasCubeAtWorld(int worldX, int worldY, int worldZ) const
{
    int chunkX, chunkY, chunkZ, localX, localY, localZ;
    worldToChunkAndLocal(worldX, worldY, worldZ, chunkX, chunkY, chunkZ, localX, localY, localZ);

    std::tuple<int, int, int> key = std::make_tuple(chunkX, chunkY, chunkZ);

    std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
    auto it = chunkKeyToPoolIndex.find(key);
    if (it == chunkKeyToPoolIndex.end())
    {
        return false;
    }

    const Chunk* chunk = chunkPool.getChunk(it->second);
    return chunk ? chunk->hasCubeAt(localX, localY, localZ) : false;
}

int ChunkManager::getLoadedChunkCount() const
{
    std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
    return static_cast<int>(chunkKeyToPoolIndex.size());
}

std::vector<std::tuple<int, int, int>> ChunkManager::getLoadedChunkCoordinates() const
{
    std::vector<std::tuple<int, int, int>> coordinates;

    std::shared_lock<std::shared_mutex> lock(poolIndexMutex);
    coordinates.reserve(chunkKeyToPoolIndex.size());

    for (const auto& pair : chunkKeyToPoolIndex)
    {
        coordinates.push_back(pair.first);
    }

    return coordinates;
}

std::vector<Chunk*> ChunkManager::getNewlyCompletedChunks()
{
    std::vector<Chunk*> completedChunks;

    std::shared_lock<std::shared_mutex> poolLock(poolIndexMutex);

    // Check all chunks for newly completed ones
    for (const auto& pair : chunkKeyToPoolIndex)
    {
        Chunk* chunk = chunkPool.getChunk(pair.second);
        if (chunk && chunk->isNewlyCompleted())
        {
            completedChunks.push_back(chunk);
        }
    }

    return completedChunks;
}

// ============================================================================
// STATUS METHODS
// ============================================================================

int ChunkManager::getChunksNeedingGeneration() const
{
    return static_cast<int>(generationQueue.size());
}

int ChunkManager::getChunksNeedingMeshUpdate() const
{
    return static_cast<int>(meshingQueue.size());
}