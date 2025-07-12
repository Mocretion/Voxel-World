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
#include <tuple>

// Forward declaration to avoid circular dependency
class Chunk;

struct TupleHash {
    std::size_t operator()(const std::tuple<int, int, int>& t) const
    {
        auto h1 = std::hash<int>{}(std::get<0>(t));
        auto h2 = std::hash<int>{}(std::get<1>(t));
        auto h3 = std::hash<int>{}(std::get<2>(t));

        // Combine hashes using a simple but effective method
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class ChunkManager {
public:
    ChunkManager();
    ~ChunkManager();

    // Main interface methods
    void markChunksForGeneration(std::vector<std::tuple<int, int, int>> chunks);
    void markChunksForMeshing(std::vector<std::tuple<int, int, int>> chunks);
    void generateChunks(int maxUpdates);
    void updateMeshes();
    int removeDistantChunks(int centerX, int centerY, int centerZ, int maxDataDistance, int maxMeshDistance);

    // Query methods
    Chunk* getChunk(int chunkX, int chunkY, int chunkZ);
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ) const;

    std::vector<Vertex> getAllVertices() const;
    std::vector<uint32_t> getAllIndices() const;
    int getLoadedChunkCount() const;
    std::vector<std::tuple<int, int, int>> getLoadedChunkCoordinates() const;

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
    // SIMPLIFIED: Memory pool for better cache locality (no TLS complexity)
    struct ChunkPool {
        std::vector<std::unique_ptr<Chunk>> chunks;  // Use unique_ptr for stable addresses
        std::vector<bool> allocated;                 // Track which slots are used
        std::vector<size_t> freeSlots;              // Available slots for reuse
        std::atomic<size_t> nextSlot{ 0 };            // Thread-safe slot allocation
        mutable std::mutex poolMutex;               // Protect pool operations

        size_t allocateChunk(int chunkX, int chunkY, int chunkZ);
        void deallocateChunk(size_t index);
        Chunk* getChunk(size_t index);
        const Chunk* getChunk(size_t index) const;
        void clear();
    };

    ChunkPool chunkPool;
    std::unordered_map<std::tuple<int, int, int>, size_t, TupleHash> chunkKeyToPoolIndex;
    mutable std::shared_mutex poolIndexMutex;  // Protect the key->index mapping

    // SIMPLIFIED: Basic work distribution
    struct WorkQueue {
        std::vector<std::tuple<int, int, int>> items;
        mutable std::mutex queueMutex;  // Protect vector operations

        void push(std::tuple<int, int, int> item);
        bool pop(std::tuple<int, int, int>& item);
        bool empty() const;
        size_t size() const;
        void clear();
        bool contains(std::tuple<int, int, int> item) const;
        bool remove(std::tuple<int, int, int> item);
    };

    WorkQueue generationQueue;
    WorkQueue meshingQueue;

    // Thread-local work batches (simplified)
    struct ThreadWorkBatch {
        std::vector<size_t> chunkIndices;           // Indices into chunk pool
        std::vector<std::tuple<int, int, int>> coordTuples; // Chunk coordinates for spatial locality

        void clear()
        {
            chunkIndices.clear();
            coordTuples.clear();
        }

        bool emptyCoords() const
        {
            return coordTuples.empty();
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

    // Thread management flags
    alignas(64) std::atomic<bool> isDoneGenerating{ true };
    alignas(64) std::atomic<bool> isDoneMeshing{ true };
    alignas(64) std::atomic<int> activeGenerationThreads{ 0 };
    alignas(64) std::atomic<int> activeMeshingThreads{ 0 };

    // Configuration - optimized for better cache performance
    static constexpr int GENERATION_THREAD_COUNT = 4;
    static constexpr int MESHING_THREAD_COUNT = 1;
    static constexpr size_t CHUNKS_PER_BATCH = 4;
    static constexpr size_t INITIAL_POOL_SIZE = 128;

    PerlinNoise noiseGenerator;

    // Thread management
    std::vector<std::thread> generationWorkers;
    std::vector<std::thread> meshingWorkers;
    std::atomic<bool> shutdownRequested{ false };

    // Helper methods
    void worldToChunkAndLocal(int worldX, int worldY, int worldZ,
        int& chunkX, int& chunkY, int& chunkZ,
        int& localX, int& localY, int& localZ) const;

    // Single chunk operations
    bool markChunkForGeneration(int chunkX, int chunkY, int chunkZ);
    bool markChunkForMeshing(int chunkX, int chunkY, int chunkZ);

    // Optimized worker methods
    void generateChunksWorker(int threadId);
    void meshChunksWorker(int threadId);
    void processMeshBatch(ThreadWorkBatch& batch, int threadId);
    void processGenerationBatch(ThreadWorkBatch& batch, int threadId);

    // Spatial locality helpers
    void sortChunksByDistance(std::vector<std::tuple<int, int, int>>& chunks, int centerX, int centerY, int centerZ);
    int calculateChunkDistance(int x1, int y1, int z1, int x2, int y2, int z2) const;
};