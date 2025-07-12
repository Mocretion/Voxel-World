#include "Chunk.h"
#include "ChunkManager.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>

// Define the cube size
const float Chunk::CUBE_SIZE = 1.0f;

Chunk::Chunk(int chunkX, int chunkY, int chunkZ, PerlinNoise* noise)
    : chunkX(chunkX), chunkY(chunkY), chunkZ(chunkZ), noiseGenerator(noise)
{
    // Zero out arrays super fast
    std::memset(cubes, false, sizeof(cubes));

    // Reserve space for mesh data - larger chunks will need more space
    vertices.reserve(2000);
    indices.reserve(3000);
}

void Chunk::generateTerrain(PerlinNoise* noise)
{
    if (noise)
    {
        noiseGenerator = noise;
        generate3DTerrain();
    }
}

void Chunk::generate3DTerrain()
{
    int surfaceHeight[CHUNK_SIZE][CHUNK_SIZE];

    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        int worldX = chunkX * CHUNK_SIZE + x;  // Convert local coordinates to world coordinates

        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            int worldZ = chunkZ * CHUNK_SIZE + z;
            float n = noiseGenerator->octaveNoise2D(worldX * 0.01f, worldZ * 0.01f, 6, 0.5f, 1.0f);
            surfaceHeight[x][z] = static_cast<int>(32 + n * 48);

        }
    }

    // Generate 3D terrain based on world coordinates
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        int worldX = chunkX * CHUNK_SIZE + x;  // Convert local coordinates to world coordinates

        for (int y = 0; y < CHUNK_SIZE; y++)
        {
            int worldY = chunkY * CHUNK_SIZE + y;

            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                int worldZ = chunkZ * CHUNK_SIZE + z;

                // Determine what material should be at this world position
                bool shouldHaveCube = false;

                if (worldY < surfaceHeight[x][z] - 5)
                {
                    // Deep underground - solid stone
                    shouldHaveCube = true;
                }
                else if (worldY < surfaceHeight[x][z])
                {
                    // Near surface - dirt/stone
                    shouldHaveCube = true;
                }
                else if (worldY == surfaceHeight[x][z])
                {
                    // Surface level - grass/sand
                    shouldHaveCube = true;
                }
                else
                {
                    // Above surface - air (with potential caves/structures)
                    shouldHaveCube = false;

                    // Add some floating islands or structures for chunks high above surface
                    if (worldY > surfaceHeight[x][z] + 100)
                    {
                        float islandNoise = noiseGenerator->octaveNoise3D(
                            worldX * 0.02f, worldY * 0.02f, worldZ * 0.02f, 4, 0.6f, 1.0f);
                        shouldHaveCube = islandNoise > 0.3f;
                    }
                }

                // Add cave systems in underground chunks
                if (shouldHaveCube && worldY < surfaceHeight[x][z] - 10)
                {
                    float caveNoise = noiseGenerator->octaveNoise3D(
                        worldX * 0.03f, worldY * 0.03f, worldZ * 0.03f, 3, 0.5f, 1.0f);

                    if (caveNoise > 0.4f)
                    {
                        shouldHaveCube = false; // Create cave
                    }
                }

                cubes[x][y][z] = shouldHaveCube;
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
        return false;
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

glm::vec3 Chunk::getTerrainColor(int worldY) const
{
    if (worldY < 0)
    {
        return glm::vec3(0.4f, 0.3f, 0.2f); // Deep underground - dark brown/stone
    }
    else if (worldY < 32)
    {
        return glm::vec3(0.6f, 0.4f, 0.2f); // Underground - brown dirt/stone
    }
    else if (worldY < 80)
    {
        return glm::vec3(0.095f, 0.871f, 0.302f); // Surface - green grass
    }
    else if (worldY < 120)
    {
        return glm::vec3(0.5f, 0.5f, 0.5f); // Mountain - gray stone
    }
    else
    {
        return glm::vec3(0.9f, 0.9f, 0.95f); // High altitude - white snow/clouds
    }
}

void Chunk::localToWorld(int localX, int localY, int localZ, int& worldX, int& worldY, int& worldZ) const
{
    worldX = chunkX * CHUNK_SIZE + localX;
    worldY = chunkY * CHUNK_SIZE + localY;
    worldZ = chunkZ * CHUNK_SIZE + localZ;
}

bool Chunk::hasCubeAtWorld(int worldX, int worldY, int worldZ, ChunkManager* manager) const
{
    if (manager == nullptr)
    {
        int localX = worldX - (chunkX * CHUNK_SIZE);
        int localY = worldY - (chunkY * CHUNK_SIZE);
        int localZ = worldZ - (chunkZ * CHUNK_SIZE);
        return hasCubeAt(localX, localY, localZ);
    }
    return manager->hasCubeAtWorld(worldX, worldY, worldZ);
}

// === MAIN MESH GENERATION WITH GREEDY MESHING ===

void Chunk::generateMeshWithManager(ChunkManager* manager, bool debug)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    vertices.clear();
    indices.clear();
    vertices.reserve(10000);  // Increased reserve for larger chunks
    indices.reserve(15000);   // Increased reserve for larger chunks

    // Use greedy meshing instead of per-cube meshing
    generateGreedyMesh(manager, debug);

    setMeshGenerated(true);

    auto endTime = std::chrono::high_resolution_clock::now();
    if (debug)
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        std::cout << "[GREEDY] Chunk (" << chunkX << ", " << chunkY << ", " << chunkZ
            << ") meshed in " << duration.count() << "ms with "
            << vertices.size() << " vertices (64x64x64 chunk)" << std::endl;
    }
}

void Chunk::generateGreedyMesh(ChunkManager* manager, bool debug)
{
    std::vector<Quad> allQuads;
    allQuads.reserve(2000); // Increased pre-allocation for larger chunks

    // Generate quads for each axis and direction
    // X-axis: Left (-X) and Right (+X) faces
    generateQuadsForAxis(0, -1, manager, allQuads); // Left faces
    generateQuadsForAxis(0, +1, manager, allQuads); // Right faces

    // Y-axis: Bottom (-Y) and Top (+Y) faces  
    generateQuadsForAxis(1, -1, manager, allQuads); // Bottom faces
    generateQuadsForAxis(1, +1, manager, allQuads); // Top faces

    // Z-axis: Back (-Z) and Front (+Z) faces
    generateQuadsForAxis(2, -1, manager, allQuads); // Back faces
    generateQuadsForAxis(2, +1, manager, allQuads); // Front faces

    if (debug)
    {
        std::cout << "[GREEDY] Generated " << allQuads.size() << " quads for 64x64x64 chunk ("
            << chunkX << ", " << chunkY << ", " << chunkZ << ")" << std::endl;
    }

    // Convert all quads to actual mesh data
    convertQuadsToMesh(allQuads);
}

void Chunk::generateQuadsForAxis(int axis, int direction, ChunkManager* manager, std::vector<Quad>& quads)
{
    // Create a 3D processed array to track which faces we've already included in quads
    std::vector<std::vector<std::vector<bool>>> processed(
        CHUNK_SIZE, std::vector<std::vector<bool>>(
            MAX_HEIGHT, std::vector<bool>(CHUNK_SIZE, false)));

    // Iterate through all possible positions in the chunk
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
        for (int y = 0; y < MAX_HEIGHT; y++)
        {
            for (int z = 0; z < CHUNK_SIZE; z++)
            {
                // Check if this position has a cube and we haven't processed this face yet
                if (!hasCubeAt(x, y, z) || processed[x][y][z])
                    continue;

                // Check if this face should be rendered (not blocked by neighbor)
                if (!shouldRenderFace(x, y, z, axis * 2 + (direction > 0 ? 1 : 0), manager))
                    continue;

                // Get the color for this face
                glm::vec3 color = getFaceColor(x, y, z, axis * 2 + (direction > 0 ? 1 : 0));

                // Try to expand this face into a larger quad (the "greedy" part!)
                glm::vec2 quadSize = expandQuad(axis, direction, x, y, z, color, manager, processed);

                // Create the quad with CORRECT normal direction
                glm::vec3 normal;
                if (axis == 0) normal = glm::vec3(direction, 0, 0);      // X-axis
                else if (axis == 1) normal = glm::vec3(0, direction, 0); // Y-axis
                else normal = glm::vec3(0, 0, direction);                // Z-axis

                // Calculate world position of bottom-left corner of the quad
                float worldX = (chunkX * CHUNK_SIZE + x) * CUBE_SIZE;
                float worldY = (chunkY * CHUNK_SIZE + y) * CUBE_SIZE;
                float worldZ = (chunkZ * CHUNK_SIZE + z) * CUBE_SIZE;

                // Adjust position based on which face we're rendering
                glm::vec3 position(worldX, worldY, worldZ);
                if (axis == 0 && direction > 0) position.x += CUBE_SIZE; // Right face
                if (axis == 1 && direction > 0) position.y += CUBE_SIZE; // Top face
                if (axis == 2 && direction > 0) position.z += CUBE_SIZE; // Front face       

                quads.emplace_back(position, quadSize, normal, color, axis);
            }
        }
    }
}

bool Chunk::shouldRenderFace(int x, int y, int z, int direction, ChunkManager* manager) const
{
    // Direction mapping: 0=left(-X), 1=right(+X), 2=bottom(-Y), 3=top(+Y), 4=back(-Z), 5=front(+Z)
    int worldX, worldY, worldZ;
    localToWorld(x, y, z, worldX, worldY, worldZ);

    int neighborX = worldX, neighborY = worldY, neighborZ = worldZ;

    switch (direction)
    {
    case 0: neighborX--; break; // Left face - check left neighbor
    case 1: neighborX++; break; // Right face - check right neighbor
    case 2: neighborY--; break; // Bottom face - check bottom neighbor
    case 3: neighborY++; break; // Top face - check top neighbor
    case 4: neighborZ--; break; // Back face - check back neighbor
    case 5: neighborZ++; break; // Front face - check front neighbor
    }

    // Face should be rendered if the neighbor position is empty
    return !manager->hasCubeAtWorld(neighborX, neighborY, neighborZ);
}

glm::vec3 Chunk::getFaceColor(int x, int y, int z, int direction) const
{
    // Calculate world Y position for color determination
    int worldY = chunkY * CHUNK_SIZE + y;

    // Get color based on world Y position (altitude-based coloring)
    glm::vec3 baseColor = getTerrainColor(worldY);

    return baseColor;
}

glm::vec2 Chunk::expandQuad(int axis, int direction, int x, int y, int z,
    const glm::vec3& color, ChunkManager* manager,
    std::vector<std::vector<std::vector<bool>>>& processed)
{
    // This is the "greedy" part - we try to make the biggest possible quad
    // by expanding in both U and V directions

    int u, v, w;
    getAxisCoords(axis, x, y, z, u, v, w);

    // First expand in U direction (width)
    int maxU = u;
    for (int testU = u + 1; testU < CHUNK_SIZE; testU++)
    {
        int testX, testY, testZ;
        setAxisCoords(axis, testU, v, w, testX, testY, testZ);

        // Check bounds
        if (testX >= CHUNK_SIZE || testY >= MAX_HEIGHT || testZ >= CHUNK_SIZE)
            break;

        // Check if this position is valid for our quad
        if (!hasCubeAt(testX, testY, testZ) ||
            processed[testX][testY][testZ] ||
            !shouldRenderFace(testX, testY, testZ, axis * 2 + (direction > 0 ? 1 : 0), manager) ||
            getFaceColor(testX, testY, testZ, axis * 2 + (direction > 0 ? 1 : 0)) != color)
        {
            break;
        }

        maxU = testU;
    }

    // Then expand in V direction (height), but only for the width we found
    int maxV = v;
    bool canExpandV = true;

    for (int testV = v + 1; testV < (axis == 1 ? MAX_HEIGHT : CHUNK_SIZE) && canExpandV; testV++)
    {
        // Check the entire row in U direction
        for (int testU = u; testU <= maxU; testU++)
        {
            int testX, testY, testZ;
            setAxisCoords(axis, testU, testV, w, testX, testY, testZ);

            // Check bounds
            if (testX >= CHUNK_SIZE || testY >= MAX_HEIGHT || testZ >= CHUNK_SIZE)
            {
                canExpandV = false;
                break;
            }

            // Check if this position is valid for our quad
            if (!hasCubeAt(testX, testY, testZ) ||
                processed[testX][testY][testZ] ||
                !shouldRenderFace(testX, testY, testZ, axis * 2 + (direction > 0 ? 1 : 0), manager) ||
                getFaceColor(testX, testY, testZ, axis * 2 + (direction > 0 ? 1 : 0)) != color)
            {
                canExpandV = false;
                break;
            }
        }

        if (canExpandV)
        {
            maxV = testV;
        }
    }

    // Mark all faces in this quad as processed
    for (int testU = u; testU <= maxU; testU++)
    {
        for (int testV = v; testV <= maxV; testV++)
        {
            int testX, testY, testZ;
            setAxisCoords(axis, testU, testV, w, testX, testY, testZ);
            processed[testX][testY][testZ] = true;
        }
    }

    // Return the size of the quad
    float width = (maxU - u + 1) * CUBE_SIZE;
    float height = (maxV - v + 1) * CUBE_SIZE;

    return glm::vec2(width, height);
}

void Chunk::getAxisCoords(int axis, int x, int y, int z, int& u, int& v, int& w) const
{
    // Convert 3D coordinates to 2D coordinates for the given axis
    switch (axis)
    {
    case 0: // X-axis (YZ plane)
        u = z; v = y; w = x;
        break;
    case 1: // Y-axis (XZ plane)
        u = x; v = z; w = y;
        break;
    case 2: // Z-axis (XY plane)
        u = x; v = y; w = z;
        break;
    }
}

void Chunk::setAxisCoords(int axis, int u, int v, int w, int& x, int& y, int& z) const
{
    // Convert 2D coordinates back to 3D coordinates
    switch (axis)
    {
    case 0: // X-axis
        x = w; y = v; z = u;
        break;
    case 1: // Y-axis
        x = u; y = w; z = v;
        break;
    case 2: // Z-axis
        x = u; y = v; z = w;
        break;
    }
}

void Chunk::convertQuadsToMesh(const std::vector<Quad>& quads)
{
    for (const Quad& quad : quads)
    {
        addQuadToMesh(quad);
    }
}

void Chunk::addQuadToMesh(const Quad& quad)
{
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    // Create 4 vertices for the quad based on its orientation
    glm::vec3 pos = quad.position;
    glm::vec2 size = quad.size;
    glm::vec3 normal = quad.normal;

    // Calculate the 4 corners of the quad based on the axis and direction
    glm::vec3 v0, v1, v2, v3;

    if (quad.axis == 0) // X-axis faces (Left/Right - YZ plane)
    {
        if (normal.x > 0) // Right face (+X)
        {
            v0 = pos + glm::vec3(0, 0, size.x);     // bottom-front  
            v1 = pos + glm::vec3(0, 0, 0);          // bottom-back
            v2 = pos + glm::vec3(0, size.y, 0);     // top-back
            v3 = pos + glm::vec3(0, size.y, size.x); // top-front
        }
        else // Left face (-X)
        {
            v0 = pos + glm::vec3(0, 0, 0);          // bottom-back
            v1 = pos + glm::vec3(0, 0, size.x);     // bottom-front
            v2 = pos + glm::vec3(0, size.y, size.x); // top-front
            v3 = pos + glm::vec3(0, size.y, 0);     // top-back
        }
    }
    else if (quad.axis == 1) // Y-axis faces (Bottom/Top - XZ plane)  
    {
        if (normal.y > 0) // Top face (+Y)
        {
            v0 = pos + glm::vec3(size.x, 0, 0);     // back-right
            v1 = pos + glm::vec3(0, 0, 0);          // back-left
            v2 = pos + glm::vec3(0, 0, size.y);     // front-left
            v3 = pos + glm::vec3(size.x, 0, size.y); // front-right
        }
        else // Bottom face (-Y)
        {
            v0 = pos + glm::vec3(size.x, 0, size.y); // front-right
            v1 = pos + glm::vec3(0, 0, size.y);     // front-left
            v2 = pos + glm::vec3(0, 0, 0);          // back-left
            v3 = pos + glm::vec3(size.x, 0, 0);     // back-right
        }
    }
    else // Z-axis faces (Back/Front - XY plane)
    {
        if (normal.z > 0) // Front face (+Z)
        {
            v0 = pos + glm::vec3(0, 0, 0);          // bottom-left
            v1 = pos + glm::vec3(size.x, 0, 0);     // bottom-right
            v2 = pos + glm::vec3(size.x, size.y, 0); // top-right
            v3 = pos + glm::vec3(0, size.y, 0);     // top-left
        }
        else // Back face (-Z)
        {
            v0 = pos + glm::vec3(size.x, 0, 0);     // bottom-right
            v1 = pos + glm::vec3(0, 0, 0);          // bottom-left
            v2 = pos + glm::vec3(0, size.y, 0);     // top-left
            v3 = pos + glm::vec3(size.x, size.y, 0); // top-right
        }
    }

    // Add vertices
    vertices.push_back({ v0, quad.color, normal });
    vertices.push_back({ v1, quad.color, normal });
    vertices.push_back({ v2, quad.color, normal });
    vertices.push_back({ v3, quad.color, normal });

    // Add indices for two triangles forming the quad (counter-clockwise winding)
    indices.insert(indices.end(), {
        baseIndex + 0, baseIndex + 1, baseIndex + 2,
        baseIndex + 2, baseIndex + 3, baseIndex + 0
        });
}