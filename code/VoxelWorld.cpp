#include "VoxelWorld.h"
#include "BgfxRenderer.h"  // Changed from VulkanRenderer
#include "InputManager.h"
#include "WorldRenderer.h"
#include "Camera.h"
#include <iostream>
#include <stdexcept>

VoxelWorld::VoxelWorld()
{
    lastFrameTime = std::chrono::high_resolution_clock::now();
}

VoxelWorld::~VoxelWorld()
{
    cleanup();
}

void VoxelWorld::run()
{
    std::cout << "Starting Voxel World application..." << std::endl;

    initWindow();
    initComponents();
    mainLoop();

    cleanup();
    std::cout << "Application ended successfully!" << std::endl;
}

void VoxelWorld::initWindow()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        throw std::runtime_error("Failed to initialize SDL");
    }

    // Create window - no need for Vulkan flag with bgfx
    window = SDL_CreateWindow(
        "Voxel World",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        currentWindowWidth,
        currentWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );

    if (!window)
    {
        throw std::runtime_error("Failed to create SDL window");
    }
}

void VoxelWorld::initComponents()
{
    std::cout << "Initializing components..." << std::endl;

    // Initialize bgfx renderer
    renderer = std::make_unique<BgfxRenderer>();
    renderer->initialize(window, currentWindowWidth, currentWindowHeight);

    // Initialize input manager
    inputManager = std::make_unique<InputManager>();

    // Set up input callbacks
    inputManager->setToggleWireframeCallback([this]() { toggleWireframe(); });
    inputManager->setToggleMouseCaptureCallback([this]() { toggleMouseCapture(); });
    inputManager->setWindowResizeCallback([this](uint32_t w, uint32_t h) { handleWindowResize(w, h); });

    // Set up render distance callbacks
    inputManager->setIncreaseRenderDistanceCallback([this]() { increaseRenderDistance(); });
    inputManager->setDecreaseRenderDistanceCallback([this]() { decreaseRenderDistance(); });

    // Initialize camera at a good starting position
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 35.0f, 0.0f));

    // Initialize world renderer
    worldRenderer = std::make_unique<WorldRenderer>(renderer.get());

    // Set initial render distance
    worldRenderer->setRenderDistance(renderDistance);

    // Enable mouse capture by default
    inputManager->setMouseCaptured(true);

    std::cout << "All components initialized successfully!" << std::endl;
}

void VoxelWorld::mainLoop()
{
    std::cout << "Starting main loop..." << std::endl;
    lastFrameTime = std::chrono::high_resolution_clock::now();

    while (true)
    {
        updateTiming();

        // Process input events
        if (!inputManager->processEvents(*camera, deltaTime))
        {
            break; // User wants to quit
        }

        // Update world based on camera position (infinite terrain)

        worldRenderer->updateWorld(*camera);
        

        // Begin rendering
        if (renderer->beginFrame())
        {
            // Render the world
            float aspectRatio = static_cast<float>(renderer->getFrameWidth()) /
                static_cast<float>(renderer->getFrameHeight());
            worldRenderer->render(*camera, aspectRatio);

            // End rendering
            renderer->endFrame();

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - lastFrameTime);

            //std::cout << "frame endered in " << duration.count() << "ms, "
            //    << std::endl;
     
        }

        // Update window title with debug info
        updateWindowTitle();
    }

    std::cout << "Main loop ended" << std::endl;
}

void VoxelWorld::updateTiming()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;

    // Cap extremely high delta times (can happen during debugging)
    if (deltaTime > 0.1f)
    {
        deltaTime = 0.1f;
    }
}

void VoxelWorld::updateWindowTitle()
{
    titleUpdateTimer += deltaTime;

    if (titleUpdateTimer >= TITLE_UPDATE_INTERVAL)
    {
        // Get camera position and direction
        glm::vec3 pos = camera->getPosition();
        std::string direction = worldRenderer->getDirectionString(camera->getFront());

        // Get current chunk and loaded chunk count
        glm::ivec2 currentChunk = worldRenderer->getCurrentPlayerChunk();
        int loadedChunks = worldRenderer->getLoadedChunkCount();

        // Create title
        std::string title = "Voxel World - Pos: (" +
            std::to_string(static_cast<int>(pos.x)) + ", " +
            std::to_string(static_cast<int>(pos.y)) + ", " +
            std::to_string(static_cast<int>(pos.z)) + ") | " +
            "Chunk: (" + std::to_string(currentChunk.x) + ", " + std::to_string(currentChunk.y) + ") | " +
            "Loaded: " + std::to_string(loadedChunks) + " chunks | " +
            "RD: " + std::to_string(worldRenderer->getRenderDistance()) + " | " +
            direction + " | " +
            "FPS: " + std::to_string(static_cast<int>(1.0f / deltaTime));

        SDL_SetWindowTitle(window, title.c_str());
        titleUpdateTimer = 0.0f;
    }
}

void VoxelWorld::toggleWireframe()
{
    renderer->toggleWireframe();
    std::cout << "Wireframe mode toggled" << std::endl;
}

void VoxelWorld::toggleMouseCapture()
{
    bool currentlyCaptures = inputManager->isMouseCaptured();
    inputManager->setMouseCaptured(!currentlyCaptures);

    std::cout << "Mouse " << (currentlyCaptures ? "released" : "captured") << std::endl;
}

void VoxelWorld::handleWindowResize(uint32_t newWidth, uint32_t newHeight)
{
    std::cout << "[DEBUG] VoxelWorld::handleWindowResize called: " << newWidth << "x" << newHeight << std::endl;
    std::cout << "[DEBUG] Previous dimensions were: " << currentWindowWidth << "x" << currentWindowHeight << std::endl;

    // Update stored window dimensions
    currentWindowWidth = newWidth;
    currentWindowHeight = newHeight;

    // Notify the bgfx renderer about the resize
    renderer->handleResize(newWidth, newHeight);

    std::cout << "[DEBUG] Window resize handled successfully" << std::endl;
}

void VoxelWorld::increaseRenderDistance()
{
    renderDistance = std::min(renderDistance + 1, 20);  // Max 20 chunks
    worldRenderer->setRenderDistance(renderDistance);
    std::cout << "Render distance increased to: " << renderDistance << std::endl;
}

void VoxelWorld::decreaseRenderDistance()
{
    renderDistance = std::max(renderDistance - 1, 2);   // Min 2 chunks
    worldRenderer->setRenderDistance(renderDistance);
    std::cout << "Render distance decreased to: " << renderDistance << std::endl;
}

void VoxelWorld::cleanup()
{
    // Smart pointers will automatically clean up when they go out of scope
    // but we can explicitly reset them in reverse order of creation
    worldRenderer.reset();
    inputManager.reset();
    renderer.reset();
    camera.reset();

    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
}