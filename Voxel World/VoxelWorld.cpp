#include "VoxelWorld.h"
#include "VulkanRenderer.h"
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

    try
    {
        initWindow();
        initComponents();
        mainLoop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Application error: " << e.what() << std::endl;
        throw;
    }

    cleanup();
    std::cout << "Application ended successfully!" << std::endl;
}

void VoxelWorld::initWindow()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        throw std::runtime_error("Failed to initialize SDL");
    }

    // Create window with Vulkan support
    window = SDL_CreateWindow(
        "Voxel World",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        currentWindowWidth,
        currentWindowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window)
    {
        throw std::runtime_error("Failed to create SDL window");
    }
}

void VoxelWorld::initComponents()
{
    // Create all components using smart pointers
    renderer = std::make_unique<VulkanRenderer>();
    inputManager = std::make_unique<InputManager>();
    worldRenderer = std::make_unique<WorldRenderer>(renderer.get());
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 30.0f, 0.0f));

    // Initialize Vulkan renderer
    renderer->initialize(window, currentWindowWidth, currentWindowHeight);

    // Generate the voxel world
    worldRenderer->generateWorld(5, 5); // 5x5 chunks

    // Set up input callbacks
    inputManager->setToggleWireframeCallback([this]() { toggleWireframe(); });
    inputManager->setToggleMouseCaptureCallback([this]() { toggleMouseCapture(); });

    // Start with mouse captured for first-person experience
    inputManager->setMouseCaptured(true);
}

void VoxelWorld::mainLoop()
{
    bool running = true;

    while (running)
    {
        updateTiming();

        // Process input events
        running = inputManager->processEvents(*camera, deltaTime);

        // Begin rendering frame
        if (renderer->beginFrame())
        {
            // Render the world
            float aspectRatio = static_cast<float>(renderer->getFrameWidth()) /
                static_cast<float>(renderer->getFrameHeight());

            worldRenderer->render(*camera, aspectRatio);

            // End frame
            renderer->endFrame();
        }

        // Update window title periodically
        titleUpdateTimer += deltaTime;
        if (titleUpdateTimer >= TITLE_UPDATE_INTERVAL)
        {
            updateWindowTitle();
            titleUpdateTimer = 0.0f;
        }
    }
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
    std::string direction = worldRenderer->getDirectionString(camera->getFront());
    glm::vec3 pos = camera->getPosition();

    std::string title = "Voxel World - Position: (" +
        std::to_string(static_cast<int>(pos.x)) + ", " +
        std::to_string(static_cast<int>(pos.y)) + ", " +
        std::to_string(static_cast<int>(pos.z)) + ") - " + direction;

    SDL_SetWindowTitle(window, title.c_str());
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