#include "WorldRenderer.h"
#include <iostream>

WorldRenderer::WorldRenderer(VulkanRenderer* renderer)
    : vulkanRenderer(renderer), worldDataLoaded(false)
{
}

void WorldRenderer::generateWorld(int chunksX, int chunksZ)
{
    std::cout << "Generating world with " << chunksX << "x" << chunksZ << " chunks..." << std::endl;

    // Generate the terrain using the chunk manager
    chunkManager.generateTerrain(chunksX, chunksZ);

    // Get all vertices and indices from the chunks
    std::vector<Vertex> allVertices = chunkManager.getAllVertices();
    std::vector<uint32_t> allIndices = chunkManager.getAllIndices();

    std::cout << "Generated " << allVertices.size() << " vertices and "
        << allIndices.size() << " indices" << std::endl;

    // Upload the mesh data to the Vulkan renderer
    vulkanRenderer->updateMeshData(allVertices, allIndices);

    worldDataLoaded = true;
    std::cout << "World generation complete!" << std::endl;
}

void WorldRenderer::render(const Camera& camera, float aspectRatio)
{
    if (!worldDataLoaded)
    {
        return; // Nothing to render yet
    }

    // Create the uniform buffer object with camera matrices
    UniformBufferObject ubo = createUBO(camera, aspectRatio);

    // Update the renderer's uniform buffers
    vulkanRenderer->updateUniforms(ubo);
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
        glm::radians(45.0f),  // Field of view (45 degrees)
        aspectRatio,          // Aspect ratio
        0.1f,                 // Near plane
        1000.0f              // Far plane (large for voxel world)
    );

    // Vulkan has inverted Y coordinate compared to OpenGL
    ubo.proj[1][1] *= -1;

    return ubo;
}