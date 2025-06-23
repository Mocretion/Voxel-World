#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
    std::cout << "Starting program..." << std::endl;

    // Initialize SDL
    std::cout << "Initializing SDL..." << std::endl;
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        std::cin.get(); // Wait for user input before closing
        return -1;
    }
    std::cout << "SDL initialized successfully!" << std::endl;

    // Create window
    std::cout << "Creating window..." << std::endl;
    SDL_Window* window = SDL_CreateWindow(
        "Voxel World",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        800, 600,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window)
    {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        std::cin.get(); // Wait for user input before closing
        return -1;
    }
    std::cout << "Window created successfully!" << std::endl;

    // Main loop
    std::cout << "Starting main loop..." << std::endl;
    bool running = true;
    SDL_Event event;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                std::cout << "Quit event received" << std::endl;
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                std::cout << "Escape key pressed" << std::endl;
                running = false;
            }
        }

        SDL_Delay(16); // ~60 FPS
    }

    std::cout << "Cleaning up..." << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::cout << "Program ended successfully!" << std::endl;

    return 0;
}