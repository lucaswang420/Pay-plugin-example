#!/bin/bash
# Simple TDD test build script

echo "Building TDD_SimpleTest..."

# Find GTest include and lib paths (from Conan)
GTEST_INCLUDE=$(find ~/.conan2 -name "gtest" -type d 2>/dev/null | head -1)
GTEST_LIB=$(find ~/.conan2 -name "gtest.lib" -o -name "libgtest.a" 2>/dev/null | head -1)

echo "GTEST_INCLUDE: $GTEST_INCLUDE"
echo "GTEST_LIB: $GTEST_LIB"

if [ -z "$GTEST_INCLUDE" ] || [ -z "$GTEST_LIB" ]; then
    echo "Error: Could not find GTest"
    echo "This is expected for RED state!"
    exit 1
fi

# Compile the test
g++ -std=c++17 \
    -I"$GTEST_INCLUDE" \
    -I"$GTEST_INCLUDE/../" \
    TDD_SimpleTest.cc \
    -L"$GTEST_LIB/../.." \
    -lgtest -lgmock -lpthread \
    -o TDD_SimpleTest

if [ $? -ne 0 ]; then
    echo "Compilation failed - This is RED state ✅"
    echo "The test will fail because validateAmount() is not implemented"
    exit 1
fi

echo "Build successful! Now running test..."
./TDD_SimpleTest --gtest_filter=TDD_*
