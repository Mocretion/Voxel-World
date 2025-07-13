#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

// Uniform Buffer Object for shader uniforms
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Vertex structure for our 3D cubes (now with normals!)
struct Vertex {
    uint32_t dataBits;  // 3x7: 0 - 20: POSITION, 21 - 23: NORMAL
    uint32_t colorBits;  // 3x8: 0 - 23: COLOR

    // Tell Vulkan how to pass vertex data to the vertex shader
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    // Describe the attributes (position, color, and normal)
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // Position and normal attribute (location 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32_UINT;
        attributeDescriptions[0].offset = offsetof(Vertex, dataBits);

        // Color attribute (location 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32_UINT;
        attributeDescriptions[1].offset = offsetof(Vertex, colorBits);

        return attributeDescriptions;
    }
};