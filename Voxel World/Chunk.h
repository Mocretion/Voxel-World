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

class ChunkManager;

class Chunk {
public:
    static const int CHUNK_SIZE = 32;  // 32x32 cubes per chunk
    static const float CUBE_SIZE;      // Size of each cube (will define in .cpp)

    // Constructor - creates a chunk at the given world position
    Chunk(int chunkX, int chunkZ, PerlinNoise* noise = nullptr);

    // Generate mesh with cross-chunk AO support
    void generateMeshWithManager(ChunkManager* manager, bool debug);

    void clearMesh()
    {
        vertices.clear();
        indices.clear();
        meshGenerated = false;
    }

    // Get the generated vertices and indices (keep original signatures)
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

    // SIMPLE OPTIMIZATION: Use static arrays instead of nested vectors
    // This eliminates ALL allocation overhead and false sharing
    static const int MAX_HEIGHT = 32;  // Maximum terrain height

    // Stack-allocated arrays - no dynamic allocation at all!
    bool cubes[CHUNK_SIZE][MAX_HEIGHT][CHUNK_SIZE];  // Stack array
    int heights[CHUNK_SIZE][CHUNK_SIZE];             // Stack array

    // Keep mesh data as regular vectors for compatibility
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