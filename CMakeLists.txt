cmake_minimum_required(VERSION 3.16)
project(VoxelWorld)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TOOLCHAIN_FILE "D:/VS C# Projects/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_BUILD_TYPE Release)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

add_subdirectory(external/FastNoise2)

find_package(OpenMP REQUIRED)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find SDL2 (vcpkg style)
find_package(SDL2 CONFIG REQUIRED)

# Find GLM
find_package(glm CONFIG REQUIRED)

# Create executable with ALL source files
add_executable(${PROJECT_NAME} 
    "code/main.cpp"
    "code/VoxelWorld.cpp"
    "code/generation/Chunk.cpp"
    "code/generation/ChunkManager.cpp"
    "code/user/Camera.cpp"
    "code/noise/PerlinNoise.cpp"
    "code/user/InputManager.cpp"
    "code/helper/ResourceManager.cpp"
    "code/rendering/vulkan/VulkanRenderer.cpp"
    "code/rendering/WorldRenderer.cpp"
    "code/rendering/shader/CompositeShader.cpp"
    "code/rendering/shader/GBufferShader.cpp"
    "code/rendering/shader/Shader.cpp"
    "code/rendering/shader/SSAOShader.cpp"
    "code/rendering/shader/SSAOBlurShader.cpp"
    # Add any other .cpp files here as you create them
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "external/FastNoise2/src"
    ${CMAKE_CURRENT_SOURCE_DIR}/code/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/code/rendering/shader
    ${CMAKE_CURRENT_SOURCE_DIR}/code/rendering/vulkan
    ${CMAKE_CURRENT_SOURCE_DIR}/code/user
    ${CMAKE_CURRENT_SOURCE_DIR}/code/noise
    ${CMAKE_CURRENT_SOURCE_DIR}/code/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/code/generation
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FS /openmp /arch:AVX2 /O2)
else()
    # Keep the existing OpenMP settings for other compilers
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp /arch:AVX2 /O2)
        target_link_libraries(${PROJECT_NAME} -fopenmp)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp /arch:AVX2 /O2)
        target_link_libraries(${PROJECT_NAME} -fopenmp)
    endif()
endif()

# Link libraries (vcpkg style)
target_link_libraries(${PROJECT_NAME} 
    Vulkan::Vulkan
    SDL2::SDL2
    SDL2::SDL2main
    glm::glm  # Added GLM since you're using it in Vertex.h
    FastNoise2
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