#include "Chunk.h"
#include "ChunkManager.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <omp.h>
#include <queue>

ChunkManager::ChunkManager() : noiseGenerator(12345)
{
    // Use a fixed seed for consistent terrain
}

uint64_t ChunkManager::chunkCoordsToKey(int chunkX, int chunkZ) const
{
    // Combine two 32-bit integers into one 64-bit key
    uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(chunkX)) << 32) |
        static_cast<uint64_t>(static_cast<uint32_t>(chunkZ));
    return key;
}

std::pair<int, int> ChunkManager::keyToChunkCoords(uint64_t key) const
{
    // Extract the upper 32 bits for chunkX
    int chunkX = static_cast<int>(static_cast<uint32_t>(key >> 32));

    // Extract the lower 32 bits for chunkZ
    int chunkZ = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));

    return std::make_pair(chunkX, chunkZ);
}

void ChunkManager::markChunksForGeneration(std::vector<std::pair<int, int>> chunks)
{
    std::unique_lock<std::mutex> lock(genMutex);  // Lock
    for (const auto& coords : chunks)
    {
        int chunkX = coords.first;
        int chunkZ = coords.second;
        markChunkForGeneration(chunkX, chunkZ);
    }
    lock.unlock();  // Manual unlock
}

bool ChunkManager::markChunkForGeneration(int chunkX, int chunkZ)
{
    uint64_t key = chunkCoordsToKey(chunkX, chunkZ);

    // If chunk doesn't exist, and if is is not already in the queue: add it to the queue for generation
    if (chunks.find(key) == chunks.end() && !chunksToGenerate.contains(key))
    {
        chunksToGenerate.push(key);
        return true;
    }

    return false;
}

void ChunkManager::markChunksForMeshing(std::vector<std::pair<int, int>> chunks)
{
    std::unique_lock<std::mutex> lock(meshMutex);  // Lock
    for (const auto& coords : chunks)
    {
        int chunkX = coords.first;
        int chunkZ = coords.second;
        markChunkForMeshing(chunkX, chunkZ);
    }
    lock.unlock();  // Manual unlock
}

bool ChunkManager::markChunkForMeshing(int chunkX, int chunkZ)
{
    uint64_t key = chunkCoordsToKey(chunkX, chunkZ);

    if (!chunksToDraw.contains(key))
    {
        chunksToDraw.push(key);
        return true;
    }

    return false;
}

void ChunkManager::generateChunks(int threads)
{
    isDoneGenerating = false;

    generationWorker = std::jthread([this]() {
        bool working = true;

        while (working)
        {
            std::unique_lock<std::mutex> mutexLock(genMutex);  // Lock for thread safety
            if (chunksToGenerate.empty())
            {
                working = false;  // No more chunks to generate
                mutexLock.unlock();  // Unlock before exiting
            }
            else
            {
                uint64_t chunkKey = chunksToGenerate.front();
                chunksToGenerate.pop();
                mutexLock.unlock();  // Unlock before generating

                auto coords = keyToChunkCoords(chunkKey);

                Chunk chunk = Chunk(coords.first, coords.second, &noiseGenerator);

                std::unique_lock<std::shared_mutex> chunkLock(chunkMutex);
                chunks.emplace(chunkKey, chunk);
                chunkLock.unlock();
            }
        }

        isDoneGenerating = true;
        usedNewGenerationValue = false;
        });
}

// OPTIMIZED: Track which chunks were removed and mark neighbors dirty
int ChunkManager::removeDistantChunks(int centerX, int centerZ, int maxDataDistance, int maxMeshDistance)
{
    std::vector<uint64_t> chunkMeshToRemove;
    std::vector<uint64_t> chunkDataToRemove;

    // Find chunks to remove
    for (const auto& pair : chunks)
    {
        const Chunk& chunk = pair.second;

        int deltaX = chunk.getChunkX() - centerX;
        int deltaZ = chunk.getChunkZ() - centerZ;
        int distance = std::max(std::abs(deltaX), std::abs(deltaZ));

        if (distance > maxDataDistance)
        {
            chunkDataToRemove.push_back(pair.first);
        }
        else if (distance > maxMeshDistance)
        {
            chunkMeshToRemove.push_back(pair.first);
        }
    }

    // Remove the distant chunks
    for (uint64_t key : chunkDataToRemove)
    {
        auto it = chunks.find(key);
        if (it != chunks.end())
        {
            /*std::cout << "Removing distant chunk data at ("
                << it->second.getChunkX() << ", "
                << it->second.getChunkZ() << ")" << std::endl;
                */
                // Remove from dirty set if it was there
            chunksToDraw.remove(key);
            chunks.erase(it);
        }
    }

    for (uint64_t key : chunkMeshToRemove)
    {
        auto it = chunks.find(key);
        if (it != chunks.end())
        {
            /*std::cout << "Removing distant chunk mesh at ("
                << it->second.getChunkX() << ", "
                << it->second.getChunkZ() << ")" << std::endl;
            */
            // Remove from dirty set if it was there
            chunksToDraw.remove(key);
            it->second.clearMesh();
        }
    }

    return static_cast<int>(chunkDataToRemove.size());
}

// Get neighboring chunk coordinates
std::vector<std::pair<int, int>> ChunkManager::getNeighborCoordinates(int chunkX, int chunkZ) const
{
    return {
        {chunkX - 1, chunkZ - 1}, {chunkX, chunkZ - 1}, {chunkX + 1, chunkZ - 1},
        {chunkX - 1, chunkZ},                           {chunkX + 1, chunkZ},
        {chunkX - 1, chunkZ + 1}, {chunkX, chunkZ + 1}, {chunkX + 1, chunkZ + 1}
    };
}

// OPTIMIZED: Only update meshes that need it
void ChunkManager::updateMeshes()
{
    threadsDoneMeshing.store(0);

    std::vector<std::vector<uint64_t>> workPartitions(MESH_THREAD_COUNT);

    // Distribute work among threads
    {
        std::unique_lock<std::mutex> lock(meshMutex);
        size_t threadIdx = 0;

        while (!chunksToDraw.empty())
        {
            uint64_t chunkKey = chunksToDraw.front();
            chunksToDraw.pop();

            // Check if chunk still exists before assigning work
            if (chunks.find(chunkKey) != chunks.end())
            {
                workPartitions[threadIdx % MESH_THREAD_COUNT].push_back(chunkKey);
                threadIdx++;
            }
        }
    }

    auto workers = std::vector<std::jthread>();
    workers.reserve(MESH_THREAD_COUNT);

    for (size_t i = 0; i < MESH_THREAD_COUNT; i++)
    {
        workers.emplace_back([this, i, chunkKeys = std::move(workPartitions[i])]() {
            std::cout << "Mesh worker " << i << " started with " << chunkKeys.size() << " chunks." << std::endl;

            // Process all assigned chunks
            for (uint64_t chunkKey : chunkKeys)
            {
                // Use shared lock to safely access chunks map
                std::shared_lock<std::shared_mutex> lock(chunkMutex);
                auto chunkIt = chunks.find(chunkKey);
                if (chunkIt != chunks.end())
                {
                    auto coords = keyToChunkCoords(chunkKey);
                    std::cout << "Mesh worker " << i << " processing chunk at ("
                        << coords.first << ", " << coords.second << ")." << std::endl;

                    // Generate mesh - this is the expensive operation
                    chunkIt->second.generateMeshWithManager(this, i == 0);
                    lock.unlock(); // Unlock before expensive mesh generation

                }
                else
                {
                    lock.unlock();
                }
            }

            // Atomic completion tracking
            int completedThreads = threadsDoneMeshing.fetch_add(1);
            std::cout << "Mesh worker " << i << " finished processing " << chunkKeys.size() << " chunks." << std::endl;

            if (completedThreads + 1 == MESH_THREAD_COUNT)
            {
                usedNewMeshingValue = false;
            }
            });
    }
}

// Get count of chunks needing mesh updates
int ChunkManager::getChunksNeedingMeshUpdate() const
{
    return static_cast<int>(chunksToDraw.size());
}

int ChunkManager::getChunksNeedingGeneration() const
{
    return static_cast<int>(chunksToGenerate.size());
}

// Keep these methods the same as before:
std::vector<Vertex> ChunkManager::getAllVertices() const
{
    std::vector<Vertex> allVertices;

    std::shared_lock<std::shared_mutex> lock(chunkDataMutex);

    for (const auto& pair : chunks)
    {
        const Chunk& chunk = pair.second;
        const auto& chunkVertices = chunk.getVertices();
        allVertices.insert(allVertices.end(), chunkVertices.begin(), chunkVertices.end());
    }

    return allVertices;
}

std::vector<uint32_t> ChunkManager::getAllIndices() const
{
    std::vector<uint32_t> allIndices;
    uint32_t vertexOffset = 0;

    std::shared_lock<std::shared_mutex> lock(chunkDataMutex);

    for (const auto& pair : chunks)
    {
        const Chunk& chunk = pair.second;
        const auto& chunkIndices = chunk.getIndices();

        for (uint32_t index : chunkIndices)
        {
            allIndices.push_back(index + vertexOffset);
        }

        vertexOffset += static_cast<uint32_t>(chunk.getVertices().size());
    }

    return allIndices;
}

Chunk* ChunkManager::getChunk(int chunkX, int chunkZ)
{
    uint64_t key = chunkCoordsToKey(chunkX, chunkZ);

    std::shared_lock<std::shared_mutex> lock(chunkDataMutex);

    auto it = chunks.find(key);

    if (it != chunks.end())
    {
        return &(it->second);
    }

    return nullptr;
}

std::vector<std::pair<int, int>> ChunkManager::getLoadedChunkCoordinates() const
{
    std::vector<std::pair<int, int>> coordinates;

    for (const auto& pair : chunks)
    {
        const Chunk& chunk = pair.second;
        coordinates.emplace_back(chunk.getChunkX(), chunk.getChunkZ());
    }

    return coordinates;
}

void ChunkManager::worldToChunkAndLocal(int worldX, int worldY, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localY, int& localZ) const
{
    // Calculate which chunk these world coordinates belong to
    // This handles negative coordinates correctly using integer division
    chunkX = worldX < 0 ? (worldX - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldX / Chunk::CHUNK_SIZE;
    chunkZ = worldZ < 0 ? (worldZ - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldZ / Chunk::CHUNK_SIZE;

    // Calculate local coordinates within the chunk
    localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    localY = worldY;  // Y coordinates are already local
    localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;

    // Ensure local coordinates are in valid range [0, CHUNK_SIZE-1]
    if (localX < 0) localX += Chunk::CHUNK_SIZE;
    if (localZ < 0) localZ += Chunk::CHUNK_SIZE;
}

bool ChunkManager::hasCubeAtWorld(int worldX, int worldY, int worldZ) const
{
    // Convert world coordinates to chunk and local coordinates
    int chunkX, chunkZ, localX, localY, localZ;
    worldToChunkAndLocal(worldX, worldY, worldZ, chunkX, chunkZ, localX, localY, localZ);

    std::shared_lock<std::shared_mutex> lock(chunkDataMutex);

    // Find the chunk
    uint64_t key = chunkCoordsToKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end())
    {
        return false;  // No chunk exists at this location
    }

    // Check if cube exists in that chunk
    return it->second.hasCubeAt(localX, localY, localZ);
}

// Create cache for a specific chunk and its neighbors
std::unordered_map<uint64_t, const Chunk*> ChunkManager::createNeighborCache(int centerChunkX, int centerChunkZ) const
{
    std::unordered_map<uint64_t, const Chunk*> cache;

    // Cache 3x3 grid of chunks around the center
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dz = -1; dz <= 1; dz++)
        {
            int neighborX = centerChunkX + dx;
            int neighborZ = centerChunkZ + dz;
            uint64_t key = chunkCoordsToKey(neighborX, neighborZ);

            auto it = chunks.find(key);
            if (it != chunks.end())
            {
                cache[key] = &it->second;
            }
        }
    }

    return cache;
}

bool ChunkManager::hasCubeAtWorldCached(int worldX, int worldY, int worldZ,
    const std::unordered_map<uint64_t, const Chunk*>& chunkCache, bool debug) const
{
    std::chrono::high_resolution_clock::time_point startTimer = std::chrono::high_resolution_clock::now();

    int chunkX, chunkZ, localX, localY, localZ;
    worldToChunkAndLocal(worldX, worldY, worldZ, chunkX, chunkZ, localX, localY, localZ);

    uint64_t key = chunkCoordsToKey(chunkX, chunkZ);
    auto it = chunkCache.find(key);
    if (it == chunkCache.end() || it->second == nullptr)
    {
        return false;
    }

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTimer);
    if (debug)
        std::cout << "Checked Cube at world in " << duration << std::endl;

    return it->second->hasCubeAt(localX, localY, localZ);
}