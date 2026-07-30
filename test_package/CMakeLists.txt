cmake_minimum_required(VERSION 3.15)
project(drogon_pay_test_package CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(DrogonPay CONFIG REQUIRED)

add_executable(test_package main.cc)
target_link_libraries(test_package PRIVATE DrogonPay::DrogonPay)
