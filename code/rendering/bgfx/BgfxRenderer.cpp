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
    this->window = window;
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;

    std::cout << "Initializing bgfx renderer..." << std::endl;
    std::cout << "Window size: " << windowWidth << "x" << windowHeight << std::endl;

    // Setup platform data for bgfx
    try
    {
        setupPlatformData();
        std::cout << "Platform data setup successful" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Platform data setup failed: " << e.what() << std::endl;
        throw;
    }

    // Try different renderer types in order of preference (Vulkan first!)
    std::vector<bgfx::RendererType::Enum> renderers = {
        bgfx::RendererType::Vulkan,       // Try Vulkan FIRST - you already have drivers
        bgfx::RendererType::Direct3D11,
        bgfx::RendererType::OpenGL,
        bgfx::RendererType::Direct3D12
    };

    bool initSuccess = false;
    bgfx::RendererType::Enum workingRenderer = bgfx::RendererType::Count;

    for (auto rendererType : renderers)
    {
        std::cout << "Trying renderer: " << bgfx::getRendererName(rendererType) << std::endl;

        bgfx::Init init;
        init.type = rendererType;
        init.resolution.width = windowWidth;
        init.resolution.height = windowHeight;
        init.resolution.reset = BGFX_RESET_VSYNC;
        init.debug = BGFX_DEBUG_TEXT;

        if (bgfx::init(init))
        {
            std::cout << "SUCCESS: " << bgfx::getRendererName(rendererType) << " initialized!" << std::endl;
            workingRenderer = rendererType;
            initSuccess = true;
            break;
        }
        else
        {
            std::cout << "FAILED: " << bgfx::getRendererName(rendererType) << " failed to initialize" << std::endl;
        }
    }

    if (!initSuccess)
    {
        std::cerr << "All renderer types failed!" << std::endl;
        std::cerr << "This usually means:" << std::endl;
        std::cerr << "1. Graphics drivers are too old" << std::endl;
        std::cerr << "2. Running in a VM without GPU acceleration" << std::endl;
        std::cerr << "3. Missing Visual C++ Redistributable" << std::endl;
        std::cerr << "4. bgfx binary is incompatible with your system" << std::endl;
        throw std::runtime_error("No compatible renderer found!");
    }

    std::cout << "bgfx::init() successful with " << bgfx::getRendererName(workingRenderer) << "!" << std::endl;

    // Print renderer info
    const bgfx::Caps* caps = bgfx::getCaps();
    std::cout << "Renderer: " << bgfx::getRendererName(caps->rendererType) << std::endl;
    std::cout << "Vendor: 0x" << std::hex << caps->vendorId << std::dec << std::endl;
    std::cout << "Device: 0x" << std::hex << caps->deviceId << std::dec << std::endl;

    // Set view 0 clear state
    bgfx::setViewClear(0
        , BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
        , 0x303030ff  // Dark gray background
        , 1.0f
        , 0
    );

    // Set view rect for view 0
    bgfx::setViewRect(0, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));

    try
    {
        // Initialize shaders
        initializeShaders();
        std::cout << "Shader initialization successful!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Shader initialization failed: " << e.what() << std::endl;
        throw;
    }

    std::cout << "bgfx renderer initialized successfully!" << std::endl;
}

void BgfxRenderer::setupPlatformData()
{
    bgfx::PlatformData pd;
    memset(&pd, 0, sizeof(pd)); // Initialize to zero

    std::cout << "Setting up platform data..." << std::endl;

#ifdef _WIN32
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);

    std::cout << "Getting Windows window info..." << std::endl;
    if (!SDL_GetWindowWMInfo(window, &wmi))
    {
        std::cerr << "SDL_GetWindowWMInfo failed: " << SDL_GetError() << std::endl;
        throw std::runtime_error("Failed to get window info");
    }

    std::cout << "Window subsystem: " << wmi.subsystem << std::endl;

    if (wmi.subsystem != SDL_SYSWM_WINDOWS)
    {
        std::cerr << "Expected Windows subsystem, got: " << wmi.subsystem << std::endl;
        throw std::runtime_error("Not running on Windows subsystem");
    }

    pd.nwh = wmi.info.win.window;
    std::cout << "Window handle: " << pd.nwh << std::endl;

    if (!pd.nwh)
    {
        throw std::runtime_error("Invalid window handle");
    }

#elif defined(__APPLE__)
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi))
    {
        throw std::runtime_error("Failed to get window info: " + std::string(SDL_GetError()));
    }
    pd.nwh = wmi.info.cocoa.window;
#else
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    if (!SDL_GetWindowWMInfo(window, &wmi))
    {
        throw std::runtime_error("Failed to get window info: " + std::string(SDL_GetError()));
    }
    pd.ndt = wmi.info.x11.display;
    pd.nwh = (void*)(uintptr_t)wmi.info.x11.window;
#endif

    std::cout << "Calling bgfx::setPlatformData..." << std::endl;
    bgfx::setPlatformData(pd);
    std::cout << "Platform data set successfully" << std::endl;
}

void BgfxRenderer::initializeShaders()
{
    std::cout << "Loading shaders..." << std::endl;

    // Load vertex and fragment shaders
    std::cout << "Loading vertex shader..." << std::endl;
    bgfx::ShaderHandle vsh = loadShader("vs_basic");

    std::cout << "Loading fragment shader..." << std::endl;
    bgfx::ShaderHandle fsh = loadShader("fs_basic");

    if (!bgfx::isValid(vsh))
    {
        throw std::runtime_error("Failed to load vertex shader!");
    }

    if (!bgfx::isValid(fsh))
    {
        throw std::runtime_error("Failed to load fragment shader!");
    }

    // Create shader program
    std::cout << "Creating shader program..." << std::endl;
    shaderProgram = bgfx::createProgram(vsh, fsh, true);

    if (!bgfx::isValid(shaderProgram))
    {
        throw std::runtime_error("Failed to create shader program!");
    }

    std::cout << "Creating uniforms..." << std::endl;
    // Create uniforms
    uniformModel = bgfx::createUniform("u_model", bgfx::UniformType::Mat4);
    uniformView = bgfx::createUniform("u_view", bgfx::UniformType::Mat4);
    uniformProj = bgfx::createUniform("u_proj", bgfx::UniformType::Mat4);

    if (!bgfx::isValid(uniformModel) || !bgfx::isValid(uniformView) || !bgfx::isValid(uniformProj))
    {
        throw std::runtime_error("Failed to create uniforms!");
    }

    std::cout << "Shaders loaded successfully!" << std::endl;
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
    // Set view 0 default viewport
    bgfx::setViewRect(0, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));

    // Touch view 0 to make sure it's processed
    bgfx::touch(0);

    // Set uniforms for all chunks
    bgfx::setUniform(uniformModel, &modelMatrix);
    bgfx::setUniform(uniformView, &viewMatrix);
    bgfx::setUniform(uniformProj, &projMatrix);

    // Render all chunks
    for (Chunk* chunk : chunksToRender)
    {
        if (chunk && chunk->hasRenderableData())
        {
            chunk->draw(this);
        }
    }

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
    std::cout << "[BGFX] Prepared " << chunks.size() << " chunks for rendering" << std::endl;
}

void BgfxRenderer::updateUniforms(const UniformBufferObject& ubo)
{
    modelMatrix = ubo.model;
    viewMatrix = ubo.view;
    projMatrix = ubo.proj;

    // Uniforms will be set in beginFrame() before rendering
}

void BgfxRenderer::handleResize(uint32_t newWidth, uint32_t newHeight)
{
    std::cout << "[BGFX] Handling resize: " << newWidth << "x" << newHeight << std::endl;

    windowWidth = newWidth;
    windowHeight = newHeight;

    // Reset bgfx with new resolution
    bgfx::reset(windowWidth, windowHeight, BGFX_RESET_VSYNC);

    // Update view rect
    bgfx::setViewRect(0, 0, 0, uint16_t(windowWidth), uint16_t(windowHeight));
}

void BgfxRenderer::toggleWireframe()
{
    wireframeMode = !wireframeMode;
    std::cout << "Wireframe mode " << (wireframeMode ? "enabled" : "disabled") << std::endl;
}

void BgfxRenderer::createVertexBuffer(const std::vector<Vertex>& vertices, bgfx::VertexBufferHandle& handle)
{
    // Create vertex buffer
    const bgfx::Memory* mem = bgfx::makeRef(vertices.data(), uint32_t(vertices.size() * sizeof(Vertex)));
    handle = bgfx::createVertexBuffer(mem, Vertex::getBgfxLayout());
}

void BgfxRenderer::createIndexBuffer(const std::vector<uint32_t>& indices, bgfx::IndexBufferHandle& handle)
{
    // Create index buffer
    const bgfx::Memory* mem = bgfx::makeRef(indices.data(), uint32_t(indices.size() * sizeof(uint32_t)));
    handle = bgfx::createIndexBuffer(mem, BGFX_BUFFER_INDEX32);
}

void BgfxRenderer::cleanup()
{
    std::cout << "Cleaning up bgfx renderer..." << std::endl;

    // Cleanup uniforms
    if (bgfx::isValid(uniformModel)) bgfx::destroy(uniformModel);
    if (bgfx::isValid(uniformView)) bgfx::destroy(uniformView);
    if (bgfx::isValid(uniformProj)) bgfx::destroy(uniformProj);

    // Cleanup shader program
    if (bgfx::isValid(shaderProgram)) bgfx::destroy(shaderProgram);

    // Shutdown bgfx
    bgfx::shutdown();

    std::cout << "bgfx cleanup completed" << std::endl;
}