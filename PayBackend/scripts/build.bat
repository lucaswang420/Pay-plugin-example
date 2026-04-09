@echo off
setlocal enabledelayedexpansion
REM Build script for Pay Plugin Backend

echo ========================================
echo Pay Plugin Backend Build Script
echo ========================================

REM Change to PayBackend directory
cd /d "%~dp0.."
echo Working directory: %CD%

REM Default build type
set BUILD_TYPE=Release

REM Parse command line arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%1"=="-release" (
    set BUILD_TYPE=Release
    shift
    goto parse_args
)
echo Unknown option: %1
echo Usage: %0 [-debug|-release]
echo   -debug     Build debug version
echo   -release   Build release version (default)
exit /b 1
:end_parse

echo Building with configuration:
echo   Build Type: %BUILD_TYPE%
echo.

REM Check if build directory exists
if not exist build (
    echo Creating build directory...
    mkdir build
)

cd build

REM Install dependencies and configure
echo Installing dependencies...
conan install .. --build=missing -s build_type=%BUILD_TYPE%
if %errorlevel% neq 0 (
    echo Error: Conan install failed
    cd /d "%~dp0.."
    exit /b 1
)

REM Configure with CMake
echo Configuring project...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
if %errorlevel% neq 0 (
    echo Error: CMake configuration failed
    cd /d "%~dp0.."
    exit /b 1
)

REM Build
echo Building project...
cmake --build . --config %BUILD_TYPE%
if %errorlevel% neq 0 (
    echo Error: Build failed
    cd /d "%~dp0.."
    exit /b 1
)

REM Copy config.json to build directory
echo Copying configuration files...
robocopy .. %BUILD_TYPE% config.json /NFL /NDL /NJH /NJS /NP
if %ERRORLEVEL% GEQ 8 (
    echo Error: Failed to copy config.json
    cd /d "%~dp0.."
    exit /b 1
)

echo ========================================
echo Build complete!
echo ========================================
cd /d "%~dp0.."
pause
