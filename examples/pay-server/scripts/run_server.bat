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

REM Change to repository root (script lives in examples/pay-server/scripts/)
cd /d "%~dp0..\..\.."
echo Working directory: %CD%

REM Check if preset build directory exists
if not exist build\%PRESET% (
    echo Error: build\%PRESET% directory not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

REM Load Conan environment if available
if exist build\%PRESET%\build\generators\conanrun.bat (
    echo Loading Conan environment...
    call build\%PRESET%\build\generators\conanrun.bat
)

set OUT_DIR=build\%PRESET%\examples\pay-server\%BUILD_MODE%

REM Check if the requested build exists
if not exist %OUT_DIR%\PayServer.exe (
    echo Error: PayServer.exe not found in %OUT_DIR%!
    echo Please run build.bat first to build the %BUILD_MODE% version.
    pause
    exit /b 1
)

echo Starting server (%BUILD_MODE% build)...
REM Run from the output directory so config.json/.env/certs are found in cwd
cd %OUT_DIR%
PayServer.exe

:end
pause
