#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include "VoxelWorld.h"
#include <omp.h>

int main(int argc, char* argv[])
{
    std::cout << "Testing OpenMP...\n";
    std::cout << "Max threads available: " << omp_get_max_threads() << "\n";

#pragma omp parallel num_threads(4)
    {
        int threadId = omp_get_thread_num();
        int numThreads = omp_get_num_threads();

#pragma omp critical  // Prevents output mixing
        {
            std::cout << "Hello from thread " << threadId
                << " of " << numThreads << " threads\n";
        }
    }


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