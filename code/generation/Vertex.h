#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>
#include <bgfx/bgfx.h>

// Uniform Buffer Object for shader uniforms
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Push constants for chunk coordinates
struct ChunkPushConstants {
    int chunkX;
    int chunkY;
    int chunkZ;
    int padding; // Align to 16 bytes
};

// Vertex structure for our 3D cubes (now with normals!)
struct Vertex {
    uint32_t dataBits;  // 3x7: 0 - 20: POSITION, 21 - 23: NORMAL
    uint32_t colorBits;  // 3x8: 0 - 23: COLOR

    static bgfx::VertexLayout getBgfxLayout()
    {
        static bgfx::VertexLayout layout;
        static bool initialized = false;

        if (!initialized)
        {
            layout.begin()
                .add(bgfx::Attrib::Position, 1, bgfx::AttribType::Float)  // dataBits
                .add(bgfx::Attrib::Color0, 1, bgfx::AttribType::Float)    // colorBits
                .end();
            initialized = true;
        }

        return layout;
    }
};

struct ChunkInstanceData {
    int x;
    int y;
    int z;
    int pad;
};