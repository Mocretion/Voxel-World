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
    // Create all components
    renderer = std::make_unique<VulkanRenderer>();
    inputManager = std::make_unique<InputManager>();
    worldRenderer = std::make_unique<WorldRenderer>(renderer.get());
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 30.0f, 0.0f));

    // Initialize Vulkan renderer
    renderer->initialize(window, currentWindowWidth, currentWindowHeight);

    // Generate the voxel world
    worldRenderer->generateWorld(5, 5);

    // Set up ALL callbacks in one place
    std::cout << "[DEBUG] Setting all input callbacks..." << std::endl;

    inputManager->setToggleWireframeCallback([this]() {
        std::cout << "[DEBUG] F1 wireframe callback triggered" << std::endl;
        toggleWireframe();
        });

    inputManager->setToggleMouseCaptureCallback([this]() {
        std::cout << "[DEBUG] ESC mouse callback triggered" << std::endl;
        toggleMouseCapture();
        });

    inputManager->setWindowResizeCallback([this](uint32_t width, uint32_t height) {
        std::cout << "[DEBUG] RESIZE CALLBACK TRIGGERED: " << width << "x" << height << std::endl;
        handleWindowResize(width, height);
        });

    std::cout << "[DEBUG] All callbacks set. Starting with mouse captured..." << std::endl;
    inputManager->setMouseCaptured(true);
    std::cout << "[DEBUG] Initialization complete!" << std::endl;
}

void VoxelWorld::mainLoop()
{
    bool running = true;

    while (inputManager->processEvents(*camera, deltaTime))
    {
        updateTiming();

        // Try to begin frame - this will return false if window is minimized
        if (renderer->beginFrame())
        {
            // Only render if we successfully began the frame
            worldRenderer->render(*camera, static_cast<float>(currentWindowWidth) / static_cast<float>(currentWindowHeight));
            renderer->endFrame();
        }
        else
        {
            // Window is minimized or needs recreation, just wait a bit
            SDL_Delay(10); // Wait 10ms to avoid burning CPU
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

    // Get pitch and yaw values from camera using getter methods
    float pitch = camera->pitch;
    float yaw = camera->yaw;

    std::string title = "Voxel World - Position: (" +
        std::to_string(static_cast<int>(pos.x)) + ", " +
        std::to_string(static_cast<int>(pos.y)) + ", " +
        std::to_string(static_cast<int>(pos.z)) + ") - " + direction +
        " - Pitch: " + std::to_string(static_cast<int>(pitch)) + "° " +
        "Yaw: " + std::to_string(static_cast<int>(yaw)) + "°";

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

void VoxelWorld::handleWindowResize(uint32_t newWidth, uint32_t newHeight)
{
    std::cout << "[DEBUG] VoxelWorld::handleWindowResize called: " << newWidth << "x" << newHeight << std::endl;
    std::cout << "[DEBUG] Previous dimensions were: " << currentWindowWidth << "x" << currentWindowHeight << std::endl;

    // Update stored window dimensions
    currentWindowWidth = newWidth;
    currentWindowHeight = newHeight;

    // Notify the Vulkan renderer about the resize
    renderer->handleResize(newWidth, newHeight);

    std::cout << "[DEBUG] Window resize handled successfully" << std::endl;
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