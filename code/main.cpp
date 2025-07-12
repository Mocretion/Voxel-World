#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include "VoxelWorld.h"
#include <FastNoise/FastNoise.h>

int main(int argc, char* argv[])
{


    std::cout << "FastNoise2 SIMD Level: "
        << FastSIMD::CPUMaxSIMDLevel() << "\n";


    std::cout << "Starting Vulkan application..." << std::endl;

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