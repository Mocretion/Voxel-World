#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "Chunk.h"
#include "ChunkManager.h"
#include "Camera.h"
#include "BgfxRenderer.h"  // Changed from VulkanRenderer
#include <chrono>

class WorldRenderer {
public:
    WorldRenderer(BgfxRenderer* renderer);  // Changed from VulkanRenderer

    // Set the render distance (in chunks)
    void setRenderDistance(int distance) { renderDistance = distance; }
    int getRenderDistance() const { return renderDistance; }

    // Update world based on player position - this replaces generateWorld
    void updateWorld(const Camera& camera);

    // Render the world with the given camera
    void render(const Camera& camera, float aspectRatio);

    // Get direction string for HUD
    std::string getDirectionString(const glm::vec3& front) const;

    // Get current statistics for debugging
    int getLoadedChunkCount() const;
    glm::ivec2 getCurrentPlayerChunk() const { return currentPlayerChunk; }

private:
    BgfxRenderer* bgfxRenderer;  // Changed from vulkanRenderer
    ChunkManager chunkManager;

    // Infinite terrain settings
    int renderDistance = 1;  // How many chunks in each direction to keep loaded
    const int ADD_CALCULATION_DISTANCE = 2;  // How many chunks to calculate around the player
    glm::ivec3 currentPlayerChunk = glm::ivec3(0, 0, 0);  // Which chunk the player is currently in
    glm::ivec3 lastUpdateChunk = glm::ivec3(INT_MAX, INT_MAX, INT_MAX);  // Last chunk where we updated terrain

    // Current chunks being rendered (maintained incrementally)
    std::vector<Chunk*> currentRenderChunks;

    // Helper methods
    UniformBufferObject createUBO(const Camera& camera, float aspectRatio) const;

    // Infinite terrain helpers
    glm::ivec3 worldPositionToChunk(const glm::vec3& worldPos) const;
    void generateChunksAroundPlayer(const glm::ivec3& playerChunk);
    void removeDistantChunks(const glm::ivec3& playerChunk);

    // Incremental display methods
    void updateIncrementalDisplay();
    void rebuildRenderList();

    std::chrono::high_resolution_clock::time_point time = std::chrono::high_resolution_clock::now();
};