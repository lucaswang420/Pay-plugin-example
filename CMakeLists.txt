cmake_minimum_required(VERSION 3.21)
project(drogon-pay VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

option(DROGON_PAY_BUILD_EXAMPLES "Build the example host server (examples/pay-server)" ON)
option(DROGON_PAY_BUILD_TESTS "Build the drogon-pay test suite" ON)
# CI historically passes -DBUILD_TESTS=ON; honor it as an alias so the flag
# remains a real switch without touching workflow files.
if(DEFINED BUILD_TESTS)
    set(DROGON_PAY_BUILD_TESTS ${BUILD_TESTS})
endif()

add_subdirectory(libs/drogon-pay)

# Tests deliberately reuse the example host's utils/controllers (ConfigLoader,
# StartupValidator, health/metrics endpoints) to replicate the host startup
# flow, so building tests implies building the example host targets.
if(DROGON_PAY_BUILD_TESTS AND NOT DROGON_PAY_BUILD_EXAMPLES)
    message(STATUS "DROGON_PAY_BUILD_TESTS=ON forces DROGON_PAY_BUILD_EXAMPLES=ON")
    set(DROGON_PAY_BUILD_EXAMPLES ON)
endif()

if(DROGON_PAY_BUILD_EXAMPLES)
    add_subdirectory(examples/pay-server)
endif()

if(DROGON_PAY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
