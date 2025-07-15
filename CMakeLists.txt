cmake_minimum_required(VERSION 3.16)
project(VoxelWorld)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TOOLCHAIN_FILE "D:/VS C# Projects/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
set(CMAKE_BUILD_TYPE Release)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# CRITICAL FIX: Force static runtime GLOBALLY before any subdirectories
if(MSVC)
    # Set global CMAKE variables to force static runtime for ALL targets
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Override default flags to ensure static runtime
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE "/MD" "/MT" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    
    # Ensure /MT is present
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif()

# Add FastNoise2 AFTER setting global runtime flags
add_subdirectory(external/FastNoise2)

find_package(OpenMP REQUIRED)

# Find SDL2 (vcpkg style)
find_package(SDL2 CONFIG REQUIRED)

# Find GLM
find_package(glm CONFIG REQUIRED)

# Create executable with bgfx source files
add_executable(${PROJECT_NAME} 
    "code/main.cpp"
    "code/VoxelWorld.cpp"
    "code/generation/Chunk.cpp"
    "code/generation/ChunkManager.cpp"
    "code/generation/NeighborCache.cpp"
    "code/user/Camera.cpp"
    "code/noise/PerlinNoise.cpp"
    "code/user/InputManager.cpp"
    "code/helper/ResourceManager.cpp"
    "code/rendering/WorldRenderer.cpp"
    "code/rendering/bgfx/BgfxRenderer.cpp"
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "external/FastNoise2/src"
    "external/bgfx/include"
    "external/bx/include"
    "external/bimg/include"
    ${CMAKE_CURRENT_SOURCE_DIR}/code/rendering
    ${CMAKE_CURRENT_SOURCE_DIR}/code/rendering/bgfx
    ${CMAKE_CURRENT_SOURCE_DIR}/code/user
    ${CMAKE_CURRENT_SOURCE_DIR}/code/noise
    ${CMAKE_CURRENT_SOURCE_DIR}/code/helper
    ${CMAKE_CURRENT_SOURCE_DIR}/code/generation
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE 
        /FS 
        /openmp 
        /arch:AVX2 
        /O2
        /MT  # Force static runtime for main target
    )
    
    # Set runtime library property for main target
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp -march=native -O2)
        target_link_libraries(${PROJECT_NAME} -fopenmp)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(${PROJECT_NAME} PRIVATE -fopenmp -march=native -O2)
        target_link_libraries(${PROJECT_NAME} -fopenmp)
    endif()
endif()

target_link_directories(${PROJECT_NAME}
    PRIVATE
    "./external/bgfx/.build/win64_vs2022/bin/"
)   

# Link libraries - REMOVED Vulkan, ADDED proper bgfx order
target_link_libraries(${PROJECT_NAME} 
    SDL2::SDL2
    SDL2::SDL2main
    glm::glm
    FastNoise2
    bgfxRelease      # bgfx must come first
    bimgRelease      # bimg depends on bx
    bxRelease        # bx is the base library
)

# Set subsystem to console and ignore conflicting runtime libraries
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE /NODEFAULTLIB:MSVCRT"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_ROOT}/installed/x64-windows/bin/SDL2.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()