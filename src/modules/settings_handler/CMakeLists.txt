# Settings Handler Module
cmake_minimum_required(VERSION 3.14)

# Determine platform-specific source files
if(APPLE)
    set(PLATFORM_SOURCES platform_path_macos.cpp)
elseif(WIN32)
    set(PLATFORM_SOURCES platform_path_windows.cpp)
else()
    # Linux/Unix
    set(PLATFORM_SOURCES platform_path_linux.cpp)
endif()

# Create a static library for the settings handler
add_library(settings_handler STATIC
    settings_handler.cpp
    settings_handler.h
    ${PLATFORM_SOURCES}
)

# Set include directories for the library
target_include_directories(settings_handler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required system libraries
target_link_libraries(settings_handler
    pthread
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(settings_handler shell32)
endif()

# Set C++ standard
target_compile_features(settings_handler PUBLIC cxx_std_17)

# Add tests subdirectory
add_subdirectory(test)