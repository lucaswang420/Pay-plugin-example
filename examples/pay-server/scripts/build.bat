@echo off
setlocal enabledelayedexpansion
REM Build script for Pay Plugin Backend (Conan 2 + CMake presets workflow)

echo ========================================
echo Pay Plugin Backend Build Script
echo ========================================

REM ========================================
REM Kill any running PayServer processes
REM ========================================
echo Checking for running PayServer processes...
taskkill /F /IM PayServer.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo Killed running PayServer.exe process
    timeout /t 1 /nobreak >nul
) else (
    echo No running PayServer.exe process found
)

REM Change to the repository root (conanfile.py / CMakePresets.json live there)
cd /d "%~dp0..\..\.."
echo Working directory: %CD%

REM Default build type
set BUILD_TYPE=Release
set PRESET=windows-msvc

REM Parse command line arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=Debug
    set PRESET=windows-msvc-debug
    shift
    goto parse_args
)
if /i "%1"=="-release" (
    set BUILD_TYPE=Release
    set PRESET=windows-msvc
    shift
    goto parse_args
)
echo Unknown option: %1
echo Usage: %0 [-debug|-release]
echo   -debug     Build debug version (preset windows-msvc-debug)
echo   -release   Build release version (default, preset windows-msvc)
exit /b 1
:end_parse

echo Building with configuration:
echo   Build Type: %BUILD_TYPE%
echo   Preset:     %PRESET%
echo.

REM Install dependencies (generates build\%PRESET%\conan_toolchain.cmake)
echo Installing dependencies via Conan...
conan install . --output-folder=build/%PRESET% -s build_type=%BUILD_TYPE% -s compiler.cppstd=17 --build=missing
if %errorlevel% neq 0 (
    echo Error: Conan install failed
    exit /b 1
)

REM Configure with CMake preset
echo Configuring project...
cmake --preset %PRESET%
if %errorlevel% neq 0 (
    echo Error: CMake configuration failed
    exit /b 1
)

REM Build with CMake preset
echo Building project...
cmake --build --preset %PRESET%
if %errorlevel% neq 0 (
    echo Error: Build failed
    exit /b 1
)

set OUT_DIR=build\%PRESET%\examples\pay-server\%BUILD_TYPE%
set TEST_OUT_DIR=build\%PRESET%\tests\%BUILD_TYPE%
set HOST_DIR=examples\pay-server

REM Copy config.json and .env to output directory
echo Copying configuration files...
robocopy %HOST_DIR% %OUT_DIR% config.json .env /NFL /NDL /NJH /NJS /NP
if %ERRORLEVEL% GEQ 8 (
    echo Error: Failed to copy config files
    exit /b 1
)

REM Copy certs directory if it exists
if exist "%HOST_DIR%\certs" (
    robocopy %HOST_DIR%\certs %OUT_DIR%\certs\ /E /NFL /NDL /NJH /NJS /NP
    if !ERRORLEVEL! GEQ 8 (
        echo Error: Failed to copy certs directory
        exit /b 1
    )
)

REM Also copy to test output directory
echo Copying configuration files to test directory...
if exist "%TEST_OUT_DIR%" (
    robocopy %HOST_DIR% %TEST_OUT_DIR% config.json .env /NFL /NDL /NJH /NJS /NP
    if exist "%HOST_DIR%\certs" (
        robocopy %HOST_DIR%\certs %TEST_OUT_DIR%\certs\ /E /NFL /NDL /NJH /NJS /NP
    )
)

echo ========================================
echo Build complete!
echo   Server: %OUT_DIR%\PayServer.exe
echo   Tests:  %TEST_OUT_DIR%\PayBackendTests.exe
echo ========================================
exit /b 0
