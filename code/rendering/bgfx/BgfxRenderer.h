#pragma once

#include <SDL2/SDL.h>
#include <bgfx/bgfx.h>
#include <bx/thread.h>
#include <bx/uint32_t.h>
#include <bgfx/platform.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>

#include "Vertex.h"
#include "Camera.h"

// Forward declarations
class Chunk;

class BgfxRenderer {
public:
    BgfxRenderer();
    ~BgfxRenderer();

    // Initialize bgfx with the given window
    void initialize(SDL_Window* window, uint32_t windowWidth, uint32_t windowHeight);
    void cleanup();

    // Frame rendering
    bool beginFrame();
    void endFrame();

    // Chunk rendering system
    void renderChunks(const std::vector<Chunk*>& chunks);

    void updateUniforms(const UniformBufferObject& ubo);

    // Handle window resize
    void handleResize(uint32_t newWidth, uint32_t newHeight);

    // Toggle wireframe mode
    void toggleWireframe();

    // Get current frame dimensions
    uint32_t getFrameWidth() const { return windowWidth; }
    uint32_t getFrameHeight() const { return windowHeight; }

    // Helper methods for chunks
    void createVertexBuffer(const std::vector<Vertex>& vertices, bgfx::VertexBufferHandle& handle);
    void createIndexBuffer(const std::vector<uint32_t>& indices, bgfx::IndexBufferHandle& handle);

    // Get shader program for chunks to use
    bgfx::ProgramHandle getShaderProgram() const { return shaderProgram; }

private:
    // Window settings
    SDL_Window* window = nullptr;
    uint32_t windowWidth;
    uint32_t windowHeight;

    bgfx::VertexBufferHandle chunkVertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle chunkIndexBuffer = BGFX_INVALID_HANDLE;

    // bgfx handles
    bgfx::ProgramHandle shaderProgram = BGFX_INVALID_HANDLE;

    // Store chunks for rendering
    std::vector<Chunk*> chunksToRender;
    bgfx::UniformHandle u_chunkPos = BGFX_INVALID_HANDLE;

    // Render state
    bool wireframeMode = false;
    uint32_t currentFrame = 0;

    // Helper methods
    void initializeShaders();

    void loadShaderByName(const char* name, bgfx::ShaderHandle& shaderHandle);

    void createUniforms();

    void destroyUniforms();
    void cleanupBuffers();

    // Shader loading
    bgfx::ShaderHandle loadShader(const char* name);
    const bgfx::Memory* loadMemory(const char* filename);


    bgfx::OcclusionQueryHandle gpuTimerQuery;
    bool sent = false; 
};