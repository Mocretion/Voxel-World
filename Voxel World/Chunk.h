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

    // Generate mesh with greedy meshing
    void generateMeshWithManager(ChunkManager* manager, bool debug);

    void clearMesh()
    {
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
    int getHeightAt(int x, int z) const;

    // Mesh state tracking
    bool isMeshGenerated() const { return meshGenerated; }
    void setMeshGenerated(bool isGenerated) { meshGenerated = isGenerated; }

private:
    int chunkX, chunkZ;
    PerlinNoise* noiseGenerator;

    static const int MAX_HEIGHT = 32;

    // Stack-allocated arrays for fast access
    bool cubes[CHUNK_SIZE][MAX_HEIGHT][CHUNK_SIZE];
    int heights[CHUNK_SIZE][CHUNK_SIZE];

    // Mesh data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool meshGenerated = false;

    // === GREEDY MESHING STRUCTURES ===

    // Represents a face quad in the mesh
    struct Quad {
        glm::vec3 position;     // Bottom-left corner position
        glm::vec2 size;         // Width and height of the quad
        glm::vec3 normal;       // Face normal (which direction it faces)
        glm::vec3 color;        // Color of the quad
        int axis;               // 0=X, 1=Y, 2=Z (which axis is the normal)

        Quad(glm::vec3 pos, glm::vec2 sz, glm::vec3 norm, glm::vec3 col, int ax)
            : position(pos), size(sz), normal(norm), color(col), axis(ax)
        {
        }
    };

    // === HELPER METHODS ===

    void generateTerrain();
    glm::vec3 getTerrainColor(int height) const;
    void localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const;

    // === GREEDY MESHING METHODS ===

    // Main greedy meshing algorithm
    void generateGreedyMesh(ChunkManager* manager, bool debug);

    // Check if a face should be rendered (not blocked by adjacent cube)
    bool shouldRenderFace(int x, int y, int z, int direction, ChunkManager* manager) const;

    // Get face color (could be different per face in the future)
    glm::vec3 getFaceColor(int x, int y, int z, int direction) const;

    // Generate quads for a specific axis/direction
    void generateQuadsForAxis(int axis, int direction, ChunkManager* manager, std::vector<Quad>& quads);

    // Try to expand a quad in both U and V directions (greedy part)
    glm::vec2 expandQuad(int axis, int direction, int x, int y, int z,
        const glm::vec3& color, ChunkManager* manager,
        std::vector<std::vector<std::vector<bool>>>& processed);

    // Convert quads to actual vertex/index data
    void convertQuadsToMesh(const std::vector<Quad>& quads);

    // Add a single quad to the mesh
    void addQuadToMesh(const Quad& quad);

    // Helper to get axis coordinates (converts 3D coords to 2D for each axis)
    void getAxisCoords(int axis, int x, int y, int z, int& u, int& v, int& w) const;
    void setAxisCoords(int axis, int u, int v, int w, int& x, int& y, int& z) const;
};