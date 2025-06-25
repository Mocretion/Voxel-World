#include "InputManager.h"
#include <iostream>

InputManager::InputManager()
{
    // Initialize SDL mouse capture
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

bool InputManager::processEvents(Camera& camera, float deltaTime)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            return false; // Signal to quit

        case SDL_KEYDOWN:
            handleKeyPress(event.key.keysym.sym);
            break;

        case SDL_MOUSEMOTION:
            if (mouseCaptured)
            {
                handleMouseMotion(event.motion, camera);
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                // Window resize will be handled by the main application
            }
            break;
        }
    }

    // Process continuous keyboard input for movement
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);
    camera.processKeyboard(keyState, deltaTime);

    return true; // Continue running
}

void InputManager::setToggleWireframeCallback(std::function<void()> callback)
{
    onToggleWireframe = callback;
}

void InputManager::setToggleMouseCaptureCallback(std::function<void()> callback)
{
    onToggleMouseCapture = callback;
}

void InputManager::setMouseCaptured(bool captured)
{
    mouseCaptured = captured;
    SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);

    if (captured)
    {
        // Hide cursor and capture it
        SDL_ShowCursor(SDL_DISABLE);
        firstMouseMovement = true;
    }
    else
    {
        // Show cursor and release it
        SDL_ShowCursor(SDL_ENABLE);
    }
}

void InputManager::handleKeyPress(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_ESCAPE:
        // Toggle mouse capture with Escape key
        if (onToggleMouseCapture)
        {
            onToggleMouseCapture();
        }
        break;

    case SDLK_F1:
        // Toggle wireframe mode with F1 key
        if (onToggleWireframe)
        {
            onToggleWireframe();
        }
        break;

    case SDLK_F11:
        // Could add fullscreen toggle here
        break;
    }
}

void InputManager::handleMouseMotion(const SDL_MouseMotionEvent& motion, Camera& camera)
{
    if (firstMouseMovement)
    {
        lastMouseX = motion.x;
        lastMouseY = motion.y;
        firstMouseMovement = false;
        return;
    }

    // Calculate mouse offset from last position
    float xOffset = motion.x - lastMouseX;
    float yOffset = lastMouseY - motion.y; // Reversed since y-coordinates are flipped

    lastMouseX = motion.x;
    lastMouseY = motion.y;

    // Pass the offset to the camera
    camera.processMouseMovement(xOffset, yOffset);
}