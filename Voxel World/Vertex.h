#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

struct Vertex {
    glm::vec3 position;  // 3 floats x 4 bytes => 12 bytes
    glm::vec3 color;  // 3 floats x 4 bytes => 12 bytes

    // These methods can be defined right in the header since they're simple
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;  // Vertex Buffer 0
        bindingDescription.stride = sizeof(Vertex);  // Each vertex has this size
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // Get new data for each vertex
        return bindingDescription;
    }

    // Maps struct to shader inputs
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position attribute
        attributeDescriptions[0].binding = 0;  // Read from buffer 0
        attributeDescriptions[0].location = 0;  // Go to shader location 0
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // 3 floats
        attributeDescriptions[0].offset = offsetof(Vertex, position);  // Start at byte 0

        // Color attribute
        attributeDescriptions[1].binding = 0;  // Read from buffer 0
        attributeDescriptions[1].location = 1;  // Go to shader location 0
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  // 3 floats
        attributeDescriptions[1].offset = offsetof(Vertex, color);  // Start at byte 0

        return attributeDescriptions;
    }
};

// Uniform buffer object
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Cube vertex data - declared here, but you could move to .cpp if it gets large
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

const std::vector<uint16_t> cubeIndices = {
    0, 1, 2, 2, 3, 0, // Back face
    4, 5, 6, 6, 7, 4, // Front face
    0, 1, 5, 5, 4, 0, // Bottom face
    2, 3, 7, 7, 6, 2, // Top face
    0, 3, 7, 7, 4, 0, // Left face
    1, 2, 6, 6, 5, 1  // Right face
};