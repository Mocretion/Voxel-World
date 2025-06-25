#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "Vertex.h"
#include "PerlinNoise.h"

class Chunk {
public:
    static const int CHUNK_SIZE = 32;  // 32x32 cubes per chunk
    static const float CUBE_SIZE;      // Size of each cube (will define in .cpp)

    // Constructor - creates a chunk at the given world position
    Chunk(int chunkX, int chunkZ, PerlinNoise* noise = nullptr);

    // Generate the vertex and index data for this chunk
    void generateMesh();

    // Get the generated vertices and indices
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }

    // Get chunk position
    int getChunkX() const { return chunkX; }
    int getChunkZ() const { return chunkZ; }

    // Check if a cube exists at the given local coordinates (0-31)
    bool hasCubeAt(int x, int y, int z) const;

    // Set whether a cube exists at local coordinates
    void setCubeAt(int x, int y, int z, bool exists);

    // Get height at local coordinates
    int getHeightAt(int x, int z) const;  // Get terrain height

private:
    int chunkX, chunkZ;  // Chunk coordinates in the world
    PerlinNoise* noiseGenerator;  // Pointer to noise generator

    // 2D array to store which cubes exist (true) or are air (false)
    // Using vector<vector<bool>> - like bool[32][32] but dynamic
    static const int MAX_HEIGHT = 32;  // Maximum terrain height
    std::vector<std::vector<std::vector<bool>>> cubes;

    std::vector<std::vector<int>> heights;

    // Generated mesh data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Helper function to add a cube at world position
    void addCube(float worldX, float worldY, float worldZ, glm::vec3 color);

    // Helper function to add cube with face culling
    void addCubeWithCulling(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ);

    void generateTerrain();

    // Get color based on height (for terrain coloring)
    glm::vec3 getTerrainColor(int height) const;
};

// Chunk Manager - handles multiple chunks
class ChunkManager {
public:
    ChunkManager();

    // Generate a floor of chunks around the origin
    void generateTerrain(int chunksX, int chunksZ);

    // Get all vertices from all chunks (for rendering)
    std::vector<Vertex> getAllVertices() const;

    // Get all indices from all chunks (for rendering)
    std::vector<uint32_t> getAllIndices() const;

    // Get chunk at specific chunk coordinates
    Chunk* getChunk(int chunkX, int chunkZ);

private:
    // Store all chunks - like List<Chunk> in C#
    std::vector<Chunk> chunks;

    PerlinNoise noiseGenerator;
};