cmake_minimum_required(VERSION 3.5)
project(PayServer CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${PROJECT_NAME} main.cc)

# ##############################################################################
# add_subdirectory(drogon)
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines

# For Debug/Release builds, ensure Conan dependencies are found before Drogon
if(CMAKE_BUILD_TYPE)
    # Find Conan packages first so Drogon can find them
    find_package(jsoncpp REQUIRED QUIET)
    find_package(brotli REQUIRED QUIET)
    find_package(SQLite3 REQUIRED QUIET)
    find_package(hiredis REQUIRED QUIET)
    find_package(ZLIB REQUIRED QUIET)

    # Set Jsoncpp paths for Drogon's FindJsoncpp.cmake
    if(jsoncpp_FOUND)
        set(JSONCPP_INCLUDE_DIRS ${jsoncpp_INCLUDE_DIRS} CACHE PATH "Jsoncpp include dirs" FORCE)
        set(JSONCPP_LIBRARIES ${jsoncpp_LIBRARIES} CACHE STRING "Jsoncpp libraries" FORCE)

        # Create Jsoncpp_lib target that Drogon expects
        if(NOT TARGET Jsoncpp_lib)
            add_library(Jsoncpp_lib INTERFACE IMPORTED)
            set_target_properties(Jsoncpp_lib PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${jsoncpp_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "JsonCpp::JsonCpp"
            )
            message(STATUS "Created Jsoncpp_lib target for Drogon: ${jsoncpp_INCLUDE_DIRS}")
        endif()
    endif()

    # Create other library targets that Drogon expects
    function(create_drogon_compatible_target PACKAGE_NAME TARGET_NAME CONAN_TARGET)
        if(${PACKAGE_NAME}_FOUND AND NOT TARGET ${TARGET_NAME})
            add_library(${TARGET_NAME} INTERFACE IMPORTED)
            set_target_properties(${TARGET_NAME} PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${${PACKAGE_NAME}_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${CONAN_TARGET}"
            )
            message(STATUS "Created ${TARGET_NAME} target for Drogon: ${${PACKAGE_NAME}_INCLUDE_DIRS}")
        endif()
    endfunction()

    create_drogon_compatible_target(brotli Brotli_lib brotli::brotli)
    create_drogon_compatible_target(SQLite3 SQLite3_lib SQLite::SQLite3)
    create_drogon_compatible_target(hiredis Hiredis_lib hiredis::hiredis)
    create_drogon_compatible_target(ZLIB ZLIB_lib ZLIB::ZLIB)
endif()

find_package(Drogon CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon OpenSSL::Crypto OpenSSL::SSL)

# ##############################################################################

if (CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "c++17 or higher is required")
elseif (CMAKE_CXX_STANDARD LESS 20)
    message(STATUS "use c++17")
else ()
    message(STATUS "use c++20")
endif ()

aux_source_directory(controllers CTL_SRC)
aux_source_directory(filters FILTER_SRC)
aux_source_directory(plugins PLUGIN_SRC)
aux_source_directory(utils UTILS_SRC)
aux_source_directory(models MODEL_SRC)
aux_source_directory(services SERVICE_SRC)

drogon_create_views(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/views
                    ${CMAKE_CURRENT_BINARY_DIR})


target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/models)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/models/orm_compat.h")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        _CRT_SECURE_NO_WARNINGS
    )
endif()

target_sources(${PROJECT_NAME}
               PRIVATE
               ${SRC_DIR}
               ${CTL_SRC}
               ${FILTER_SRC}
               ${PLUGIN_SRC}
               ${UTILS_SRC}
               ${MODEL_SRC}
               ${SERVICE_SRC})

enable_testing()
add_subdirectory(test)
