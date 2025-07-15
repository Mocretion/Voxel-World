@echo off
cd /d "%~dp0"

echo Compiling bgfx shaders...

:: Set bgfx tools path (adjust as needed)
set BGFX_DIR="D:\VS C# Projects\Voxel World\external\bgfx"
set SHADERC=%BGFX_DIR%\.build\win64_vs2022\bin\shadercRelease.exe
set INCLUDE_PATH="D:\VS C# Projects\Voxel World\external\bgfx\src"

:: Create output directory
if not exist shaders mkdir shaders

:: Compile vertex shader
%SHADERC% -f code\shader\vs_basic.sc -o shaders\vs_basic.bin --platform windows --type vertex --varyingdef code\shader\varying.def.sc --profile spirv -i "D:\VS C# Projects\Voxel World\external\bgfx\src"

:: Compile fragment shader  
%SHADERC% -f code\shader\fs_basic.sc -o shaders\fs_basic.bin --platform windows --type fragment --varyingdef code\shader\varying.def.sc --profile spirv -i "D:\VS C# Projects\Voxel World\external\bgfx\src"

if %errorlevel% neq 0 (
    echo Shader compilation failed!
    pause
    exit /b %errorlevel%
)

echo bgfx shaders compiled successfully!
pause