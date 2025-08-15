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
#include "DynamicBufferPool.h"

// Forward declarations
class Chunk;

class BgfxRenderer {
public:
    BgfxRenderer();
    ~BgfxRenderer();

    // Initialize bgfx with the given window
    void initialize(SDL_Window* window, uint32_t windowWidth, uint32_t windowHeight);
    void performMaintenance();
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

    // Get shader program for chunks to use
    bgfx::ProgramHandle getShaderProgram() const { return shaderProgram; }

    DynamicBufferPool* getBufferPool() { return bufferPool.get(); }

private:
    // Window settings
    SDL_Window* window = nullptr;
    uint32_t windowWidth;
    uint32_t windowHeight;

    std::unique_ptr<DynamicBufferPool> bufferPool;

    // bgfx handles
    bgfx::ProgramHandle shaderProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle computeProgram = BGFX_INVALID_HANDLE;

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

    // Shader loading
    bgfx::ShaderHandle loadShader(const char* name);
    const bgfx::Memory* loadMemory(const char* filename);
};