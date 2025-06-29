cmake_minimum_required(VERSION 3.16)
project(VoxelWorld)  # Changed from VulkanApp to VoxelWorld
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TOOLCHAIN_FILE "D:/VS C# Projects/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

find_package(OpenMP REQUIRED)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find SDL2 (vcpkg style)
find_package(SDL2 CONFIG REQUIRED)

# Find GLM
find_package(glm CONFIG REQUIRED)

# Create executable with ALL source files
add_executable(${PROJECT_NAME} 
    "main.cpp"
    "VoxelWorld.cpp"
    "Chunk.cpp"
    "Camera.cpp"
    "PerlinNoise.cpp"
    "InputManager.cpp"
    "ResourceManager.cpp"
    "VulkanRenderer.cpp"
    "WorldRenderer.cpp"
    # Add any other .cpp files here as you create them
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /openmp)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp)
    target_link_libraries(${PROJECT_NAME} -fopenmp)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp)
    target_link_libraries(${PROJECT_NAME} -fopenmp)
endif()

# Link libraries (vcpkg style)
target_link_libraries(${PROJECT_NAME} 
    Vulkan::Vulkan
    SDL2::SDL2
    SDL2::SDL2main
    glm::glm  # Added GLM since you're using it in Vertex.h
)

# Set subsystem to console (optional, for debugging)
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_ROOT}/installed/x64-windows/bin/SDL2.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()