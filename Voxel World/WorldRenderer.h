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

    // Generate and load the world
    void generateWorld(int chunksX = 5, int chunksZ = 5);

    // Render the world with the given camera
    void render(const Camera& camera, float aspectRatio);

    // Get direction string for HUD
    std::string getDirectionString(const glm::vec3& front) const;

private:
    VulkanRenderer* vulkanRenderer;
    ChunkManager chunkManager;

    // Cache for mesh data to avoid regenerating every frame
    bool worldDataLoaded = false;

    // Helper method to create matrices
    UniformBufferObject createUBO(const Camera& camera, float aspectRatio) const;
};