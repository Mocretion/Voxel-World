#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

// Vertex struct definition
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position attribute
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Color attribute
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

// Uniform buffer object for transformations
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Cube vertex data (8 corners)
const std::vector<Vertex> cubeVertices = {
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // Red
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // Green
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}, // Blue
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, // Yellow
    {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}}, // Magenta
    {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}}, // Cyan
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}, // White
    {{-0.5f,  0.5f,  0.5f}, {0.5f, 0.5f, 0.5f}}  // Gray
};

// Indices for the 12 triangles (6 faces × 2 triangles each)
// Fixed winding order for Vulkan (counter-clockwise)
const std::vector<uint16_t> cubeIndices = {
    // Back face (looking at -Z)
    0, 3, 2,  2, 1, 0,
    // Front face (looking at +Z)  
    4, 5, 6,  6, 7, 4,
    // Left face (looking at -X)
    0, 4, 7,  7, 3, 0,
    // Right face (looking at +X)
    1, 2, 6,  6, 5, 1,
    // Bottom face (looking at -Y)
    0, 1, 5,  5, 4, 0,
    // Top face (looking at +Y)
    3, 7, 6,  6, 2, 3
};