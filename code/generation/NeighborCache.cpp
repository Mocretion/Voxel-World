#include "Chunk.h"
#include "ChunkManager.h"
#include <cstring>

bool NeighborCache::hasCubeAtWorld(int worldX, int worldY, int worldZ, const Chunk* centerChunk) const
{
    // Convert world coordinates to chunk coordinates
    int chunkX = worldX / Chunk::CHUNK_SIZE;
    int chunkY = worldY / Chunk::CHUNK_SIZE;
    int chunkZ = worldZ / Chunk::CHUNK_SIZE;

    // Handle negative coordinates properly
    if (worldX < 0 && worldX % Chunk::CHUNK_SIZE != 0) chunkX--;
    if (worldY < 0 && worldY % Chunk::CHUNK_SIZE != 0) chunkY--;
    if (worldZ < 0 && worldZ % Chunk::CHUNK_SIZE != 0) chunkZ--;

    // Calculate local coordinates within the chunk
    int localX = worldX - (chunkX * Chunk::CHUNK_SIZE);
    int localY = worldY - (chunkY * Chunk::CHUNK_SIZE);
    int localZ = worldZ - (chunkZ * Chunk::CHUNK_SIZE);

    // Check if this is the center chunk (the one being meshed)
    if (chunkX == centerChunkX && chunkY == centerChunkY && chunkZ == centerChunkZ)
    {
        return centerChunk->hasCubeAt(localX, localY, localZ);
    }

    // Calculate neighbor index (0, 1, or 2 for each axis)
    int dx = chunkX - centerChunkX + 1;  // -1,0,1 -> 0,1,2
    int dy = chunkY - centerChunkY + 1;
    int dz = chunkZ - centerChunkZ + 1;

    // Check if the neighbor is within our cache range
    if (dx < 0 || dx > 2 || dy < 0 || dy > 2 || dz < 0 || dz > 2)
    {
        return false;  // Outside cache range
    }

    const BorderData& neighbor = neighbors[dx][dy][dz];
    if (!neighbor.hasData)
    {
        return false;  // Neighbor chunk doesn't exist
    }

    // Now we need to check the appropriate border array based on which face we're accessing
    // We only cache border blocks, so check if we're accessing a border

    // For X neighbors
    if (dx == 0)
    {  // Left neighbor (-X)
        if (localX == Chunk::CHUNK_SIZE - 1)
        {  // Right border of left neighbor
            return neighbor.rightFace[localY][localZ];
        }
    }
    else if (dx == 2)
    {  // Right neighbor (+X)
        if (localX == 0)
        {  // Left border of right neighbor
            return neighbor.leftFace[localY][localZ];
        }
    }

    // For Y neighbors
    if (dy == 0)
    {  // Bottom neighbor (-Y)
        if (localY == Chunk::CHUNK_SIZE - 1)
        {  // Top border of bottom neighbor
            return neighbor.topFace[localX][localZ];
        }
    }
    else if (dy == 2)
    {  // Top neighbor (+Y)
        if (localY == 0)
        {  // Bottom border of top neighbor
            return neighbor.bottomFace[localX][localZ];
        }
    }

    // For Z neighbors
    if (dz == 0)
    {  // Back neighbor (-Z)
        if (localZ == Chunk::CHUNK_SIZE - 1)
        {  // Front border of back neighbor
            return neighbor.frontFace[localX][localY];
        }
    }
    else if (dz == 2)
    {  // Front neighbor (+Z)
        if (localZ == 0)
        {  // Back border of front neighbor
            return neighbor.backFace[localX][localY];
        }
    }

    // If we're not accessing a border block, we can't use the cache
    // This shouldn't happen during normal meshing as we only check immediate neighbors
    return false;
}

void NeighborCache::populateFromManager(ChunkManager* manager, int chunkX, int chunkY, int chunkZ)
{
    centerChunkX = chunkX;
    centerChunkY = chunkY;
    centerChunkZ = chunkZ;

    // Clear existing data
    clear();

    // Use a read lock to fetch neighbor data
    std::shared_lock<std::shared_mutex> lock(manager->chunkMutex);

    // Iterate through all 26 possible neighbors (and self)
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            for (int dz = -1; dz <= 1; dz++)
            {
                if (dx == 0 && dy == 0 && dz == 0) continue;  // Skip self

                int neighborX = chunkX + dx;
                int neighborY = chunkY + dy;
                int neighborZ = chunkZ + dz;

                Chunk* neighborChunk = manager->getChunk(neighborX, neighborY, neighborZ);
                if (neighborChunk == nullptr)
                {
                    neighbors[dx + 1][dy + 1][dz + 1].hasData = false;
                    continue;
                }

                BorderData& border = neighbors[dx + 1][dy + 1][dz + 1];
                border.hasData = true;

                // Copy only the border blocks we need
                // Left face (-X border, x=0)
                for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
                {
                    for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
                    {
                        border.leftFace[y][z] = neighborChunk->hasCubeAt(0, y, z);
                    }
                }

                // Right face (+X border, x=CHUNK_SIZE-1)
                for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
                {
                    for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
                    {
                        border.rightFace[y][z] = neighborChunk->hasCubeAt(Chunk::CHUNK_SIZE - 1, y, z);
                    }
                }

                // Bottom face (-Y border, y=0)
                for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
                {
                    for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
                    {
                        border.bottomFace[x][z] = neighborChunk->hasCubeAt(x, 0, z);
                    }
                }

                // Top face (+Y border, y=MAX_HEIGHT-1)
                for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
                {
                    for (int z = 0; z < Chunk::CHUNK_SIZE; z++)
                    {
                        border.topFace[x][z] = neighborChunk->hasCubeAt(x, Chunk::CHUNK_SIZE - 1, z);
                    }
                }

                // Back face (-Z border, z=0)
                for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
                {
                    for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
                    {
                        border.backFace[x][y] = neighborChunk->hasCubeAt(x, y, 0);
                    }
                }

                // Front face (+Z border, z=CHUNK_SIZE-1)
                for (int x = 0; x < Chunk::CHUNK_SIZE; x++)
                {
                    for (int y = 0; y < Chunk::CHUNK_SIZE; y++)
                    {
                        border.frontFace[x][y] = neighborChunk->hasCubeAt(x, y, Chunk::CHUNK_SIZE - 1);
                    }
                }
            }
        }
    }
}

void NeighborCache::clear()
{
    // Reset all neighbor data
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            for (int k = 0; k < 3; k++)
            {
                neighbors[i][j][k].hasData = false;
            }
        }
    }
}