#include "Chunk.h"
#include "ChunkManager.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <omp.h>
#include <queue>
#include <cstring>  // For memset

// Define the cube size (1.0f means each cube is 1 unit)
const float Chunk::CUBE_SIZE = 1.0f;

// SUPER-FAST Constructor using stack allocation
Chunk::Chunk(int chunkX, int chunkZ, PerlinNoise* noise)
    : chunkX(chunkX), chunkZ(chunkZ), noiseGenerator(noise)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // FASTEST POSSIBLE: Zero out stack arrays with memset
    // This is MUCH faster than any dynamic allocation
    std::memset(cubes, false, sizeof(cubes));    // Zero out all cubes
    std::memset(heights, 0, sizeof(heights));    // Zero out all heights

    auto allocTime = std::chrono::high_resolution_clock::now();
    auto allocDuration = std::chrono::duration_cast<std::chrono::microseconds>(allocTime - startTime);

    // Reserve space for mesh vectors to avoid reallocations later
    vertices.reserve(1000);  // Typical chunk has ~500-1500 vertices
    indices.reserve(1500);   // Typical chunk has ~750-2250 indices

    // Generate terrain if we have a noise generator
    if (noiseGenerator)
    {
        generateTerrain();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Only log occasionally to avoid spam
    static thread_local int constructorCount = 0;
    if (++constructorCount % 100 == 0)
    {
        std::cout << "[FAST] Chunk constructor - Alloc: " << allocDuration.count()
            << "micro-s, Total: " << totalDuration.count() << "ms (chunk #"
            << constructorCount << ")" << std::endl;
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

            // FAST: Direct array access
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

    // FAST: Direct array access
    return cubes[x][y][z];
}

void Chunk::setCubeAt(int x, int y, int z, bool exists)
{
    if (x >= 0 && x < CHUNK_SIZE &&
        y >= 0 && y < MAX_HEIGHT &&
        z >= 0 && z < CHUNK_SIZE)
    {
        // FAST: Direct array access
        cubes[x][y][z] = exists;
    }
}

int Chunk::getHeightAt(int x, int z) const
{
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE)
    {
        // FAST: Direct array access
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

// Calculate ambient occlusion for a vertex (same as before)
float Chunk::calculateVertexAO(int x, int y, int z, const glm::vec3& normal, const glm::vec3& vertexOffset) const
{
    // Convert vertex offset to integer directions for neighbor checking
    int dx = static_cast<int>(vertexOffset.x * 2.0f);
    int dy = static_cast<int>(vertexOffset.y * 2.0f);
    int dz = static_cast<int>(vertexOffset.z * 2.0f);

    int side1 = 0, side2 = 0, corner = 0;

    if (std::abs(normal.x) > 0.5f) // X-facing normal (left/right face)
    {
        int offsetX = normal.x > 0 ? 1 : -1;
        side1 = hasCubeAt(x + offsetX, y + dy, z) ? 1 : 0;
        side2 = hasCubeAt(x + offsetX, y, z + dz) ? 1 : 0;
        corner = hasCubeAt(x + offsetX, y + dy, z + dz) ? 1 : 0;
    }
    else if (std::abs(normal.y) > 0.5f) // Y-facing normal (top/bottom face)
    {
        int offsetY = normal.y > 0 ? 1 : -1;
        side1 = hasCubeAt(x + dx, y + offsetY, z) ? 1 : 0;
        side2 = hasCubeAt(x, y + offsetY, z + dz) ? 1 : 0;
        corner = hasCubeAt(x + dx, y + offsetY, z + dz) ? 1 : 0;
    }
    else // Z-facing normal (front/back face)
    {
        int offsetZ = normal.z > 0 ? 1 : -1;
        side1 = hasCubeAt(x + dx, y, z + offsetZ) ? 1 : 0;
        side2 = hasCubeAt(x, y + dy, z + offsetZ) ? 1 : 0;
        corner = hasCubeAt(x + dx, y + dy, z + offsetZ) ? 1 : 0;
    }

    if (side1 && side2)
    {
        return 0.25f;
    }
    else
    {
        return 1.0f - (side1 + side2 + corner) * 0.25f;
    }
}

void Chunk::localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const
{
    worldX = chunkX * CHUNK_SIZE + localX;
    worldY = localY;  // Y coordinates are already world coordinates
    worldZ = chunkZ * CHUNK_SIZE + localZ;
}

// Check if cube exists at world coordinates using ChunkManager (same as before)
bool Chunk::hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const
{
    if (manager == nullptr)
    {
        int localX = worldX - (chunkX * CHUNK_SIZE);
        int localY = worldY;
        int localZ = worldZ - (chunkZ * CHUNK_SIZE);
        return hasCubeAt(localX, localY, localZ);
    }

    return manager->hasCubeAtWorld(worldX, worldY, worldZ);
}

// Generate mesh (same logic as before, but now uses stack-allocated cube data)
void Chunk::generateMeshWithManager(ChunkManager* manager, bool debug)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Clear and reserve space for mesh data
    vertices.clear();
    indices.clear();
    vertices.reserve(1000);
    indices.reserve(1500);

    auto setupTime = std::chrono::high_resolution_clock::now();

    // Create cache with ONE lock (same as before)
    std::unordered_map<uint64_t, const Chunk*> neighborCache;
    {
        std::shared_lock<std::shared_mutex> lock(manager->chunkMutex);
        neighborCache = manager->createNeighborCache(chunkX, chunkZ);
    }

    auto cacheTime = std::chrono::high_resolution_clock::now();

    if (debug)
        std::cout << "[FAST] Generating mesh for chunk (" << chunkX << ", " << chunkZ
        << ") with " << neighborCache.size() << " neighbors cached." << std::endl;

    // Generate mesh using cached data (NO MORE LOCKS!)
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

                    float variation = (noiseGenerator->noise2D(worldX * 0.1f, worldZ * 0.1f) + 1) * 0.5f * 0.4f; // +1 * 0.5f to normalize to 0-1 range
                    color += glm::vec3(variation);
                    color = glm::clamp(color, 0.0f, 1.0f);

                    addCubeWithCullingAndAOCached(worldX, worldY, worldZ, color, x, y, z, manager, neighborCache, debug);
                }
            }
        }
    }

    neighborCache.clear();
    setMeshGenerated(true);

    auto endTime = std::chrono::high_resolution_clock::now();

    if (debug)
    {
        auto setupDuration = std::chrono::duration_cast<std::chrono::microseconds>(setupTime - startTime);
        auto cacheDuration = std::chrono::duration_cast<std::chrono::microseconds>(cacheTime - setupTime);
        auto meshDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - cacheTime);
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        std::cout << "[FAST] Mesh generation - Setup: " << setupDuration.count()
            << "micro-s, Cache: " << cacheDuration.count()
            << "micro-s, Mesh: " << meshDuration.count()
            << "ms, Total: " << totalDuration.count()
            << "ms (" << vertices.size() << " vertices)" << std::endl;
    }
}

// Calculate AO with cross-chunk support (same logic as before)
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
}

// Add cube with culling and AO (same as before, just using regular std::vector)
void Chunk::addCubeWithCullingAndAOCached(float worldX, float worldY, float worldZ, glm::vec3 color,
    int localX, int localY, int localZ,
    ChunkManager* manager,
    const std::unordered_map<uint64_t, const Chunk*>& neighborCache, bool debug)
{
    // Convert local coordinates to world coordinates
    int worldCubeX, worldCubeY, worldCubeZ;
    localToWorld(localX, localY, localZ, worldCubeX, worldCubeY, worldCubeZ);

    // Check neighbors using CACHED data
    bool renderFront = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY, worldCubeZ + 1, neighborCache, debug);
    bool renderBack = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY, worldCubeZ - 1, neighborCache, debug);
    bool renderRight = !manager->hasCubeAtWorldCached(worldCubeX + 1, worldCubeY, worldCubeZ, neighborCache, debug);
    bool renderLeft = !manager->hasCubeAtWorldCached(worldCubeX - 1, worldCubeY, worldCubeZ, neighborCache, debug);
    bool renderTop = !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY + 1, worldCubeZ, neighborCache, debug);
    bool renderBottom = localY > 0 && !manager->hasCubeAtWorldCached(worldCubeX, worldCubeY - 1, worldCubeZ, neighborCache, debug);

    // Generate faces with cached AO calculation (same logic as before)
    if (renderFront)
    {
        glm::vec3 normal(0, 0, 1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

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

    // ... (rest of the face generation code - same as before)
    // I'll include the full implementation for all faces

    if (renderBack)
    {
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

    if (renderLeft)
    {
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