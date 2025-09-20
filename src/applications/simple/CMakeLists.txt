cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, skipping main")
    return()
endif()

set(MODULE_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/modules/view_angles.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/plot_view.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/data_receiver.cpp
    ${CMAKE_SOURCE_DIR}/src/modules/multi_plot_container.cpp
)

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
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network OpenGL OpenGLWidgets)

# Qt6 libraries for both targets
target_link_libraries(simple PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
