#include "Chunk.h"
#include "ChunkManager.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <omp.h>
#include <queue>

// Define the cube size (1.0f means each cube is 1 unit)
const float Chunk::CUBE_SIZE = 1.0f;

// Chunk Constructor
Chunk::Chunk(int chunkX, int chunkZ, PerlinNoise* noise) : chunkX(chunkX), chunkZ(chunkZ), noiseGenerator(noise)
{
    // Initialize the 3D vector - like creating new bool[32][32][32] in C#
    cubes.resize(CHUNK_SIZE);
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        cubes[x].resize(MAX_HEIGHT);
        for (int y = 0; y < MAX_HEIGHT; y++)
        {
            cubes[x][y].resize(CHUNK_SIZE, false);  // false = air
        }
    }

    // Initialize heights array
    heights.resize(CHUNK_SIZE);
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        heights[x].resize(CHUNK_SIZE, 0);
    }

    // Generate terrain if we have a noise generator
    if (noiseGenerator)
    {
        generateTerrain();
    }
}

void Chunk::generateTerrain()
{
    // Generate height map using Perlin noise
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            // Calculate world position
            float worldX = (chunkX * CHUNK_SIZE + x) * CUBE_SIZE;
            float worldZ = (chunkZ * CHUNK_SIZE + z) * CUBE_SIZE;

            // Get noise value using octave noise for more interesting terrain
            float noiseValue = noiseGenerator->octaveNoise2D(
                worldX * 0.01f,  // Scale down for larger features
                worldZ * 0.01f,
                6,               // 6 octaves for good detail
                0.5f,            // persistence
                1.0f             // base scale
            );

            // Convert noise value (-1 to 1) to height (0 to MAX_HEIGHT)
            int height = static_cast<int>(10 + (noiseValue + 1.0f) * 10.0f);
            height = std::max(1, std::min(height, MAX_HEIGHT - 1));

            heights[x][z] = height;

            // Fill cubes up to this height
            for (int y = 0; y < height; y++)
            {
                cubes[x][y][z] = true;
            }
        }
    }
}

bool Chunk::hasCubeAt(int x, int y, int z) const
{
    if (x < 0 || x >= CHUNK_SIZE ||
        y < 0 || y >= MAX_HEIGHT ||
        z < 0 || z >= CHUNK_SIZE)
    {
        return false;  // Out of bounds
    }
    return cubes[x][y][z];
}

void Chunk::setCubeAt(int x, int y, int z, bool exists)
{
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < MAX_HEIGHT &&
        z >= 0 && z < CHUNK_SIZE)
    {
        cubes[x][y][z] = exists;
    }
}

int Chunk::getHeightAt(int x, int z) const
{
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE)
    {
        return heights[x][z];
    }
    return 0;
}

glm::vec3 Chunk::getTerrainColor(int height) const
{
    // Color based on height - creates a nice terrain gradient
    if (height < 5)
    {
        // Beach/sand color
        return glm::vec3(0.9f, 0.8f, 0.6f);
    }
    else if (height < 25)
    {
        // Grass color
        return glm::vec3(0.095f, 0.871f, 0.302f);
    }
    else
    {
        // Snow color
        return glm::vec3(0.9f, 0.9f, 0.95f);
    }
}

// Calculate ambient occlusion for a vertex
float Chunk::calculateVertexAO(int x, int y, int z, const glm::vec3& normal, const glm::vec3& vertexOffset) const
{
    // The key insight: we need to check neighbors RELATIVE TO THE FACE, not the voxel center
    // We check neighbors that are "outside" the current voxel, adjacent to the rendered face

    // Convert vertex offset to integer directions for neighbor checking
    int dx = static_cast<int>(vertexOffset.x * 2.0f);  // *2 because offset is ±0.5
    int dy = static_cast<int>(vertexOffset.y * 2.0f);
    int dz = static_cast<int>(vertexOffset.z * 2.0f);

    // For each vertex, we check 3 neighbors: 2 edge neighbors and 1 corner neighbor
    int side1 = 0, side2 = 0, corner = 0;

    // Determine which neighbors to check based on the face normal and vertex position
    if (std::abs(normal.x) > 0.5f) // X-facing normal (left/right face)
    {
        // For X faces, we check neighbors in the Y and Z directions relative to the vertex
        // We offset by the normal direction to check neighbors "outside" the current voxel
        int offsetX = normal.x > 0 ? 1 : -1;  // Move outside the current voxel

        side1 = hasCubeAt(x + offsetX, y + dy, z) ? 1 : 0;      // Y direction neighbor
        side2 = hasCubeAt(x + offsetX, y, z + dz) ? 1 : 0;      // Z direction neighbor
        corner = hasCubeAt(x + offsetX, y + dy, z + dz) ? 1 : 0; // Corner neighbor
    }
    else if (std::abs(normal.y) > 0.5f) // Y-facing normal (top/bottom face)
    {
        // For Y faces, we check neighbors in the X and Z directions
        int offsetY = normal.y > 0 ? 1 : -1;  // Move outside the current voxel

        side1 = hasCubeAt(x + dx, y + offsetY, z) ? 1 : 0;      // X direction neighbor
        side2 = hasCubeAt(x, y + offsetY, z + dz) ? 1 : 0;      // Z direction neighbor
        corner = hasCubeAt(x + dx, y + offsetY, z + dz) ? 1 : 0; // Corner neighbor
    }
    else // Z-facing normal (front/back face)
    {
        // For Z faces, we check neighbors in the X and Y directions
        int offsetZ = normal.z > 0 ? 1 : -1;  // Move outside the current voxel

        side1 = hasCubeAt(x + dx, y, z + offsetZ) ? 1 : 0;      // X direction neighbor
        side2 = hasCubeAt(x, y + dy, z + offsetZ) ? 1 : 0;      // Y direction neighbor
        corner = hasCubeAt(x + dx, y + dy, z + offsetZ) ? 1 : 0; // Corner neighbor
    }

    // Calculate AO using the standard voxel AO formula
    // This formula creates smooth gradients based on neighbor configuration
    if (side1 && side2)
    {
        // Both sides blocked = heavily occluded (deep corner)
        return 0.25f;
    }
    else
    {
        // Gradual occlusion based on number of blocked neighbors
        return 1.0f - (side1 + side2 + corner) * 0.25f;
    }
}

void Chunk::localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const
{
    worldX = chunkX * CHUNK_SIZE + localX;
    worldY = localY;  // Y coordinates are already world coordinates
    worldZ = chunkZ * CHUNK_SIZE + localZ;
}

// Check if cube exists at world coordinates using ChunkManager
bool Chunk::hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const
{
    if (manager == nullptr)
    {
        // Fallback to local checking if no manager available
        // Convert world coords back to local for bounds checking
        int localX = worldX - (chunkX * CHUNK_SIZE);
        int localY = worldY;
        int localZ = worldZ - (chunkZ * CHUNK_SIZE);
        return hasCubeAt(localX, localY, localZ);
    }

    return manager->hasCubeAtWorld(worldX, worldY, worldZ);
}

// Generate mesh using ChunkManager for cross-chunk AO
void Chunk::generateMeshWithManager(ChunkManager* manager, bool debug)
{
    vertices.clear();
    indices.clear();

    // Step 1: Create cache with ONE lock
    std::unordered_map<uint64_t, const Chunk*> neighborCache;
    {
        std::shared_lock<std::shared_mutex> lock(manager->chunkMutex);
        neighborCache = manager->createNeighborCache(chunkX, chunkZ);
    }
    // Lock released here!

    if (debug)
        std::cout << "Generating mesh for chunk (" << chunkX << ", " << chunkZ << ") with " 
		<< neighborCache.size() << " neighbors cached." << std::endl;

    // Step 2: Generate mesh using cached data (NO MORE LOCKS!)
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            for (int y = 0; y < MAX_HEIGHT; y++)
            {
                if (hasCubeAt(x, y, z))
                {
                    float worldX = (chunkX * CHUNK_SIZE + x) * CUBE_SIZE;
                    float worldY = y * CUBE_SIZE;
                    float worldZ = (chunkZ * CHUNK_SIZE + z) * CUBE_SIZE;

                    glm::vec3 color = getTerrainColor(y);

                    // Use cached version instead of manager->hasCubeAtWorld
                    addCubeWithCullingAndAOCached(worldX, worldY, worldZ, color, x, y, z, manager, neighborCache, debug);
                    
                }
            }
        }
    }

    neighborCache.clear();

    
    setMeshGenerated(true);
}

// Calculate AO with cross-chunk support
float Chunk::calculateVertexAOCached(int localX, int localY, int localZ, const glm::vec3& normal,
    const glm::vec3& vertexOffset, ChunkManager* manager,
    const std::unordered_map<uint64_t, const Chunk*>& neighborCache) const
{
    int dx = static_cast<int>(vertexOffset.x * 2.0f);
    int dy = static_cast<int>(vertexOffset.y * 2.0f);
    int dz = static_cast<int>(vertexOffset.z * 2.0f);

    int side1 = 0, side2 = 0, corner = 0;
    int worldX, worldY, worldZ;
    localToWorld(localX, localY, localZ, worldX, worldY, worldZ);

    if (std::abs(normal.x) > 0.5f)
    {
        int offsetX = normal.x > 0 ? 1 : -1;
        side1 = manager->hasCubeAtWorldCached(worldX + offsetX, worldY + dy, worldZ, neighborCache, false) ? 1 : 0;
        side2 = manager->hasCubeAtWorldCached(worldX + offsetX, worldY, worldZ + dz, neighborCache, false) ? 1 : 0;
        corner = manager->hasCubeAtWorldCached(worldX + offsetX, worldY + dy, worldZ + dz, neighborCache, false) ? 1 : 0;
    }
    else if (std::abs(normal.y) > 0.5f)
    {
        int offsetY = normal.y > 0 ? 1 : -1;
        side1 = manager->hasCubeAtWorldCached(worldX + dx, worldY + offsetY, worldZ, neighborCache, false) ? 1 : 0;
        side2 = manager->hasCubeAtWorldCached(worldX, worldY + offsetY, worldZ + dz, neighborCache, false) ? 1 : 0;
        corner = manager->hasCubeAtWorldCached(worldX + dx, worldY + offsetY, worldZ + dz, neighborCache, false) ? 1 : 0;
    }
    else
    {
        int offsetZ = normal.z > 0 ? 1 : -1;
        side1 = manager->hasCubeAtWorldCached(worldX + dx, worldY, worldZ + offsetZ, neighborCache, false) ? 1 : 0;
        side2 = manager->hasCubeAtWorldCached(worldX, worldY + dy, worldZ + offsetZ, neighborCache, false) ? 1 : 0;
        corner = manager->hasCubeAtWorldCached(worldX + dx, worldY + dy, worldZ + offsetZ, neighborCache, false) ? 1 : 0;
    }

    if (side1 && side2)
    {
        return 0.25f;
    }
    else
    {
        return 1.0f - (side1 + side2 + corner) * 0.25f;
    }

};

void Chunk::addCubeWithCullingAndAOCached(float worldX, float worldY, float worldZ, glm::vec3 color,
    int localX, int localY, int localZ,
    ChunkManager* manager,
    const std::unordered_map<uint64_t, const Chunk*>& neighborCache, bool debug)
{
    // Convert local coordinates to world coordinates
    int worldCubeX, worldCubeY, worldCubeZ;
    localToWorld(localX, localY, localZ, worldCubeX, worldCubeY, worldCubeZ);

    // Check neighbors using CACHED data instead of locks
    bool renderFront = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY, worldCubeZ + 1, neighborCache, debug);
    bool renderBack = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY, worldCubeZ - 1, neighborCache, debug);
    bool renderRight = !manager->hasCubeAtWorldCached(worldCubeX + 1, worldCubeY, worldCubeZ, neighborCache, debug);
    bool renderLeft = !manager->hasCubeAtWorldCached(worldCubeX - 1, worldCubeY, worldCubeZ, neighborCache, debug);
    bool renderTop = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY + 1, worldCubeZ, neighborCache, debug);
    bool renderBottom = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY - 1, worldCubeZ, neighborCache, debug);

    // Generate faces with cached AO calculation
    if (renderFront)
    {
        glm::vec3 normal(0, 0, 1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        // Calculate AO using cached data
        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao3}
            });

        // Add indices with proper winding
        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }

    if (renderBack)
    {
        // Back face (Z-) with cross-chunk AO
        glm::vec3 normal(0, 0, -1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao0},
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao1},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao2},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao3}
            });

        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }

    // Continue with renderLeft, renderRight, renderTop, renderBottom...
    // (Apply the same pattern but with appropriate normals and vertex offsets)

    if (renderLeft)
    {
        // Left face (X-) with cross-chunk AO
        glm::vec3 normal(-1, 0, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao0},
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao1},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao3}
            });

        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }

    if (renderRight)
    {
        // Right face (X+) with cross-chunk AO
        glm::vec3 normal(1, 0, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao2},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao3}
            });

        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }

    if (renderTop)
    {
        // Top face (Y+) with cross-chunk AO
        glm::vec3 normal(0, 1, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, ao3}
            });

        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }

    if (renderBottom)
    {
        // Bottom face (Y-) with cross-chunk AO
        glm::vec3 normal(0, -1, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao1 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager, neighborCache);
        float ao2 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager, neighborCache);
        float ao3 = calculateVertexAOCached(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager, neighborCache);

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao3}
            });

        if (ao0 + ao2 > ao1 + ao3)
        {
            indices.insert(indices.end(), {
                baseIndex + 0, baseIndex + 1, baseIndex + 2,
                baseIndex + 2, baseIndex + 3, baseIndex + 0
                });
        }
        else
        {
            indices.insert(indices.end(), {
                baseIndex + 1, baseIndex + 2, baseIndex + 3,
                baseIndex + 3, baseIndex + 0, baseIndex + 1
                });
        }
    }
}