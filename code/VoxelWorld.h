#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <chrono>
#include <string>

// Forward declarations to avoid including heavy headers
class VulkanRenderer;
class InputManager;
class WorldRenderer;
class Camera;

class VoxelWorld {
public:
    VoxelWorld();
    ~VoxelWorld();

    // Main application entry point
    void run();

private:
    // Window settings
    static const uint32_t DEFAULT_WIDTH = 800;
    static const uint32_t DEFAULT_HEIGHT = 600;
    uint32_t currentWindowWidth = DEFAULT_WIDTH;
    uint32_t currentWindowHeight = DEFAULT_HEIGHT;

    // SDL window
    SDL_Window* window = nullptr;

    // Core components (using smart pointers for automatic cleanup)
    std::unique_ptr<VulkanRenderer> renderer;
    std::unique_ptr<InputManager> inputManager;
    std::unique_ptr<WorldRenderer> worldRenderer;
    std::unique_ptr<Camera> camera;

    // Timing
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    float deltaTime = 0.0f;

    // HUD/Window titleVulkanRenderer: Updating SSAO uniforms,
    float titleUpdateTimer = 0.0f;
    const float TITLE_UPDATE_INTERVAL = 0.1f;

    // Render distance control
    int renderDistance = 6;  // How many chunks to keep loaded around player

    // Initialization and cleanup
    void initWindow();
    void initComponents();
    void mainLoop();
    void cleanup();

    // Frame timing and HUD
    void updateTiming();
    void updateWindowTitle();

    // Callbacks for input events
    void toggleWireframe();
    void toggleMouseCapture();
    void handleWindowResize(uint32_t newWidth, uint32_t newHeight);

    // Handle render distance changes
    void increaseRenderDistance();
    void decreaseRenderDistance();
};