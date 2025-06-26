#include "Chunk.h"
#include <random>
#include <algorithm>
#include <iostream>

// Define the cube size (1.0f means each cube is 1 unit)
const float Chunk::CUBE_SIZE = 1.0f;

// Chunk Constructor
Chunk::Chunk(int chunkX, int chunkZ, PerlinNoise* noise) : chunkX(chunkX), chunkZ(chunkZ), noiseGenerator(noise)
{
    // Initialize the 2D vector - like creating new bool[32][32] in C#
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
            // Parameters: x, y, octaves, persistence, scale
            float noiseValue = noiseGenerator->octaveNoise2D(
                worldX * 0.01f,  // Scale down for larger features
                worldZ * 0.01f,
                6,               // 6 octaves for good detail
                0.5f,            // persistence
                1.0f             // base scale
            );

            // Convert noise value (-1 to 1) to height (0 to MAX_HEIGHT)
            // Add base height of 10 and scale by 20
            int height = static_cast<int>(10 + (noiseValue + 1.0f) * 10.0f);

            // Clamp height to valid range
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

void Chunk::generateMesh()
{
    vertices.clear();
    indices.clear();

    int blocksProcessed = 0;
    int facesAdded = 0;

    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            for (int y = 0; y < MAX_HEIGHT; y++)
            {
                if (hasCubeAt(x, y, z))
                {
                    blocksProcessed++;

                    float worldX = (chunkX * CHUNK_SIZE + x) * CUBE_SIZE;
                    float worldY = y * CUBE_SIZE;
                    float worldZ = (chunkZ * CHUNK_SIZE + z) * CUBE_SIZE;

                    glm::vec3 color = getTerrainColor(y);
					float variation = (noiseGenerator->noise2D(worldX * 0.1f, worldZ * 0.1f) + 1) * 0.5f * 0.4f; // +1 * 0.5f to normalize to 0-1 range

					//std::cout << noiseGenerator->noise2D(x, z) * 0.4 << std::endl;

                    color += glm::vec3(variation);
                    color = glm::clamp(color, 0.0f, 1.0f);

                    size_t verticesBeforeAdd = vertices.size();
                    addCubeWithCulling(worldX, worldY, worldZ, color, x, y, z);
                    size_t verticesAfterAdd = vertices.size();

                    if (verticesAfterAdd > verticesBeforeAdd)
                    {
                        facesAdded += (verticesAfterAdd - verticesBeforeAdd) / 4;
                    }
                }
            }
        }
    }
}

void Chunk::addCubeWithCulling(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localY, int localZ)
{
    // Check which faces should be rendered based on neighbors
    bool renderFront = !hasCubeAt(localX, localY, localZ + 1);  // +Z direction
    bool renderBack = !hasCubeAt(localX, localY, localZ - 1);   // -Z direction
    bool renderRight = !hasCubeAt(localX + 1, localY, localZ);  // +X direction
    bool renderLeft = !hasCubeAt(localX - 1, localY, localZ);   // -X direction
    bool renderTop = !hasCubeAt(localX, localY + 1, localZ);    // +Y direction
    bool renderBottom = !hasCubeAt(localX, localY - 1, localZ); // -Y direction

    // Add faces one by one, calculating the correct base index for each face

    if (renderFront)
    {
        // Front face (Z+) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size()); // Changed to uint32_t
        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }

    if (renderBack)
    {
        // Back face (Z-) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }

    if (renderLeft)
    {
        // Left face (X-) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }

    if (renderRight)
    {
        // Right face (X+) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }

    if (renderTop)
    {
        // Top face (Y+) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},
            {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color},
            {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }

    if (renderBottom)
    {
        // Bottom face (Y-) vertices
        uint32_t baseIndex = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), {
            {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},
            {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},
            {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color}
            });
        indices.insert(indices.end(), {
            baseIndex + 0, baseIndex + 1, baseIndex + 2,
            baseIndex + 2, baseIndex + 3, baseIndex + 0
            });
    }
}

void Chunk::addCube(float worldX, float worldY, float worldZ, glm::vec3 color)
{
    // Get the current vertex count (for index calculation)
    uint16_t baseIndex = static_cast<uint16_t>(vertices.size());

    // Define cube vertices (8 vertices total)
    // Using consistent vertex layout for easier index calculation
    std::vector<Vertex> cubeVertices = {
        // Front face vertices (Z+)
        {{worldX - 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},  // 0: front bottom-left
        {{worldX + 0.5f, worldY - 0.5f, worldZ + 0.5f}, color},  // 1: front bottom-right
        {{worldX + 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},  // 2: front top-right
        {{worldX - 0.5f, worldY + 0.5f, worldZ + 0.5f}, color},  // 3: front top-left

        // Back face vertices (Z-)
        {{worldX - 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},  // 4: back bottom-left
        {{worldX + 0.5f, worldY - 0.5f, worldZ - 0.5f}, color},  // 5: back bottom-right
        {{worldX + 0.5f, worldY + 0.5f, worldZ - 0.5f}, color},  // 6: back top-right
        {{worldX - 0.5f, worldY + 0.5f, worldZ - 0.5f}, color},  // 7: back top-left
    };

    // Add vertices to our chunk's vertex list
    vertices.insert(vertices.end(), cubeVertices.begin(), cubeVertices.end());

    // Define indices with correct counter-clockwise winding (when viewed from outside the cube)
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