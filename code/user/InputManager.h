#pragma once

#include <SDL2/SDL.h>
#include <functional>
#include "Camera.h"

class InputManager {
public:
    InputManager();

    // Process SDL events and update camera
    bool processEvents(Camera& camera, float deltaTime);

    // Set callbacks for special events
    void setToggleWireframeCallback(std::function<void()> callback);
    void setToggleMouseCaptureCallback(std::function<void()> callback);
    void setToggleChunkGenerationCallback(std::function<void()> callback);
    void setWindowResizeCallback(std::function<void(uint32_t, uint32_t)> callback);

    // Callbacks for render distance control
    void setIncreaseRenderDistanceCallback(std::function<void()> callback);
    void setDecreaseRenderDistanceCallback(std::function<void()> callback);

    // Mouse capture state
    bool isMouseCaptured() const { return mouseCaptured; }
    void setMouseCaptured(bool captured);

    void setEnableChunkGeneration(bool generate) { enableChunkGeneration = generate; }
    bool getEnableChunkGeneration() { return enableChunkGeneration; }

private:
    bool mouseCaptured = false;
    bool enableChunkGeneration = true;
    bool firstMouseMovement = true;
    int lastMouseX = 0;
    int lastMouseY = 0;

    // Callbacks for special actions
    std::function<void()> onToggleWireframe;
    std::function<void()> onToggleMouseCapture;
    std::function<void()> onToggleChunkGeneration;
    std::function<void(uint32_t, uint32_t)> onWindowResize;

    // Render distance callbacks
    std::function<void()> onIncreaseRenderDistance;
    std::function<void()> onDecreaseRenderDistance;

    void handleKeyPress(SDL_Keycode key);
    void handleMouseMotion(const SDL_MouseMotionEvent& motion, Camera& camera);
};