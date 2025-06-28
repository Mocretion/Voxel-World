#include "WorldRenderer.h"
#include <iostream>
#include <cmath>
#include <chrono>

WorldRenderer::WorldRenderer(VulkanRenderer* renderer)
    : vulkanRenderer(renderer), worldDataDirty(true)
{
    std::cout << "WorldRenderer initialized with render distance: " << renderDistance << " chunks" << std::endl;
}

void WorldRenderer::updateWorld(const Camera& camera)
{
    // Convert player's world position to chunk coordinates
    glm::ivec2 playerChunk = worldPositionToChunk(camera.getPosition());

    // Only update terrain if player moved to a different chunk
    if (playerChunk != lastUpdateChunk)
    {
        std::cout << "Player moved to chunk (" << playerChunk.x << ", " << playerChunk.y
            << ") from (" << lastUpdateChunk.x << ", " << lastUpdateChunk.y << ")" << std::endl;

        auto startTime = std::chrono::high_resolution_clock::now();

        currentPlayerChunk = playerChunk;

        // Generate new chunks around the player
        generateChunksAroundPlayer(playerChunk);

        // Remove chunks that are too far away
        removeDistantChunks(playerChunk);

        auto chunkTime = std::chrono::high_resolution_clock::now();
        auto chunkDuration = std::chrono::duration_cast<std::chrono::milliseconds>(chunkTime - startTime);

        // Mark mesh data as dirty so it gets uploaded to GPU
        lastUpdateChunk = playerChunk;
    }

    // OPTIMIZED: Check if any chunks still need mesh updates (this can happen incrementally)
    int chunksNeedingGeneration = chunkManager.getChunksNeedingGeneration();
    if (chunksNeedingGeneration > 0)
    {
        std::cout << "Creating chunk data for chunks...: " << chunksNeedingGeneration << std::endl;
        chunkManager.generateChunks(5);
    }

    int chunksNeedingUpdate = chunkManager.getChunksNeedingMeshUpdate();
    if (chunksNeedingGeneration == 0 && chunksNeedingUpdate > 0)
    {
        std::cout << "Updating meshes for chunks that need it...: " << chunksNeedingUpdate << std::endl;
        chunkManager.updateMeshes(1);
        worldDataDirty = true;
    }

    // Update mesh data on GPU if needed
    if (worldDataDirty)
    {
        updateMeshData();
        worldDataDirty = false;
    }
}

glm::ivec2 WorldRenderer::worldPositionToChunk(const glm::vec3& worldPos) const
{
    // Convert world coordinates to chunk coordinates
    int chunkX = static_cast<int>(std::floor(worldPos.x / Chunk::CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(worldPos.z / Chunk::CHUNK_SIZE));

    return glm::ivec2(chunkX, chunkZ);
}

void WorldRenderer::generateChunksAroundPlayer(const glm::ivec2& playerChunk)
{
    // Generate chunks in a square around the player
    int chunksGenerated = 0;
    int markedDirty = 0;

    for (int x = playerChunk.x - renderDistance - ADD_CALCULATION_DISTANCE; x <= playerChunk.x + renderDistance + ADD_CALCULATION_DISTANCE; x++)
    {
        for (int z = playerChunk.y - renderDistance - ADD_CALCULATION_DISTANCE; z <= playerChunk.y + renderDistance + ADD_CALCULATION_DISTANCE; z++)
        {
            Chunk* chunk = chunkManager.getChunk(x, z);

            // Check if this chunk does not already exists
            if (chunk == nullptr || !chunk->isMeshGenerated())
            {
				// Render only chunks within the render distance but NOT calculation distance
                bool markDirty =
                    x >= playerChunk.x - renderDistance && x <= playerChunk.x + renderDistance &&
                    z >= playerChunk.y - renderDistance && z <= playerChunk.y + renderDistance;

				std::cout << "[DEBUG] Dirty: " << markDirty << ", at: " << x << " " << z << ". Center: " << playerChunk.x << " " << playerChunk.y << std::endl;

                chunksGenerated += chunkManager.markChunkForGeneration(x, z, markDirty);
                markedDirty += markDirty;
            }
        }
    }

    if (chunksGenerated > 0)
    {
        std::cout << "Generated " << chunksGenerated << " new chunks" << std::endl;
        std::cout << "Marked " << markedDirty << " chunks for mesh generation" << std::endl;
    }
}

void WorldRenderer::removeDistantChunks(const glm::ivec2& playerChunk)
{
    int removeMeshDistance = renderDistance;
    int removeDataDistance = renderDistance + ADD_CALCULATION_DISTANCE;
    int chunksRemoved = chunkManager.removeDistantChunks(playerChunk.x, playerChunk.y, removeDataDistance, removeMeshDistance);

    if (chunksRemoved > 0)
    {
        std::cout << "Removed " << chunksRemoved << " distant chunks" << std::endl;
    }
}

void WorldRenderer::updateMeshData()
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Get all vertices and indices from all currently loaded chunks
    std::vector<Vertex> allVertices = chunkManager.getAllVertices();
    std::vector<uint32_t> allIndices = chunkManager.getAllIndices();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Collected mesh data in " << duration.count() << "ms: "
        << allVertices.size() << " vertices, "
        << allIndices.size() << " indices from " << getLoadedChunkCount() << " chunks" << std::endl;

    // Upload the mesh data to the Vulkan renderer
    vulkanRenderer->updateMeshData(allVertices, allIndices);
}

void WorldRenderer::render(const Camera& camera, float aspectRatio)
{
    // Create the uniform buffer object with camera matrices
    UniformBufferObject ubo = createUBO(camera, aspectRatio);

    // Update the renderer's uniform buffers
    vulkanRenderer->updateUniforms(ubo);
}

int WorldRenderer::getLoadedChunkCount() const
{
    return chunkManager.getLoadedChunkCount();
}

std::string WorldRenderer::getDirectionString(const glm::vec3& front) const
{
    // Determine primary direction based on the camera's front vector
    float absX = std::abs(front.x);
    float absZ = std::abs(front.z);

    if (absX > absZ)
    {
        // Facing more along X axis
        if (front.x > 0)
        {
            return "East (+X)";
        }
        else
        {
            return "West (-X)";
        }
    }
    else
    {
        // Facing more along Z axis
        if (front.z > 0)
        {
            return "South (+Z)";
        }
        else
        {
            return "North (-Z)";
        }
    }
}

UniformBufferObject WorldRenderer::createUBO(const Camera& camera, float aspectRatio) const
{
    UniformBufferObject ubo{};

    // Model matrix (identity - no transformation needed for world geometry)
    ubo.model = glm::mat4(1.0f);

    // View matrix from camera
    ubo.view = camera.getViewMatrix();

    // Projection matrix
    ubo.proj = glm::perspective(
        glm::radians(80.0f),  // Field of view
        aspectRatio,          // Aspect ratio
        0.1f,                 // Near plane
        1000.0f              // Far plane (large for voxel world)
    );

    // Vulkan has inverted Y coordinate compared to OpenGL
    ubo.proj[1][1] *= -1;

    return ubo;
}