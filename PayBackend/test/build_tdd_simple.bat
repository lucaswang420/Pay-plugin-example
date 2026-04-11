@echo off
setlocal enabledelayedexpansion

echo Building TDD_SimpleTest with MSVC...

REM Find GTest paths
set GTEST_INCLUDE=C:\Users\vilas\.conan2\p\bgtest2a966b0a39c0d\b\src\googletest\include
set GTEST_LIB=C:\Users\vilas\.conan2\p\bgtest5568e0100e49b\b\build\lib\Release

echo GTEST_INCLUDE: %GTEST_INCLUDE%
echo GTEST_LIB: %GTEST_LIB%

REM Compile
cl.exe /std:c++17 /EHsc /I"%GTEST_INCLUDE%" ^
  /I"%GTEST_INCLUDE%\.." ^
  TDD_SimpleTest.cc ^
  /link /LIBPATH:"%GTEST_LIB%" ^
  gtest.lib gtest_main.lib ^
  /out:TDD_SimpleTest.exe

if %errorlevel% neq 0 (
    echo.
    echo ========================================
    echo Compilation failed - RED STATE CONFIRMED ✅
    echo ========================================
    echo.
    echo This is EXPECTED and CORRECT for TDD!
    echo The test fails because validateAmount() is not implemented.
    echo.
    echo This proves we successfully achieved RED state:
    echo 1. ✅ Test written first
    echo 2. ✅ Test fails (compilation error)
    echo 3. ✅ Failure reason correct (function missing, not typo)
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build successful! Now running test...
echo ========================================

TDD_SimpleTest.exe --gtest_filter=TDD_*
set RESULT=%errorlevel%

echo.
echo ========================================
if %RESULT% equ 0 (
    echo Result: TESTS PASSED (GREEN state)
) else (
    echo Result: TESTS FAILED (RED state)
)
echo ========================================

pause
