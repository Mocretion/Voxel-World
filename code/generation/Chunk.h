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
#include <atomic>
#include <bgfx/bgfx.h>

class VulkanRenderer;
class ChunkManager;
class Chunk;
class GBufferShader;

constexpr int CHNK_SIZE = 64;

struct NeighborCache {

    // Cache structure for fast neighbor lookups without locks
    // We only need to cache the border blocks of neighboring chunks
    struct BorderData {
        // For each face, we store a 2D array of border blocks
        // Index: [face][u][v] where u,v are the 2D coordinates on that face
        bool leftFace[CHNK_SIZE][CHNK_SIZE];    // -X face (YZ plane)
        bool rightFace[CHNK_SIZE][CHNK_SIZE];   // +X face (YZ plane)
        bool bottomFace[CHNK_SIZE][CHNK_SIZE];  // -Y face (XZ plane)
        bool topFace[CHNK_SIZE][CHNK_SIZE];     // +Y face (XZ plane)
        bool backFace[CHNK_SIZE][CHNK_SIZE];    // -Z face (XY plane)
        bool frontFace[CHNK_SIZE][CHNK_SIZE];   // +Z face (XY plane)

        bool hasData = false;  // Whether this neighbor exists
    };

    // 3x3x3 grid of neighbors (1,1,1 is self, which we don't use)
    BorderData neighbors[3][3][3];

    // The chunk coordinates this cache is for
    int centerChunkX, centerChunkY, centerChunkZ;

    // Check if a world position has a cube using the cache
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ, const Chunk* centerChunk) const;

    // Populate the cache from the chunk manager (done once before meshing)
    void populateFromManager(ChunkManager* manager, int chunkX, int chunkY, int chunkZ);

    // Clear the cache
    void clear();
};

class Chunk {
public:
    static const int CHUNK_SIZE = CHNK_SIZE;  // 64x64x64 cubes per chunk
    static const float CUBE_SIZE;      // Size of each cube (will define in .cpp)    

    // Constructor - creates a chunk at the given 3D world position
    Chunk(int chunkX, int chunkY, int chunkZ);

    ~Chunk();

    void generateTerrain();

    // Generate mesh with greedy meshing
    void generateMeshWithManager(ChunkManager* manager, NeighborCache* cache, bool debug);

    void clearMesh();

    void uploadToBgfx(class BgfxRenderer* renderer);
    void draw(class BgfxRenderer* renderer);
    void cleanupBgfxResources();

    // Get the generated vertices and indices
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint32_t>& getIndices() const { return indices; }

    // Get chunk position
    int getChunkX() const { return chunkX; }
    int getChunkY() const { return chunkY; }
    int getChunkZ() const { return chunkZ; }

    // Check if a cube exists at the given local coordinates (0-63)
    bool hasCubeAt(int x, int y, int z) const;

    // Check if a cube exists at world coordinates (can query other chunks)
    bool hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const;

    // Set whether a cube exists at local coordinates
    void setCubeAt(int x, int y, int z, bool exists);

    // Mesh state tracking
    bool isMeshGenerated() const { return meshGenerated; }
    void setMeshGenerated(bool isGenerated) { meshGenerated = isGenerated; }

    bool isUploadedToGPU() const { return uploadedToGPU; }
    bool hasRenderableData() const
    {
        return meshGenerated && !indices.empty() &&
            bgfx::isValid(vertexBuffer) && bgfx::isValid(indexBuffer);
    }

    // Completion tracking for incremental display
    bool isNewlyCompleted() const { return newlyCompleted.load(); }
    void markAsDisplayed() { newlyCompleted.store(false); }

private:

    int chunkX, chunkY, chunkZ;

    // Stack-allocated arrays for fast access
    bool cubes[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE];
    bool getCubeAtCoord(int x, int y, int z) const
    {
        return cubes[(z * CHUNK_SIZE * CHUNK_SIZE) + (y * CHUNK_SIZE) + x];
    }
    void setCubeAtCoord(int x, int y, int z, bool exists)
    {
        cubes[(z * CHUNK_SIZE * CHUNK_SIZE) + (y * CHUNK_SIZE) + x] = exists;
    }

    float surfaceHeight[CHUNK_SIZE * CHUNK_SIZE];

    // Mesh data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    bool meshGenerated = false;

    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;
    bool uploadedToGPU = false;
    bool gpuResourcesDirty = true;  // Track if mesh changed since last upload

    // Atomic flag for completion tracking
    std::atomic<bool> newlyCompleted{ false };

    // === GREEDY MESHING STRUCTURES ===

    // Represents a face quad in the mesh
    struct Quad {
        glm::vec3 position;     // Bottom-left corner position
        glm::vec2 size;         // Width and height of the quad
        glm::uint8_t normal;       // Face normal (which direction it faces)
        glm::vec3 color;        // Color of the quad
        int axis;               // 0=X, 1=Y, 2=Z (which axis is the normal)

        Quad(glm::vec3 pos, glm::vec2 sz, glm::vec3 norm, glm::vec3 col, int ax)
            : position(pos), size(sz), color(col), axis(ax)
        {
            normal = 0;  // +X
            if (norm.x < 0) normal = 1; // -X
            else if (norm.y > 0) normal = 2; // +Y
            else if (norm.y < 0) normal = 3; // -Y
            else if (norm.z > 0) normal = 4; // +Z
            else if (norm.z < 0) normal = 5; // -Z
        }
    };

    // === HELPER METHODS ===

    void generate3DTerrain();
    glm::vec3 getTerrainColor(int worldY) const;
    void localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const;

    // === GREEDY MESHING METHODS ===

    // Main greedy meshing algorithm
    void generateGreedyMesh(ChunkManager* manager, NeighborCache* cache, bool debug);

    // Check if a face should be rendered (not blocked by adjacent cube)
    bool shouldRenderFace(int x, int y, int z, int direction, NeighborCache* cache) const;

    // Get face color (could be different per face in the future)
    glm::vec3 getFaceColor(int x, int y, int z, int direction) const;

    // Generate quads for a specific axis/direction
    void generateQuadsForAxis(int axis, int direction, NeighborCache* cache, std::vector<Quad>& quads);

    // Try to expand a quad in both U and V directions (greedy part)
    glm::vec2 expandQuad(int axis, int direction, int x, int y, int z,
        const glm::vec3& color, NeighborCache* cache,
        std::vector<std::vector<std::vector<bool>>>& processed);

    // Convert quads to actual vertex/index data
    void convertQuadsToMesh(const std::vector<Quad>& quads);

    // Add a single quad to the mesh
    void addQuadToMesh(const Quad& quad);

    // Helper to get axis coordinates (converts 3D coords to 2D for each axis)
    void getAxisCoords(int axis, int x, int y, int z, int& u, int& v, int& w) const;
    void setAxisCoords(int axis, int u, int v, int w, int& x, int& y, int& z) const;

    // Allow ChunkManager to mark completion
    friend class ChunkManager;
    void markNewlyCompleted() { newlyCompleted.store(true); }
};