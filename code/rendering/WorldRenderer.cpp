#include "WorldRenderer.h"
#include <iostream>
#include <cmath>
#include <chrono>
#include <bgfx\BgfxRenderer.h>

WorldRenderer::WorldRenderer(BgfxRenderer* renderer)  // Changed parameter type
    : bgfxRenderer(renderer)  // Changed member name
{
    std::cout << "WorldRenderer initialized with render distance: " << renderDistance << " chunks" << std::endl;
}

void WorldRenderer::updateWorld(const Camera& camera)
{
    // Convert player's world position to chunk coordinates
    glm::ivec3 playerChunk = worldPositionToChunk(camera.getPosition());

    // Only update terrain if player moved to a different chunk
    if (playerChunk != lastUpdateChunk)
    {
        std::cout << "Player moved to chunk (" << playerChunk.x << ", " << playerChunk.y << ", " << playerChunk.z
            << ") from (" << lastUpdateChunk.x << ", " << lastUpdateChunk.y << ", " << lastUpdateChunk.z << ")" << std::endl;

        currentPlayerChunk = playerChunk;

        // Generate new chunks around the player
        generateChunksAroundPlayer(playerChunk);

        // Remove chunks that are too far away
        removeDistantChunks(playerChunk);

        // Rebuild the render list when player moves to new chunk
        rebuildRenderList();

        lastUpdateChunk = playerChunk;
    }

    int chunksNeedingGeneration = chunkManager.getChunksNeedingGeneration();
    // Check if a generator is currently running, if not, start generating chunks
    if (chunkManager.getIsDoneGenerating() && chunksNeedingGeneration > 0)
    {
        time = std::chrono::high_resolution_clock::now();
        std::cout << "Creating chunk data for chunks...: " << chunksNeedingGeneration << std::endl;
        chunkManager.generateChunks();
    }

    int chunksNeedingUpdate = chunkManager.getChunksNeedingMeshUpdate();
    // Check if a mesher is currently running, and the generator is not => start meshing chunks
    if (chunkManager.getIsDoneMeshing() && chunkManager.getIsDoneGenerating() && !chunkManager.usedNewGenerationValue && chunksNeedingGeneration == 0 && chunksNeedingUpdate > 0)
    {
        auto chunkDuration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - time);
        chunkManager.usedNewGenerationValue = true;
        std::cout << "Updating meshes for chunks that need it...: " << chunksNeedingUpdate << ". Generation took: " << chunkDuration << std::endl;
        time = std::chrono::high_resolution_clock::now();
        chunkManager.updateMeshes();
    }

    // Check for newly completed chunks every frame and display them immediately
    updateIncrementalDisplay();
}

void WorldRenderer::updateIncrementalDisplay()
{
    // Get chunks that have just finished meshing
    std::vector<Chunk*> newlyCompleted = chunkManager.getNewlyCompletedChunks();

    if (!newlyCompleted.empty())
    {
        std::cout << "[INCREMENTAL] Adding " << newlyCompleted.size() << " newly completed chunks to render list" << std::endl;

        // Add newly completed chunks to our render list
        for (Chunk* chunk : newlyCompleted)
        {
            if (chunk && chunk->isMeshGenerated() && !chunk->getVertices().empty())
            {
                // Check if chunk is already in render list to avoid duplicates
                auto it = std::find(currentRenderChunks.begin(), currentRenderChunks.end(), chunk);
                if (it == currentRenderChunks.end())
                {
                    currentRenderChunks.push_back(chunk);
                }
            }
        }

        // Update the renderer with the new chunk list
        bgfxRenderer->renderChunks(currentRenderChunks);  // Changed method call

        // Mark these chunks as displayed so they won't be processed again
        chunkManager.markChunksAsDisplayed(newlyCompleted);

        std::cout << "[INCREMENTAL] Total chunks in render list: " << currentRenderChunks.size() << std::endl;
    }
}

void WorldRenderer::rebuildRenderList()
{
    auto startTime = std::chrono::high_resolution_clock::now();

    // Get all loaded chunks
    std::vector<std::tuple<int, int, int>> loadedChunkCoords = chunkManager.getLoadedChunkCoordinates();

    // Rebuild the render list from scratch
    currentRenderChunks.clear();
    currentRenderChunks.reserve(loadedChunkCoords.size());

    int totalChunks = 0;
    int renderableChunks = 0;

    for (const auto& coords : loadedChunkCoords)
    {
        int chunkX = std::get<0>(coords);
        int chunkY = std::get<1>(coords);
        int chunkZ = std::get<2>(coords);

        Chunk* chunk = chunkManager.getChunk(chunkX, chunkY, chunkZ);
        totalChunks++;

        if (chunk && chunk->isMeshGenerated() && !chunk->getVertices().empty())
        {
            currentRenderChunks.push_back(chunk);
            renderableChunks++;

            // Mark as displayed since we're adding it to render list
            chunk->markAsDisplayed();
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "[REBUILD] Rebuilt chunk rendering list in " << duration.count() << "ms: "
        << renderableChunks << "/" << totalChunks << " chunks ready to render" << std::endl;

    // Send chunk list to renderer
    bgfxRenderer->renderChunks(currentRenderChunks);  // Changed method call
}

glm::ivec3 WorldRenderer::worldPositionToChunk(const glm::vec3& worldPos) const
{
    // Convert world coordinates to chunk coordinates
    int chunkX = static_cast<int>(std::floor(worldPos.x / Chunk::CHUNK_SIZE));
    int chunkY = static_cast<int>(std::floor(worldPos.y / Chunk::CHUNK_SIZE));
    int chunkZ = static_cast<int>(std::floor(worldPos.z / Chunk::CHUNK_SIZE));

    return glm::ivec3(chunkX, chunkY, chunkZ);
}

void WorldRenderer::generateChunksAroundPlayer(const glm::ivec3& playerChunk)
{
    // Generate chunks in a square around the player

    std::vector<std::tuple<int, int, int>> chunksToGenerate;
    std::vector<std::tuple<int, int, int>> chunksToMesh;

    for (int x = playerChunk.x - renderDistance - ADD_CALCULATION_DISTANCE; x <= playerChunk.x + renderDistance + ADD_CALCULATION_DISTANCE; x++)
    {
        for (int y = playerChunk.y - renderDistance - ADD_CALCULATION_DISTANCE; y <= playerChunk.y + renderDistance + ADD_CALCULATION_DISTANCE; y++)
        {
            for (int z = playerChunk.z - renderDistance - ADD_CALCULATION_DISTANCE; z <= playerChunk.z + renderDistance + ADD_CALCULATION_DISTANCE; z++)
            {
                Chunk* chunk = chunkManager.getChunk(x, y, z);

                bool inMeshingRange =
                    x >= playerChunk.x - renderDistance && x <= playerChunk.x + renderDistance &&
                    y >= playerChunk.y - renderDistance && y <= playerChunk.y + renderDistance &&
                    z >= playerChunk.z - renderDistance && z <= playerChunk.z + renderDistance;

                // Check if this chunk does not already exists
                if (chunk == nullptr)
                {
                    // Render only chunks within the render distance but NOT calculation distance
                    chunksToGenerate.emplace_back(x, y, z);
                }

                if ((chunk == nullptr || !chunk->isMeshGenerated()) && inMeshingRange)  // Chunk also needs meshing
                {
                    chunksToMesh.emplace_back(x, y, z);
                }
            }
        }
    }

    if (!chunksToGenerate.empty())
    {
        chunkManager.markChunksForGeneration(chunksToGenerate);
    }

    if (!chunksToMesh.empty())
    {
        chunkManager.markChunksForMeshing(chunksToMesh);
    }
}

void WorldRenderer::removeDistantChunks(const glm::ivec3& playerChunk)
{
    int removeMeshDistance = renderDistance;
    int removeDataDistance = renderDistance + ADD_CALCULATION_DISTANCE;

    // Note: bgfx cleanup will be handled in Chunk::cleanupBgfxResources()
    int chunksRemoved = chunkManager.removeDistantChunks(
        playerChunk.x, playerChunk.y, playerChunk.z,
        removeDataDistance, removeMeshDistance
    );

    if (chunksRemoved > 0)
    {
        rebuildRenderList();
        std::cout << "Removed " << chunksRemoved << " distant chunks" << std::endl;
    }
}

void WorldRenderer::render(const Camera& camera, float aspectRatio)
{
    // Create the uniform buffer object with camera matrices
    UniformBufferObject ubo = createUBO(camera, aspectRatio);

    // Update the renderer's uniform buffers
    bgfxRenderer->updateUniforms(ubo);  // Changed method call
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

    // bgfx coordinate system might need adjustment
    // ubo.proj[1][1] *= -1; // Only needed for Vulkan

    return ubo;
}