@echo off
REM TDD Simple Test Build and Run Script
REM This script compiles and runs the simple TDD test

echo ========================================
echo TDD Simple Test - Build and Run
echo ========================================

cd /d "%~dp0"
echo Working directory: %CD%

REM Create build directory
if not exist build_tdd (
    echo Creating build directory...
    mkdir build_tdd
)

cd build_tdd

REM Configure CMake
echo Configuring CMake...
cmake .. -DCMAKE_TOOLCHAIN_FILE=../conan_toolchain.cmake -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_BUILD_TYPE=Release -P CMakeLists_TDD_Simple.txt
if %errorlevel% neq 0 (
    echo Error: CMake configuration failed
    echo.
    echo Note: This is expected for RED state!
    echo The test will fail because validateAmount() is not implemented.
    echo.
    cd /d "%~dp0"
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo Error: Build failed
    echo This is EXPECTED for RED state!
    echo.
    cd /d "%~dp0"
    pause
    exit /b 1
)

REM Run test
echo.
echo Running TDD test...
echo ========================================
Release\TDD_SimpleTest.exe --gtest_filter=TDD_*
echo ========================================

Release\TDD_SimpleTest.exe --gtest_filter=TDD_*
set TEST_RESULT=%errorlevel%

echo.
echo ========================================
if %TEST_RESULT% equ 0 (
    echo Result: TESTS PASSED (GREEN state)
) else (
    echo Result: TESTS FAILED (RED state - expected)
)
echo ========================================

cd /d "%~dp0"
pause
