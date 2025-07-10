@echo off
cd /d "%~dp0"

echo Compiling shaders...

glslangValidator -V vertex.vert -o shaders/vert.spv
glslangValidator -V fragment.frag -o shaders/frag.spv

glslangValidator -V copy_vertex.vert -o shaders/copy_vert.spv
glslangValidator -V copy_fragment.frag -o shaders/copy_frag.spv

glslangValidator -V ssao_vertex.vert -o shaders/ssao_vert.spv
glslangValidator -V ssao_fragment.frag -o shaders/ssao_frag.spv

glslangValidator -V ssao_blur_vertex.vert -o shaders/ssao_blur_vert.spv
glslangValidator -V ssao_blur_fragment.frag -o shaders/ssao_blur_frag.spv

glslangValidator -V ssao_only_fragment.frag -o shaders/ssao_only_frag.spv
glslangValidator -V final_composite_fragment.frag -o shaders/final_composite_frag.spv

if %errorlevel% neq 0 (
    echo Shader compilation failed!
    pause
    exit /b %errorlevel%
)

echo Shaders compiled successfully!
::pause