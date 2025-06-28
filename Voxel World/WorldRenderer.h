#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "Chunk.h"
#include "Camera.h"
#include "VulkanRenderer.h"

class WorldRenderer {
public:
    WorldRenderer(VulkanRenderer* renderer);

    // NEW: Set the render distance (in chunks)
    void setRenderDistance(int distance) { renderDistance = distance; }
    int getRenderDistance() const { return renderDistance; }

    // NEW: Update world based on player position - this replaces generateWorld
    void updateWorld(const Camera& camera);

    // Render the world with the given camera
    void render(const Camera& camera, float aspectRatio);

    // Get direction string for HUD
    std::string getDirectionString(const glm::vec3& front) const;

    // NEW: Get current statistics for debugging
    int getLoadedChunkCount() const;
    glm::ivec2 getCurrentPlayerChunk() const { return currentPlayerChunk; }

    //int getPendingMeshJobs() const { return chunkManager.getPendingMeshJobs(); }

private:
	const int CHUNK_GEN_PER_FRAME = 5;  // How many chunks to generate per frame
	const int CHUNK_MESH_UPDATE_PER_FRAME = 1;  // How many chunks to update meshes per frame

    VulkanRenderer* vulkanRenderer;
    ChunkManager chunkManager;

    // NEW: Infinite terrain settings
    int renderDistance = 3;  // How many chunks in each direction to keep loaded
	const int ADD_CALCULATION_DISTANCE = 2;  // How many chunks to calculate around the player
    glm::ivec2 currentPlayerChunk = glm::ivec2(0, 0);  // Which chunk the player is currently in
    glm::ivec2 lastUpdateChunk = glm::ivec2(INT_MAX, INT_MAX);  // Last chunk where we updated terrain

    // Cache for mesh data to avoid regenerating every frame
    bool worldDataDirty = true;  // Track if we need to regenerate mesh data

    // Helper methods
    UniformBufferObject createUBO(const Camera& camera, float aspectRatio) const;

    // NEW: Infinite terrain helpers
    glm::ivec2 worldPositionToChunk(const glm::vec3& worldPos) const;
    void generateChunksAroundPlayer(const glm::ivec2& playerChunk);
    void removeDistantChunks(const glm::ivec2& playerChunk);
    void updateMeshData();
};