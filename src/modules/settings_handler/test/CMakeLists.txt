# Settings Handler Tests
cmake_minimum_required(VERSION 3.14)

# Create test executable
add_executable(settings_handler_test
    settings_handler_test.cpp
)

# Link against settings_handler module and gtest
target_link_libraries(settings_handler_test
    settings_handler
    ${GTEST_LIB_FILES}
)

# Set C++ standard
target_compile_features(settings_handler_test PUBLIC cxx_std_17)

# Add test to CTest
add_test(NAME settings_handler_test COMMAND settings_handler_test)