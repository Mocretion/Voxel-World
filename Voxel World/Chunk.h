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

class ChunkManager;

class Chunk {
public:
	static const int CHUNK_SIZE = 32;  // 32x32 cubes per chunk
	static const float CUBE_SIZE;      // Size of each cube (will define in .cpp)

	// Constructor - creates a chunk at the given world position
	Chunk(int chunkX, int chunkZ, PerlinNoise* noise = nullptr);

	// Generate mesh with cross-chunk AO support
	void generateMeshWithManager(ChunkManager* manager, bool debug);

	void clearMesh() {
		vertices.clear();
		indices.clear();
		meshGenerated = false;
	}

	// Get the generated vertices and indices
	const std::vector<Vertex>& getVertices() const { return vertices; }
	const std::vector<uint32_t>& getIndices() const { return indices; }

	// Get chunk position
	int getChunkX() const { return chunkX; }
	int getChunkZ() const { return chunkZ; }

	// Check if a cube exists at the given local coordinates (0-31)
	bool hasCubeAt(int x, int y, int z) const;

	// Check if a cube exists at world coordinates (can query other chunks)
	bool hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const;

	// Set whether a cube exists at local coordinates
	void setCubeAt(int x, int y, int z, bool exists);

	// Get height at local coordinates
	int getHeightAt(int x, int z) const;  // Get terrain height

	// Mesh state tracking
	bool isMeshGenerated() const { return meshGenerated; }
	void setMeshGenerated(bool isGenerated) { meshGenerated = isGenerated; }

private:
	int chunkX, chunkZ;  // Chunk coordinates in the world
	PerlinNoise* noiseGenerator;  // Pointer to noise generator

	// 3D array to store which cubes exist (true) or are air (false)
	// Using vector<vector<vector<bool>>> - like bool[32][32][32] but dynamic
	static const int MAX_HEIGHT = 32;  // Maximum terrain height
	std::vector<std::vector<std::vector<bool>>> cubes;

	std::vector<std::vector<int>> heights;

	// Generated mesh data
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	// Track if mesh has been generated
	bool meshGenerated = false;

	// Helper function with cross-chunk AO support
	void addCubeWithCullingAndAOCached(float worldX, float worldY, float worldZ, glm::vec3 color,
		int localX, int localY, int localZ,
		ChunkManager* manager,
		const std::unordered_map<uint64_t, const Chunk*>& neighborCache, bool debug);

	// Calculate ambient occlusion for a specific vertex (original version)
	float calculateVertexAO(int x, int y, int z, const glm::vec3& normal, const glm::vec3& vertexOffset) const;

	// Calculate ambient occlusion with cross-chunk support
	float calculateVertexAOCached(int localX, int localY, int localZ, const glm::vec3& normal,
		const glm::vec3& vertexOffset, ChunkManager* manager,
		const std::unordered_map<uint64_t, const Chunk*>& neighborCache) const;

	void generateTerrain();

	// Get color based on height (for terrain coloring)
	glm::vec3 getTerrainColor(int height) const;

	// Helper to convert local coordinates to world coordinates
	void localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const;
};