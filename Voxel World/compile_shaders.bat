@echo off
cd /d "%~dp0"

echo Compiling shaders...

glslangValidator -V vertex.vert -o shaders/vert.spv
glslangValidator -V fragment.frag -o shaders/frag.spv

if %errorlevel% neq 0 (
    echo Shader compilation failed!
    pause
    exit /b %errorlevel%
)

echo Shaders compiled successfully!
::pause