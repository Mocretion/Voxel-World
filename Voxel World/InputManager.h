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

    // Mouse capture state
    bool isMouseCaptured() const { return mouseCaptured; }
    void setMouseCaptured(bool captured);

private:
    bool mouseCaptured = false;
    bool firstMouseMovement = true;
    int lastMouseX = 0;
    int lastMouseY = 0;

    // Callbacks for special actions
    std::function<void()> onToggleWireframe;
    std::function<void()> onToggleMouseCapture;

    void handleKeyPress(SDL_Keycode key);
    void handleMouseMotion(const SDL_MouseMotionEvent& motion, Camera& camera);
};