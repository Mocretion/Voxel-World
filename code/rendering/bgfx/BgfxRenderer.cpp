#include "BgfxRenderer.h"
#include "Chunk.h"
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#endif

BgfxRenderer::BgfxRenderer()
{
    // Constructor - main initialization happens in initialize()
}

BgfxRenderer::~BgfxRenderer()
{
    cleanup();
}

void BgfxRenderer::initialize(SDL_Window* window, uint32_t windowWidth, uint32_t windowHeight)
{
    std::cout << "Initializing bgfx renderer..." << std::endl;
    this->window = window;
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;

    std::cout << "Window size: " << windowWidth << "x" << windowHeight << std::endl;

    bool initSuccess = false;

    bgfx::Init init;
    init.type = bgfx::RendererType::Vulkan;
    init.vendorId = BGFX_PCI_ID_NVIDIA;  // Auto-detect GPU
    init.resolution.width = windowWidth;
    init.resolution.height = windowHeight;
    //init.resolution.reset = BGFX_RESET_VSYNC;
    init.callback = nullptr;  // Or add custom callback
    init.allocator = nullptr;  // Use default
    //init.debug = BGFX_DEBUG_STATS; // Default debug level

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    // Link with SDL window
    if (SDL_GetWindowWMInfo(window, &wmInfo))
    {
        // On Windows
        HWND hwnd = wmInfo.info.win.window;
        init.platformData.nwh = hwnd;
    }
    else
    {
        // Failed to get window info
        printf("Failed to get native window handle\n");
    }

    // Start the window
    if (bgfx::init(init))
    {
        std::cout << "SUCCESS: Vulkan initialized!" << std::endl;
        initSuccess = true;
    }
    else
    {
        std::cout << "FAILED: Vulkan failed to initialize" << std::endl;
    }

    // Set view 0 clear state
    bgfx::setViewClear(0
        , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
        , 0x303030ff
        , 1.0f
        , 0
    );

    // Set view rect for view 0
    bgfx::setViewRect(0, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));

    // Set render state
    uint64_t state = BGFX_STATE_WRITE_RGB
        | BGFX_STATE_WRITE_A
        | BGFX_STATE_WRITE_Z
        | BGFX_STATE_DEPTH_TEST_LESS
        | BGFX_STATE_CULL_CW;

    bgfx::setState(state);

    try
    {
        // Initialize shaders
        initializeShaders();
        createUniforms();
        std::cout << "Shader initialization successful!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Shader initialization failed: " << e.what() << std::endl;
        throw;
    }

    const bgfx::Caps* caps = bgfx::getCaps();

    std::cout << "Creating DynamicBufferPool..." << std::endl;
    try
    {
        bufferPool = std::make_unique<DynamicBufferPool>();
        std::cout << "DynamicBufferPool created successfully!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create DynamicBufferPool: " << e.what() << std::endl;
        throw;
    }

    bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_STATS);

    std::cout << "bgfx renderer initialized successfully!" << std::endl;
}

void BgfxRenderer::initializeShaders()
{
    std::cout << "Loading shaders..." << std::endl;

    // Load vertex and fragment shaders
    bgfx::ShaderHandle vsh;
    loadShaderByName("vs_basic", vsh);

    bgfx::ShaderHandle fsh;
    loadShaderByName("fs_basic", fsh);

    // Create shader program
    std::cout << "Creating shader program..." << std::endl;
    shaderProgram = bgfx::createProgram(vsh, fsh, true);

    if (!bgfx::isValid(shaderProgram))
    {
        throw std::runtime_error("Failed to create shader program!");
    }

    bgfx::ShaderHandle csh;
    loadShaderByName("cs_indirect", csh);

    std::cout << "Creating compute shader program..." << std::endl;
    computeProgram = bgfx::createProgram(csh, true);

    if (!bgfx::isValid(computeProgram))
    {
        throw std::runtime_error("Failed to create compute program!");
    }

    std::cout << "Creating uniforms..." << std::endl;

    std::cout << "Shaders loaded successfully!" << std::endl;
}

void BgfxRenderer::loadShaderByName(const char* name, bgfx::ShaderHandle& shaderHandle)
{
    std::cout << "Loading '" << name << "' shader..." << std::endl;
    shaderHandle = loadShader(name);

    if (!bgfx::isValid(shaderHandle))
    {
        throw std::runtime_error(std::string("Failed to load '") + name + "' shader!");
    }
}

void BgfxRenderer::createUniforms()
{
    std::cout << "Creating uniforms..." << std::endl;

    // Create chunk position uniform (reused every frame)
    u_chunkPos = bgfx::createUniform("u_chunkPos", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_chunkPos))
    {
        throw std::runtime_error("Failed to create chunk position uniform!");
    }

    std::cout << "Uniforms created successfully!" << std::endl;
}

void BgfxRenderer::destroyUniforms()
{
    if (bgfx::isValid(u_chunkPos))
    {
        bgfx::destroy(u_chunkPos);
        u_chunkPos = BGFX_INVALID_HANDLE;
    }
}

bgfx::ShaderHandle BgfxRenderer::loadShader(const char* name)
{
    std::string filename = std::string("shaders/") + name + ".bin";

    std::cout << "Attempting to load shader: " << filename << std::endl;

    const bgfx::Memory* mem = loadMemory(filename.c_str());
    if (!mem)
    {
        std::cerr << "Failed to load shader file: " << filename << std::endl;
        throw std::runtime_error("Failed to load shader: " + filename);
    }

    std::cout << "Shader file loaded, size: " << mem->size << " bytes" << std::endl;

    bgfx::ShaderHandle handle = bgfx::createShader(mem);

    if (!bgfx::isValid(handle))
    {
        std::cerr << "Failed to create shader from file: " << filename << std::endl;
        throw std::runtime_error("Failed to create shader from: " + filename);
    }

    std::cout << "Shader created successfully: " << filename << std::endl;
    return handle;
}

const bgfx::Memory* BgfxRenderer::loadMemory(const char* filename)
{
    std::cout << "Loading file: " << filename << std::endl;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cout << "Failed to open file: " << filename << std::endl;
        return nullptr;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::cout << "File size: " << size << " bytes" << std::endl;

    const bgfx::Memory* mem = bgfx::alloc(uint32_t(size + 1));
    file.read((char*)mem->data, size);
    mem->data[mem->size - 1] = '\0';

    std::cout << "File loaded into memory successfully" << std::endl;
    return mem;
}

bool BgfxRenderer::beginFrame()
{
	bgfx::touch(0);

    if (chunksToRender.empty())
        return true;

    for (Chunk* chunk : chunksToRender)
    {
        chunk->uploadToBufferPool(bufferPool.get());
    }

    // Render all chunks using indirect drawing
    bufferPool->renderAll(computeProgram, shaderProgram, u_chunkPos);

    return true;
}

void BgfxRenderer::endFrame()
{
    // Advance to next frame
    bgfx::frame();
    currentFrame++;
}

void BgfxRenderer::renderChunks(const std::vector<Chunk*>& chunks)
{
    chunksToRender = chunks;
    //std::cout << "[BGFX] Prepared " << chunks.size() << " chunks for rendering" << std::endl;
}

void BgfxRenderer::updateUniforms(const UniformBufferObject& ubo)
{
    bgfx::setViewTransform(0, &ubo.view, &ubo.proj);

    // Uniforms will be set in beginFrame() before rendering
}

void BgfxRenderer::handleResize(uint32_t newWidth, uint32_t newHeight)
{
    std::cout << "[BGFX] Handling resize: " << newWidth << "x" << newHeight << std::endl;

    windowWidth = newWidth;
    windowHeight = newHeight;

    // Reset bgfx with new resolution
    bgfx::reset(windowWidth, windowHeight);

    // Update view rect
    bgfx::setViewRect(0, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));
}

void BgfxRenderer::toggleWireframe()
{
    wireframeMode = !wireframeMode;
    std::cout << "Wireframe mode " << (wireframeMode ? "enabled" : "disabled") << std::endl;
}

void BgfxRenderer::performMaintenance()
{
    // Defragment if fragmentation is high
    float fragmentation = bufferPool->getAverageFragmentation();
    if (fragmentation > 0.3f)
    {
        std::cout << "Performing buffer defragmentation (fragmentation: "
            << fragmentation << ")" << std::endl;
        bufferPool->defragmentBuffers(0.3f);
    }    
}

void BgfxRenderer::cleanup()
{
    std::cout << "Cleaning up bgfx renderer..." << std::endl;

    bufferPool.reset();

    // Cleanup uniforms
    destroyUniforms();

    // Cleanup shader program
    if (bgfx::isValid(shaderProgram)) bgfx::destroy(shaderProgram);
    if (bgfx::isValid(computeProgram)) bgfx::destroy(computeProgram);

    // Shutdown bgfx
    bgfx::shutdown();

    std::cout << "bgfx cleanup completed" << std::endl;
}
