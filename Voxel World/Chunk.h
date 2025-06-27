#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "Vertex.h"
#include "PerlinNoise.h"

// Forward declaration for circular dependency
class ChunkManager;

class Chunk {
public:
    static const int CHUNK_SIZE = 32;  // 32x32 cubes per chunk
    static const float CUBE_SIZE;      // Size of each cube (will define in .cpp)

    // Constructor - creates a chunk at the given world position
    Chunk(int chunkX, int chunkZ, PerlinNoise* noise = nullptr);

    // Generate the vertex and index data for this chunk
    void generateMesh();

    // NEW: Generate mesh with cross-chunk AO support
    void generateMeshWithManager(ChunkManager* manager);

    // Get the generated vertices and indices
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }

    // Get chunk position
    int getChunkX() const { return chunkX; }
    int getChunkZ() const { return chunkZ; }

    // Check if a cube exists at the given local coordinates (0-31)
    bool hasCubeAt(int x, int y, int z) const;

    // NEW: Check if a cube exists at world coordinates (can query other chunks)
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const;

    // Set whether a cube exists at local coordinates
    void setCubeAt(int x, int y, int z, bool exists);

    // Get height at local coordinates
    int getHeightAt(int x, int z) const;  // Get terrain height

    // NEW: Mesh state tracking
    bool isMeshGenerated() const { return meshGenerated; }
    void markMeshAsDirty() { meshGenerated = false; }

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

    // NEW: Track if mesh has been generated
    bool meshGenerated = false;

    // Helper function to add a cube at world position (old method without AO)
    void addCube(float worldX, float worldY, float worldZ, glm::vec3 color);

    // Helper function to add cube with face culling AND ambient occlusion
    void addCubeWithCullingAndAO(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ);

    // NEW: Helper function with cross-chunk AO support
    void addCubeWithCullingAndAOCrossChunk(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ, ChunkManager* manager);

    // Calculate ambient occlusion for a specific vertex (original version)
    float calculateVertexAO(int x, int y, int z, const glm::vec3& normal, const glm::vec3& vertexOffset) const;

    // NEW: Calculate ambient occlusion with cross-chunk support
    float calculateVertexAOCrossChunk(int localX, int localY, int localZ, const glm::vec3& normal, const glm::vec3& vertexOffset, ChunkManager* manager) const;

    void generateTerrain();

    // Get color based on height (for terrain coloring)
    glm::vec3 getTerrainColor(int height) const;

    // NEW: Helper to convert local coordinates to world coordinates
    void localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const;
};

// Chunk Manager - handles multiple chunks with infinite terrain support and optimized mesh generation
class ChunkManager {
public:
    ChunkManager();

    // OLD: Generate a floor of chunks around the origin (still available for testing)
    void generateTerrain(int chunksX, int chunksZ);

    // NEW: Generate a single chunk at specific coordinates
    void generateSingleChunk(int chunkX, int chunkZ);

    // NEW: Remove chunks that are too far from the given center point
    int removeDistantChunks(int centerX, int centerZ, int maxDistance);

    // OPTIMIZED: Only regenerate meshes that need updates
    void updateMeshes();

    // NEW: Mark a chunk and its neighbors for mesh regeneration
    void markChunkAndNeighborsDirty(int chunkX, int chunkZ);

    // Get all vertices from all chunks (for rendering)
    std::vector<Vertex> getAllVertices() const;

    // Get all indices from all chunks (for rendering)
    std::vector<uint32_t> getAllIndices() const;

    // Get chunk at specific chunk coordinates
    Chunk* getChunk(int chunkX, int chunkZ);

    // NEW: Check if a cube exists at world coordinates (across all chunks)
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ) const;

    // NEW: Get the number of currently loaded chunks
    int getLoadedChunkCount() const { return static_cast<int>(chunks.size()); }

    // NEW: Get list of all loaded chunk coordinates (for debugging)
    std::vector<std::pair<int, int>> getLoadedChunkCoordinates() const;

    // NEW: Performance tracking
    int getChunksNeedingMeshUpdate() const;

private:
    // NEW: Use a hash map instead of vector for efficient chunk lookup
    // Key: pair<chunkX, chunkZ>, Value: Chunk object
    // This is like Dictionary<(int, int), Chunk> in C#
    std::unordered_map<uint64_t, Chunk> chunks;

    // NEW: Track which chunks need mesh updates
    std::unordered_set<uint64_t> dirtyChunks;

    PerlinNoise noiseGenerator;

    // NEW: Helper to convert chunk coordinates to hash key
    uint64_t chunkCoordsToKey(int chunkX, int chunkZ) const;

    // NEW: Helper to convert world coordinates to chunk coordinates and local coordinates
    void worldToChunkAndLocal(int worldX, int worldY, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localY, int& localZ) const;

    // NEW: Get neighboring chunk coordinates
    std::vector<std::pair<int, int>> getNeighborCoordinates(int chunkX, int chunkZ) const;
};