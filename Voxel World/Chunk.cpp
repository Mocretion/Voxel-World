#include "Chunk.h"
#include <random>
#include <algorithm>
#include <iostream>

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

    // Generate the mesh data
    generateMesh();
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

// NEW: Calculate ambient occlusion for a vertex
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

void Chunk::generateMesh()
{
    vertices.clear();
    indices.clear();

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
                    float variation = (noiseGenerator->noise2D(worldX * 0.1f, worldZ * 0.1f) + 1) * 0.5f * 0.4f;

                    color += glm::vec3(variation);
                    color = glm::clamp(color, 0.0f, 1.0f);

                    // Generate faces with ambient occlusion
                    addCubeWithCullingAndAO(worldX, worldY, worldZ, color, x, y, z);
                }
            }
        }
    }
}

void Chunk::addCubeWithCullingAndAO(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ)
{
    // Check which faces should be rendered based on neighbors
    bool renderFront = !hasCubeAt(localX, localY, localZ + 1);  // +Z direction
    bool renderBack = !hasCubeAt(localX, localY, localZ - 1);   // -Z direction
    bool renderRight = !hasCubeAt(localX + 1, localY, localZ);  // +X direction
    bool renderLeft = !hasCubeAt(localX - 1, localY, localZ);   // -X direction
    bool renderTop = !hasCubeAt(localX, localY + 1, localZ);    // +Y direction
    bool renderBottom = !hasCubeAt(localX, localY - 1, localZ); // -Y direction

    if (renderFront)
    {
        // Front face (Z+) with AO calculation
        glm::vec3 normal(0, 0, 1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        // Calculate AO for each vertex of this face
        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f));

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao3}
            });

        // Adjust triangle winding based on AO to prevent visual artifacts
        // This ensures smooth gradients across triangulated quads
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
        // Back face (Z-) with AO
        glm::vec3 normal(0, 0, -1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f));

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
        // Left face (X-) with AO
        glm::vec3 normal(-1, 0, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f));

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
        // Right face (X+) with AO
        glm::vec3 normal(1, 0, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f));

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
        // Top face (Y+) with AO
        glm::vec3 normal(0, 1, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f));

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
        // Bottom face (Y-) with AO
        glm::vec3 normal(0, -1, 0);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f));
        float ao1 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f));
        float ao2 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f));
        float ao3 = calculateVertexAO(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f));

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

// ... (rest of the implementation remains the same: addCube, ChunkManager, etc.)

void Chunk::addCube(float worldX, float worldY, float worldZ, glm::vec3 color)
{
    // Get the current vertex count (for index calculation)
    uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

    // Define cube vertices (8 vertices total)
    // Note: This method doesn't use AO - it's the old implementation
    std::vector<Vertex> cubeVertices = {
        // Front face vertices (Z+)
        {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, 1.0f},  // Full brightness (no AO)
        {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, 1.0f},
        {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, 1.0f},
        {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, 1.0f},

        // Back face vertices (Z-)
        {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, 1.0f},
        {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color, 1.0f},
        {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, 1.0f},
        {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color, 1.0f},
    };

    // Add vertices to our chunk's vertex list
    vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

    // Define indices with correct counter-clockwise winding
    std::vector<uint16_t> cubeIndices = {
        // Front face (Z+): 0,1,2 and 2,3,0
        static_cast<uint16_t>(baseIndex + 0), static_cast<uint16_t>(baseIndex + 1), static_cast<uint16_t>(baseIndex + 2),
        static_cast<uint16_t>(baseIndex + 2), static_cast<uint16_t>(baseIndex + 3), static_cast<uint16_t>(baseIndex + 0),

        // Back face (Z-): When viewed from outside (negative Z direction), we see this face
        // Order: 5,4,7 and 7,6,5 (counter-clockwise when viewed from negative Z)
        static_cast<uint16_t>(baseIndex + 5), static_cast<uint16_t>(baseIndex + 4), static_cast<uint16_t>(baseIndex + 7),
        static_cast<uint16_t>(baseIndex + 7), static_cast<uint16_t>(baseIndex + 6), static_cast<uint16_t>(baseIndex + 5),

        // Left face (X-): When viewed from outside (negative X direction)
        // Order: 4,0,3 and 3,7,4 (counter-clockwise when viewed from negative X)
        static_cast<uint16_t>(baseIndex + 4), static_cast<uint16_t>(baseIndex + 0), static_cast<uint16_t>(baseIndex + 3),
        static_cast<uint16_t>(baseIndex + 3), static_cast<uint16_t>(baseIndex + 7), static_cast<uint16_t>(baseIndex + 4),

        // Right face (X+): When viewed from outside (positive X direction)
        // Order: 1,5,6 and 6,2,1 (counter-clockwise when viewed from positive X)
        static_cast<uint16_t>(baseIndex + 1), static_cast<uint16_t>(baseIndex + 5), static_cast<uint16_t>(baseIndex + 6),
        static_cast<uint16_t>(baseIndex + 6), static_cast<uint16_t>(baseIndex + 2), static_cast<uint16_t>(baseIndex + 1),

        // Top face (Y+): When viewed from outside (positive Y direction)
        // Order: 3,2,6 and 6,7,3 (counter-clockwise when viewed from positive Y)
        static_cast<uint16_t>(baseIndex + 3), static_cast<uint16_t>(baseIndex + 2), static_cast<uint16_t>(baseIndex + 6),
        static_cast<uint16_t>(baseIndex + 6), static_cast<uint16_t>(baseIndex + 7), static_cast<uint16_t>(baseIndex + 3),

        // Bottom face (Y-): When viewed from outside (negative Y direction)
        // Order: 1,0,4 and 4,5,1 (counter-clockwise when viewed from negative Y)
        static_cast<uint16_t>(baseIndex + 1), static_cast<uint16_t>(baseIndex + 0), static_cast<uint16_t>(baseIndex + 4),
        static_cast<uint16_t>(baseIndex + 4), static_cast<uint16_t>(baseIndex + 5), static_cast<uint16_t>(baseIndex + 1)
    };

    // Add indices to our chunk's index list
    indices.insert(indices.end(), cubeIndices.begin(), cubeIndices.end());
}

ChunkManager::ChunkManager() : noiseGenerator(12345)  // Use a fixed seed for consistent terrain
{
    // You can change the seed to get different terrain
}

// ChunkManager Implementation
void ChunkManager::generateTerrain(int chunksX, int chunksZ)
{
    chunks.clear();  // Clear any existing chunks

    // Generate chunks in a grid pattern
    // Center the grid around origin (like generating from -chunksX/2 to +chunksX/2)
    int startX = -chunksX / 2;
    int startZ = -chunksZ / 2;

    for (int x = 0; x < chunksX; x++)
    {
        for (int z = 0; z < chunksZ; z++)
        {
            int chunkX = startX + x;
            int chunkZ = startZ + z;

            // Create and add new chunk to our list
            chunks.emplace_back(chunkX, chunkZ, &noiseGenerator);
        }
    }
}

std::vector<Vertex> ChunkManager::getAllVertices() const
{
    std::vector<Vertex> allVertices;

    // Combine vertices from all chunks
    for (const auto& chunk : chunks)
    {
        const auto& chunkVertices = chunk.getVertices();
        allVertices.insert(allVertices.end(), chunkVertices.begin(), chunkVertices.end());
    }

    return allVertices;
}

std::vector<uint32_t> ChunkManager::getAllIndices() const
{
    std::vector<uint32_t> allIndices;
    uint32_t vertexOffset = 0;

    // Combine indices from all chunks, adjusting for vertex offset
    for (const auto& chunk : chunks)
    {
        const auto& chunkIndices = chunk.getIndices();

        // Add indices with offset to account for previous chunks' vertices
        for (uint32_t index : chunkIndices)
        {
            allIndices.push_back(index + vertexOffset);
        }

        // Update offset for next chunk
        vertexOffset += static_cast<uint32_t>(chunk.getVertices().size());
    }

    return allIndices;
}

Chunk* ChunkManager::getChunk(int chunkX, int chunkZ)
{
    // Find chunk with matching coordinates
    for (auto& chunk : chunks)
    {
        if (chunk.getChunkX() == chunkX && chunk.getChunkZ() == chunkZ)
        {
            return &chunk;  // Return pointer to the chunk
        }
    }
    return nullptr;  // Not found (like returning null in C#)
}

void Chunk::localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const
{
    worldX = chunkX * CHUNK_SIZE + localX;
    worldY = localY;  // Y coordinates are already world coordinates
    worldZ = chunkZ * CHUNK_SIZE + localZ;
}

// NEW: Check if cube exists at world coordinates using ChunkManager
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

// NEW: Generate mesh using ChunkManager for cross-chunk AO
void Chunk::generateMeshWithManager(ChunkManager* manager)
{
    vertices.clear();
    indices.clear();

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
                    float variation = (noiseGenerator->noise2D(worldX * 0.1f, worldZ * 0.1f) + 1) * 0.5f * 0.4f;

                    color += glm::vec3(variation);
                    color = glm::clamp(color, 0.0f, 1.0f);

                    // Generate faces with cross-chunk ambient occlusion
                    addCubeWithCullingAndAOCrossChunk(worldX, worldY, worldZ, color, x, y, z, manager);
                }
            }
        }
    }
}

// NEW: Calculate AO with cross-chunk support
float Chunk::calculateVertexAOCrossChunk(int localX, int localY, int localZ, const glm::vec3& normal, const glm::vec3& vertexOffset, ChunkManager* manager) const
{
    // Convert vertex offset to integer directions for neighbor checking
    int dx = static_cast<int>(vertexOffset.x * 2.0f);  // *2 because offset is ±0.5
    int dy = static_cast<int>(vertexOffset.y * 2.0f);
    int dz = static_cast<int>(vertexOffset.z * 2.0f);

    // For each vertex, we check 3 neighbors: 2 edge neighbors and 1 corner neighbor
    int side1 = 0, side2 = 0, corner = 0;

    // Convert local coordinates to world coordinates for cross-chunk queries
    int worldX, worldY, worldZ;
    localToWorld(localX, localY, localZ, worldX, worldY, worldZ);

    // Determine which neighbors to check based on the face normal and vertex position
    if (std::abs(normal.x) > 0.5f) // X-facing normal (left/right face)
    {
        int offsetX = normal.x > 0 ? 1 : -1;  // Move outside the current voxel

        // Use world coordinates for cross-chunk queries
        side1 = hasCubeAtWorld(worldX + offsetX, worldY + dy, worldZ, manager) ? 1 : 0;      // Y direction neighbor
        side2 = hasCubeAtWorld(worldX + offsetX, worldY, worldZ + dz, manager) ? 1 : 0;      // Z direction neighbor
        corner = hasCubeAtWorld(worldX + offsetX, worldY + dy, worldZ + dz, manager) ? 1 : 0; // Corner neighbor
    }
    else if (std::abs(normal.y) > 0.5f) // Y-facing normal (top/bottom face)
    {
        int offsetY = normal.y > 0 ? 1 : -1;  // Move outside the current voxel

        side1 = hasCubeAtWorld(worldX + dx, worldY + offsetY, worldZ, manager) ? 1 : 0;      // X direction neighbor
        side2 = hasCubeAtWorld(worldX, worldY + offsetY, worldZ + dz, manager) ? 1 : 0;      // Z direction neighbor
        corner = hasCubeAtWorld(worldX + dx, worldY + offsetY, worldZ + dz, manager) ? 1 : 0; // Corner neighbor
    }
    else // Z-facing normal (front/back face)
    {
        int offsetZ = normal.z > 0 ? 1 : -1;  // Move outside the current voxel

        side1 = hasCubeAtWorld(worldX + dx, worldY, worldZ + offsetZ, manager) ? 1 : 0;      // X direction neighbor
        side2 = hasCubeAtWorld(worldX, worldY + dy, worldZ + offsetZ, manager) ? 1 : 0;      // Y direction neighbor
        corner = hasCubeAtWorld(worldX + dx, worldY + dy, worldZ + offsetZ, manager) ? 1 : 0; // Corner neighbor
    }

    // Calculate AO using the standard voxel AO formula
    if (side1 && side2)
    {
        return 0.25f;  // Both sides blocked = heavily occluded
    }
    else
    {
        return 1.0f - (side1 + side2 + corner) * 0.25f;  // Gradual occlusion
    }
}

// NEW: Add cube with cross-chunk culling and AO
void Chunk::addCubeWithCullingAndAOCrossChunk(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ, ChunkManager* manager)
{
    // Convert local coordinates to world coordinates for cross-chunk neighbor checking
    int worldCubeX, worldCubeY, worldCubeZ;
    localToWorld(localX, localY, localZ, worldCubeX, worldCubeY, worldCubeZ);

    // Check which faces should be rendered based on cross-chunk neighbors
    bool renderFront = !hasCubeAtWorld(worldCubeX, worldCubeY, worldCubeZ + 1, manager);   // +Z direction
    bool renderBack = !hasCubeAtWorld(worldCubeX, worldCubeY, worldCubeZ - 1, manager);    // -Z direction
    bool renderRight = !hasCubeAtWorld(worldCubeX + 1, worldCubeY, worldCubeZ, manager);   // +X direction
    bool renderLeft = !hasCubeAtWorld(worldCubeX - 1, worldCubeY, worldCubeZ, manager);    // -X direction
    bool renderTop = !hasCubeAtWorld(worldCubeX, worldCubeY + 1, worldCubeZ, manager);     // +Y direction
    bool renderBottom = !hasCubeAtWorld(worldCubeX, worldCubeY - 1, worldCubeZ, manager);  // -Y direction

    if (renderFront)
    {
        // Front face (Z+) with cross-chunk AO calculation
        glm::vec3 normal(0, 0, 1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        // Calculate cross-chunk AO for each vertex of this face
        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager);

        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao0},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color, ao1},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao2},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color, ao3}
            });

        // Adjust triangle winding based on AO
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

    // Repeat similar pattern for other faces (renderBack, renderLeft, renderRight, renderTop, renderBottom)
    // I'll show one more example and you can apply the same pattern to the rest

    if (renderBack)
    {
        // Back face (Z-) with cross-chunk AO
        glm::vec3 normal(0, 0, -1);
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager);

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

        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager);

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

        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager);

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

        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, 0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, 0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, 0.5f, -0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, 0.5f, -0.5f), manager);

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

        float ao0 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, -0.5f), manager);
        float ao1 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, -0.5f), manager);
        float ao2 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(0.5f, -0.5f, 0.5f), manager);
        float ao3 = calculateVertexAOCrossChunk(localX, localY, localZ, normal, glm::vec3(-0.5f, -0.5f, 0.5f), manager);

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

// NEW: ChunkManager methods for cross-chunk support

void ChunkManager::worldToChunkAndLocal(int worldX, int worldY, int worldZ, int& chunkX, int& chunkZ, int& localX, int& localY, int& localZ) const
{
    // Calculate which chunk these world coordinates belong to
    // This handles negative coordinates correctly using integer division
    chunkX = worldX < 0 ? (worldX - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldX / Chunk::CHUNK_SIZE;
    chunkZ = worldZ < 0 ? (worldZ - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE : worldZ / Chunk::CHUNK_SIZE;

    // Calculate local coordinates within the chunk
    localX = worldX - chunkX * Chunk::CHUNK_SIZE;
    localY = worldY;  // Y coordinates are already local
    localZ = worldZ - chunkZ * Chunk::CHUNK_SIZE;

    // Ensure local coordinates are in valid range [0, CHUNK_SIZE-1]
    if (localX < 0) localX += Chunk::CHUNK_SIZE;
    if (localZ < 0) localZ += Chunk::CHUNK_SIZE;
}

bool ChunkManager::hasCubeAtWorld(int worldX, int worldY, int worldZ) const
{
    // Convert world coordinates to chunk and local coordinates
    int chunkX, chunkZ, localX, localY, localZ;
    worldToChunkAndLocal(worldX, worldY, worldZ, chunkX, chunkZ, localX, localY, localZ);

    // Find the chunk
    Chunk* chunk = const_cast<ChunkManager*>(this)->getChunk(chunkX, chunkZ);
    if (chunk == nullptr)
    {
        return false;  // No chunk exists at this location
    }

    // Check if cube exists in that chunk
    return chunk->hasCubeAt(localX, localY, localZ);
}

void ChunkManager::regenerateAllMeshes()
{
    // Regenerate all chunk meshes with cross-chunk AO support
    for (auto& chunk : chunks)
    {
        chunk.generateMeshWithManager(this);
    }
}