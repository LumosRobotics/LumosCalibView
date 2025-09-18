cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, skipping main")
    return()
endif()

# Application source files
set(APP_SOURCE_FILES 
    main.cpp 
)

# Create standalone binary for development
add_executable(simple ${APP_SOURCE_FILES} ${MODULE_SOURCE_FILES})

# Qt6 setup for both targets
set_target_properties(simple PROPERTIES
    AUTOMOC ON
)

# Include directories for both targets
target_include_directories(simple PRIVATE
    ${CMAKE_SOURCE_DIR}/src/modules
    ${CMAKE_SOURCE_DIR}/third_party/cpython/Include
    ${CMAKE_SOURCE_DIR}/third_party/cpython
)

# Find required Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# Qt6 libraries for both targets
target_link_libraries(simple PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)
