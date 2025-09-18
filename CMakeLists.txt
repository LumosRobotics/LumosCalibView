# Minimum CMake version required
cmake_minimum_required(VERSION 3.14)

# Project name and languages
project(LumosWorkspace C CXX)

# Set the C++ standard (adjust according to your needs)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug options
option(ENABLE_DEBUG_PORT "Enable debug TCP port for GUI testing" ON)

# Optionally enable debugging support
set(CMAKE_BUILD_TYPE Debug)

# Optionally define a version
set(PROJECT_VERSION "1.0.0")

# Googletest
link_directories(${CMAKE_SOURCE_DIR}/third_party/googletest/lib)
include_directories(${CMAKE_SOURCE_DIR}/third_party/googletest/googletest/include)

set(GTEST_LIB_FILES libgtest.a
                    libgtest_main.a
                    pthread)

# Qt6
set(CMAKE_PREFIX_PATH "/usr/local/opt/qt/lib/cmake")
find_package(Qt6 COMPONENTS Core Widgets QUIET)

# Nlohmann
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann/include)

add_compile_options(-Wall -Wextra -pedantic)

# Only add repl_gui if Qt6 is found
if(Qt6_FOUND)
    add_subdirectory(src/applications/simple)
    message(STATUS "Building repl_gui with Qt6")
else()
    message(STATUS "Qt6 not found, skipping simple app")
endif()
