#include "InputManager.h"
#include <iostream>

InputManager::InputManager()
{
    std::cout << "[DEBUG] InputManager constructor called" << std::endl;
    // Don't set relative mouse mode here - wait for explicit capture
}

bool InputManager::processEvents(Camera& camera, float deltaTime)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            std::cout << "[DEBUG] SDL_QUIT event received" << std::endl;
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
                std::cout << "[DEBUG] Window resize event - New size: " << event.window.data1 << "x" << event.window.data2 << std::endl;
                // Notify VoxelWorld about the resize
                if (onWindowResize)
                {
                    std::cout << "[DEBUG] Calling resize callback..." << std::endl;
                    onWindowResize(static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2));
                }
                else
                {
                    std::cout << "[DEBUG] ERROR: No resize callback set!" << std::endl;
                }
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

void InputManager::setWindowResizeCallback(std::function<void(uint32_t, uint32_t)> callback)
{
    std::cout << "[DEBUG] Setting window resize callback" << std::endl;
    onWindowResize = callback;
    std::cout << "[DEBUG] Window resize callback set successfully" << std::endl;
}

void InputManager::setMouseCaptured(bool captured)
{
    std::cout << "[DEBUG] Setting mouse captured: " << (captured ? "true" : "false") << std::endl;
    mouseCaptured = captured;

    if (captured)
    {
        // Enable relative mouse mode - this is KEY!
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
        firstMouseMovement = true;
        std::cout << "[DEBUG] Relative mouse mode ENABLED" << std::endl;
    }
    else
    {
        // Disable relative mouse mode
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
        std::cout << "[DEBUG] Relative mouse mode DISABLED" << std::endl;
    }
}

void InputManager::handleKeyPress(SDL_Keycode key)
{
    switch (key)
    {
    case SDLK_ESCAPE:
        std::cout << "[DEBUG] Escape key pressed - toggling mouse capture" << std::endl;
        if (onToggleMouseCapture)
        {
            onToggleMouseCapture();
        }
        break;

    case SDLK_F1:
        std::cout << "[DEBUG] F1 key pressed - toggling wireframe" << std::endl;
        if (onToggleWireframe)
        {
            onToggleWireframe();
        }
        break;

    case SDLK_F11:
        std::cout << "[DEBUG] F11 key pressed" << std::endl;
        break;
    }
}

void InputManager::handleMouseMotion(const SDL_MouseMotionEvent& motion, Camera& camera)
{
    if (firstMouseMovement)
    {
        std::cout << "[DEBUG] First mouse movement - ignoring" << std::endl;
        firstMouseMovement = false;
        return;
    }

    // In relative mouse mode, motion.xrel and motion.yrel contain the relative movement
    // These are NOT limited by screen boundaries!
    float xOffset = static_cast<float>(motion.xrel);
    float yOffset = static_cast<float>(-motion.yrel); // Invert Y axis

    // Pass the offset to the camera
    camera.processMouseMovement(xOffset, yOffset);
}