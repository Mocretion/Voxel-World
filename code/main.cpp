#include <SDL2/SDL.h>
#include <SDL_syswm.h>
#include <iostream>
#include <vector>
#include "VoxelWorld.h"
#include <FastNoise/FastNoise.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

/*int main(int, char**)
{
    bgfx::Init init;
    init.type = bgfx::RendererType::Count;  // Auto pick renderer
    init.resolution.width = 1280;
    init.resolution.height = 720;
    init.resolution.reset = BGFX_RESET_VSYNC;

	SDL_Window* window = SDL_CreateWindow("Voxel World", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    // You must provide your HWND here:
    if (SDL_GetWindowWMInfo(window, &wmInfo))
    {
        // On Windows
        HWND hwnd = wmInfo.info.win.window;
        init.platformData.nwh = hwnd;
        // On Linux/X11, use wmInfo.info.x11.window and display
        // On macOS, use wmInfo.info.cocoa.window
    }
    else
    {
        // Failed to get window info
        printf("Failed to get native window handle\n");
    }

    if (!bgfx::init(init))
    {
        printf("Failed to initialize bgfx\n");
    }

    return 0;
}*/


int main(int argc, char* argv[])
{


    std::cout << "FastNoise2 SIMD Level: "
        << FastSIMD::CPUMaxSIMDLevel() << "\n";

    VoxelWorld app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Application error: " << e.what() << std::endl;
        std::cin.get(); // Wait for user input before closing
        return -1;
    }

    std::cout << "Application ended successfully!" << std::endl;
    return 0;
}