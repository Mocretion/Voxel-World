#include "Chunk.h"
#include <random>

// Define the cube size (1.0f means each cube is 1 unit)
const float Chunk::CUBE_SIZE = 1.0f;

// Chunk Constructor
Chunk::Chunk(int chunkX, int chunkZ) : chunkX(chunkX), chunkZ(chunkZ)
{
    // Initialize the 2D vector - like creating new bool[32][32] in C#
    cubes.resize(CHUNK_SIZE);
    for (int i = 0; i < CHUNK_SIZE; i++)
    {
        cubes[i].resize(CHUNK_SIZE, true);  // true = cube exists, false = air
    }

    // Generate the mesh data
    generateMesh();
}

bool Chunk::hasCubeAt(int x, int z) const
{
    if (x < 0 || x >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
    {
        return false;  // Out of bounds
    }
    return cubes[x][z];
}

void Chunk::setCubeAt(int x, int z, bool exists)
{
    if (x >= 0 && x < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE)
    {
        cubes[x][z] = exists;
    }
}

void Chunk::generateMesh()
{
    // Clear previous data
    vertices.clear();
    indices.clear();

    // Random number generator for colors
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> colorDist(0.3f, 1.0f);

    // Loop through all positions in the chunk
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            if (hasCubeAt(x, z))
            {
                // Calculate world position
                float worldX = (chunkX * CHUNK_SIZE + x) * CUBE_SIZE;
                float worldY = 0.0f;  // Floor level
                float worldZ = (chunkZ * CHUNK_SIZE + z) * CUBE_SIZE;

                // Generate a random color for each cube
                glm::vec3 color(colorDist(gen), colorDist(gen), colorDist(gen));

                // Only render faces that are visible (not blocked by neighbors)
                addCubeWithCulling(worldX, worldY, worldZ, color, x, z);
            }
        }
    }
}

void Chunk::addCubeWithCulling(float worldX, float worldY, float worldZ, glm::vec3 color, int localX, int localZ)
{
    // Check which faces should be rendered based on neighbors
    bool renderFront = !hasCubeAt(localX, localZ + 1);  // +Z direction
    bool renderBack = !hasCubeAt(localX, localZ - 1);   // -Z direction
    bool renderRight = !hasCubeAt(localX + 1, localZ);  // +X direction
    bool renderLeft = !hasCubeAt(localX - 1, localZ);   // -X direction
    bool renderTop = true;    // Always render top (no cubes above in a floor)
    bool renderBottom = true; // Always render bottom for now (you might want to change this)

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

// ChunkManager Implementation
void ChunkManager::generateFloor(int chunksX, int chunksZ)
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
            chunks.emplace_back(chunkX, chunkZ);
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