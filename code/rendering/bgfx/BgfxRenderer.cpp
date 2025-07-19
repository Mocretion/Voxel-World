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
    maxThreads = bx::min<int32_t>(caps->limits.maxEncoders, BX_COUNTOF(renderThreads));
    numThreads = (maxThreads + 1) / 2;

    for (int32_t ii = 0; ii < maxThreads; ++ii)
    {
        renderThreads[ii].init(threadFunc, this);
    }

    bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_STATS);

    gpuTimerQuery = bgfx::createOcclusionQuery();

    std::cout << "bgfx renderer initialized successfully!" << std::endl;
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

    std::cout << "Shaders loaded successfully!" << std::endl;
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

int32_t BgfxRenderer::renderThread(bx::Thread* _thread)
{
    for (;;)
    {
        union
        {
            void* ptr;
            uintptr_t id;

        } cast;

        cast.ptr = _thread->pop();  // Wait for work signal in beginFrame()
        if (UINTPTR_MAX == cast.id)  // Work signal was the shutdown signal
        {
            break;
        }


        const uint32_t numThreads = uint32_t(cast.id);
        const uint32_t idx = uint32_t(_thread - renderThreads);
        //std::cout << "[BGFX] Thread " << idx << " recieved a work signal" << std::endl;

        const uint32_t num = chunksToRender.size() / numThreads;
        const uint32_t rem = idx == numThreads - 1 ? uint32_t(chunksToRender.size()) % numThreads : 0;
        const uint32_t xx = idx * num;

        //std::cout << "[BGFX] Thread " << idx << " processing chunks from " << xx
		//	<< " to " << (xx + num + rem) <<". Total chunks: " << chunksToRender.size() << std::endl;

        submitChunk(idx + 1, xx, num + rem);
    }

    return bx::kExitSuccess;
}

void BgfxRenderer::submitChunk(uint32_t _tid, uint32_t _xstart, uint32_t _num)
{
    bgfx::Encoder* encoder = bgfx::begin();
    
    if (0 != _tid)
    {
        sync.post();
    }

    if (NULL != encoder)
    {

        for (uint32_t xx = _xstart, xend = _xstart + _num; xx < xend; ++xx)
        {
            //std::cout << xx << std::endl;

			Chunk* chunk = chunksToRender[xx];
            if (chunk)
            {
                float chunkPos[4] = { chunk->getChunkX(), chunk->getChunkY(), chunk->getChunkZ(), 0.0f };
                bgfx::setUniform(u_chunkPos, chunkPos);
                /*if (!sent)
                {
                    chunk->draw(this, encoder, gpuTimerQuery, true);
                    sent = true;
                }
                else
                {*/
                    chunk->draw(this, encoder, gpuTimerQuery, false);

                    int32_t gpuTimeNs = 0;
                    if (bgfx::getResult(gpuTimerQuery, &gpuTimeNs))
                    {
                        float gpuTimeMs = float(gpuTimeNs) / 1e6f;
                        //printf("GPU draw time: %.3f ms\n", gpuTimeMs);
                        sent = false;
                    }
                //}
            }

            
        }
        
        bgfx::end(encoder);
    }
}


bool BgfxRenderer::beginFrame()
{
	bgfx::touch(0);

    if (chunksToRender.empty())
        return true;

    for (Chunk* chunk : chunksToRender)
    {
        chunk->uploadToBgfx(this);
	}

    if (false)//1 < numThreads && chunksToRender.size() > 26)
    {
        for (uint32_t ii = 0; ii < numThreads; ++ii)
        {
            //std::cout << "[BGFX] sending thread " << ii << " a work signal" << std::endl;
            renderThreads[ii].push(reinterpret_cast<void*>(uintptr_t(numThreads)));
        }

        for (uint32_t ii = 0; ii < numThreads; ++ii)
        {
            sync.wait();
        }
    }
    else
    {
        //std::cout << "[BGFX] Using no threads for rendering" << std::endl;
        submitChunk(0, 0, uint32_t(chunksToRender.size()));
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
    destroyUniforms();

    // Cleanup shader program
    if (bgfx::isValid(shaderProgram)) bgfx::destroy(shaderProgram);

    // Shutdown bgfx
    bgfx::shutdown();

    std::cout << "bgfx cleanup completed" << std::endl;
}
