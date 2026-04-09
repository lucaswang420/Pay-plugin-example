@echo off
REM Build script for Pay Plugin Backend

echo ========================================
echo Pay Plugin Backend Build Script
echo ========================================

REM Check if build directory exists
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

REM Install dependencies and configure
echo Installing dependencies...
conan install .. --build=missing -s build_type=Release

REM Configure with CMake
echo Configuring project...
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake

REM Build
echo Building project...
cmake --build . --config Release

echo ========================================
echo Build complete!
echo ========================================
pause
