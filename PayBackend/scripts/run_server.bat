@echo off
REM Run script for Pay Plugin Backend

echo ========================================
echo Starting Pay Plugin Backend
echo ========================================

cd build

if not exist Release\PayBackend.exe (
    echo Error: PayBackend.exe not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

echo Starting server...
Release\PayBackend.exe

pause
