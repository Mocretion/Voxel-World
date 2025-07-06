@echo off
cd /d "%~dp0"

echo Compiling shaders...

glslangValidator -V vertex.vert -o shaders/vert.spv
glslangValidator -V fragment.frag -o shaders/frag.spv

glslangValidator -V copy_vertex.vert -o shaders/copy_vert.spv
glslangValidator -V copy_fragment.frag -o shaders/copy_frag.spv

if %errorlevel% neq 0 (
    echo Shader compilation failed!
    pause
    exit /b %errorlevel%
)

echo Shaders compiled successfully!
::pause