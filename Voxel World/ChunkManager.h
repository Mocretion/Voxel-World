#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Vertex.h"
#include "PerlinNoise.h"
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

// Forward declaration to avoid circular dependency
class Chunk;

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager();

    // Main interface methods
    void markChunksForGeneration(std::vector<std::pair<int, int>> chunks);
    void markChunksForMeshing(std::vector<std::pair<int, int>> chunks);
    void generateChunks(int maxUpdates);
    void updateMeshes();
    int removeDistantChunks(int centerX, int centerZ, int maxDataDistance, int maxMeshDistance);

    // Query methods
    Chunk* getChunk(int chunkX, int chunkZ);
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ) const;
    bool hasCubeAtWorldCached(int worldX, int worldY, int worldZ,
        const std::unordered_map<uint64_t, const Chunk*>& chunkCache, bool debug) const;
    std::unordered_map<uint64_t, const Chunk*> createNeighborCache(int centerChunkX, int centerChunkZ) const;

    std::vector<Vertex> getAllVertices() const;
    std::vector<uint32_t> getAllIndices() const;
    int getLoadedChunkCount() const;
    std::vector<std::pair<int, int>> getLoadedChunkCoordinates() const;

    // Status methods
    int getChunksNeedingGeneration() const;
    int getChunksNeedingMeshUpdate() const;
    bool getIsDoneGenerating() const { return isDoneGenerating.load(); }
    bool getIsDoneMeshing() const { return isDoneMeshing.load(); }

    // Public flags for optimization
    std::atomic<bool> usedNewGenerationValue{ true };
    std::atomic<bool> usedNewMeshingValue{ true };

    // Public mutex for external access (kept for compatibility)
    mutable std::shared_mutex chunkMutex;

private:
    // OPTIMIZATION 1: Memory pool for better cache locality
    struct ChunkPool {
        std::vector<std::unique_ptr<Chunk>> chunks;  // Use unique_ptr for stable addresses
        std::vector<bool> allocated;                 // Track which slots are used
        std::vector<size_t> freeSlots;              // Available slots for reuse
        std::atomic<size_t> nextSlot{ 0 };            // Thread-safe slot allocation
        mutable std::mutex poolMutex;               // Protect pool operations

        size_t allocateChunk(int chunkX, int chunkZ, PerlinNoise* noise);
        void deallocateChunk(size_t index);
        Chunk* getChunk(size_t index);
        const Chunk* getChunk(size_t index) const;
        void clear();
    };

    ChunkPool chunkPool;
    std::unordered_map<uint64_t, size_t> chunkKeyToPoolIndex;
    mutable std::shared_mutex poolIndexMutex;  // Protect the key->index mapping

    // OPTIMIZATION 2: Lock-free work distribution
    struct WorkQueue {
        std::vector<uint64_t> items;
        std::atomic<size_t> head{ 0 };
        std::atomic<size_t> tail{ 0 };
        mutable std::mutex queueMutex;  // Protect vector operations

        void push(uint64_t item);
        bool pop(uint64_t& item);
        bool empty() const;
        size_t size() const;
        void clear();
        bool contains(uint64_t item) const;
        bool remove(uint64_t item);
    };

    WorkQueue generationQueue;
    WorkQueue meshingQueue;

    // OPTIMIZATION 3: Thread-local work batches
    struct ThreadWorkBatch {
        std::vector<size_t> chunkIndices;           // Indices into chunk pool
        std::vector<std::pair<int, int>> coordPairs; // Chunk coordinates for spatial locality

        void clear()
        {
            chunkIndices.clear();
            coordPairs.clear();
        }

        bool emptyCoords() const
        {
            return coordPairs.empty();
        }

        bool empty() const
        {
            return chunkIndices.empty();
        }

        size_t size() const
        {
            return chunkIndices.size();
        }
    };

    // OPTIMIZATION 4: Minimize false sharing with proper alignment
    alignas(64) std::atomic<bool> isDoneGenerating{ true };
    alignas(64) std::atomic<bool> isDoneMeshing{ true };
    alignas(64) std::atomic<int> activeGenerationThreads{ 0 };
    alignas(64) std::atomic<int> activeMeshingThreads{ 0 };

    // Configuration - optimized for better cache performance
    static constexpr int GENERATION_THREAD_COUNT = 4;  // Reduced from multiple threads
    static constexpr int MESHING_THREAD_COUNT = 1;     // Reduced from 4 to minimize cache conflicts
    static constexpr size_t CHUNKS_PER_BATCH = 4;      // Process chunks in small batches
    static constexpr size_t INITIAL_POOL_SIZE = 128;   // Pre-allocate pool memory

    PerlinNoise noiseGenerator;

    // Thread management
    std::vector<std::thread> generationWorkers;
    std::vector<std::thread> meshingWorkers;
    std::atomic<bool> shutdownRequested{ false };

    // Helper methods
    uint64_t chunkCoordsToKey(int chunkX, int chunkZ) const;
    std::pair<int, int> keyToChunkCoords(uint64_t key) const;
    void worldToChunkAndLocal(int worldX, int worldY, int worldZ,
        int& chunkX, int& chunkZ,
        int& localX, int& localY, int& localZ) const;

    // Single chunk operations
    bool markChunkForGeneration(int chunkX, int chunkZ);
    bool markChunkForMeshing(int chunkX, int chunkZ);

    // Optimized worker methods
    void generateChunksWorker();
    void meshChunksWorker(int threadId);
    void processMeshBatch(ThreadWorkBatch& batch, int threadId);
    void processGenerationBatch(ThreadWorkBatch& batch);

    // Spatial locality helpers
    void sortChunksByDistance(std::vector<std::pair<int, int>>& chunks, int centerX, int centerZ);
    int calculateChunkDistance(int x1, int z1, int x2, int z2) const;
};