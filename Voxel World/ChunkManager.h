#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Vertex.h"
#include "PerlinNoise.h"
#include "RemovableQueue.h"
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include "Chunk.h"

class ChunkManager {
public:
	ChunkManager();

	// Generate a single chunk at specific coordinates
	void generateChunks(int maxUpdates);

	// Add chunks to the generation queue.
	void markChunksForGeneration(std::vector<std::pair<int, int>> chunks);

	// Add chunks to the meshing queue.
	void markChunksForMeshing(std::vector<std::pair<int, int>> chunks);

	// Remove chunks that are too far from the given center point
	int removeDistantChunks(int centerX, int centerZ, int maxDataDistance, int maxMeshDistance);

	// OPTIMIZED: Only regenerate meshes that need updates
	void updateMeshes();

	// Get all vertices from all chunks (for rendering)
	std::vector<Vertex> getAllVertices() const;

	// Get all indices from all chunks (for rendering)
	std::vector<uint32_t> getAllIndices() const;

	std::unordered_map<uint64_t, const Chunk*> createNeighborCache(int centerChunkX, int centerChunkZ) const;

	// Get chunk at specific chunk coordinates
	Chunk* getChunk(int chunkX, int chunkZ);

	// Check if a cube exists at world coordinates (across all chunks)
	bool hasCubeAtWorld(int worldX, int worldY, int worldZ) const;

	bool hasCubeAtWorldCached(int worldX, int worldY, int worldZ,
		const std::unordered_map<uint64_t, const Chunk*>& chunkCache, bool debug) const;

	// Get the number of currently loaded chunks
	int getLoadedChunkCount() const { return static_cast<int>(chunks.size()); }

	// Get list of all loaded chunk coordinates (for debugging)
	std::vector<std::pair<int, int>> getLoadedChunkCoordinates() const;

	// Performance tracking

	int getChunksNeedingGeneration() const;

	bool getIsDoneGenerating() const
	{
		return isDoneGenerating;
	}

	std::atomic<bool> usedNewGenerationValue{ true };

	int getChunksNeedingMeshUpdate() const;

	bool isDoneMeshing() const
	{
		return threadsDoneMeshing.load() == MESH_THREAD_COUNT;
	}

	std::atomic<bool> usedNewMeshingValue{ true };
	std::shared_mutex chunkMutex;


private:
	int GEN_THREAD_COUNT = 1;  // Number of threads for data generation
	int MESH_THREAD_COUNT = 4;  // Number of threads for mesh generation

	// Use a hash map instead of vector for efficient chunk lookup
	// Key: pair<chunkX, chunkZ>, Value: Chunk object
	// This is like Dictionary<(int, int), Chunk> in C#
	std::unordered_map<uint64_t, Chunk> chunks;

	// Track which chunks need to be generated
	RemovableQueue<uint64_t> chunksToGenerate;

	// Track which chunks need mesh updates
	RemovableQueue<uint64_t> chunksToDraw;

	std::mutex genMutex, meshMutex;

	PerlinNoise noiseGenerator;

	std::jthread generationWorker;
	// Add a chunk to the generation queue.
	bool markChunkForGeneration(int chunkX, int chunkZ);
	std::atomic<bool> isDoneGenerating{ true };

	std::jthread meshWorker;
	// Add a chunk to the meshing queue.
	bool markChunkForMeshing(int chunkX, int chunkZ);
	std::atomic<int> threadsDoneMeshing{ MESH_THREAD_COUNT };

	// Helper to convert chunk coordinates to hash key
	uint64_t chunkCoordsToKey(int chunkX, int chunkZ) const;

	std::pair<int, int> keyToChunkCoords(uint64_t key) const;

	// Helper to convert world coordinates to chunk coordinates and local coordinates
	void worldToChunkAndLocal(int worldX, int worldY, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localY, int& localZ) const;

	// Get neighboring chunk coordinates
	std::vector<std::pair<int, int>> getNeighborCoordinates(int chunkX, int chunkZ) const;

	mutable std::shared_mutex chunkDataMutex;
};