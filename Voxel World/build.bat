@echo off
cd /d "%~dp0"
if not exist build mkdir build
cd build
cmake -DVCPKG_ROOT="D:/VS C# Projects/vcpkg" -DCMAKE_TOOLCHAIN_FILE="D:/VS C# Projects/vcpkg/scripts/buildsystems/vcpkg.cmake" ..
if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %errorlevel%
)
cmake --build .

cd ..
call compile_shaders.bat

cd "D:\VS C# Projects\Voxel World\Voxel World\build\Debug"
mkdir "shaders"
xcopy /s "D:\VS C# Projects\Voxel World\Voxel World\shaders" "D:\VS C# Projects\Voxel World\Voxel World\build\Debug\shaders" /y
VoxelWorld.exe
pause