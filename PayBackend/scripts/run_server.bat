@echo off
setlocal enabledelayedexpansion
REM Run script for Pay Plugin Backend

REM Parse command line arguments
set BUILD_MODE=Release
set PRESET=windows-msvc
if "%1"=="-debug" (
    set BUILD_MODE=Debug
    set PRESET=windows-msvc-debug
)
if "%1"=="-release" (
    set BUILD_MODE=Release
    set PRESET=windows-msvc
)

echo ========================================
echo Starting Pay Plugin Backend
echo Build Mode: %BUILD_MODE%
echo Preset:     %PRESET%
echo ========================================

REM Change to PayBackend directory
cd /d "%~dp0.."
echo Working directory: %CD%

REM Check if preset build directory exists
if not exist build\%PRESET% (
    echo Error: build\%PRESET% directory not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

cd build\%PRESET%

REM Load Conan environment if available
if exist conanrun.bat (
    echo Loading Conan environment...
    call conanrun.bat
)

REM Check if the requested build exists
if not exist %BUILD_MODE%\PayServer.exe (
    echo Error: PayServer.exe not found in build\%PRESET%\%BUILD_MODE%!
    echo Please run build.bat first to build the %BUILD_MODE% version.
    pause
    exit /b 1
)

echo Starting server (%BUILD_MODE% build)...
REM Run from the output directory so config.json/.env/certs are found in cwd
cd %BUILD_MODE%
PayServer.exe

:end
pause
