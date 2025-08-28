# XP2GDL90 Test Suite CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# Find Google Test framework
find_package(GTest QUIET)

# If GTest is not found, download and build it
if(NOT GTest_FOUND)
    message(STATUS "Google Test not found. Downloading and building...")
    
    # Download and unpack googletest at configure time
    configure_file(GoogleTestDownload.cmake.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                     ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                     EXCLUDE_FROM_ALL)
endif()

# Test include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/XPLM
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/Widgets
)

# Create mock X-Plane SDK for testing
add_library(xplane_mock STATIC
    mocks/XPLMMock.cpp
    mocks/XPLMDataAccessMock.cpp
    mocks/XPLMProcessingMock.cpp
)

target_include_directories(xplane_mock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/XPLM
)

# Test source files
set(TEST_SOURCES
    unit/test_gdl90_encoding.cpp
    unit/test_flight_data.cpp
    unit/test_network_layer.cpp
    unit/test_plugin_lifecycle.cpp
    integration/test_full_workflow.cpp
)

# Create test executable
add_executable(xp2gdl90_tests ${TEST_SOURCES})

# Link against Google Test, mock library, and system libraries
if(GTest_FOUND)
    target_link_libraries(xp2gdl90_tests 
        GTest::gtest 
        GTest::gtest_main
        xplane_mock
    )
else()
    target_link_libraries(xp2gdl90_tests 
        gtest 
        gtest_main
        xplane_mock
    )
endif()

# Platform-specific linking for network functionality
if(WIN32)
    target_link_libraries(xp2gdl90_tests ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(xp2gdl90_tests pthread)
endif()

# Compiler-specific settings for tests
if(MSVC)
    target_compile_options(xp2gdl90_tests PRIVATE /W4)
else()
    target_compile_options(xp2gdl90_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add tests to CTest
enable_testing()

# Individual test discovery
add_test(NAME GDL90EncodingTests COMMAND xp2gdl90_tests --gtest_filter=GDL90Encoding*)
add_test(NAME FlightDataTests COMMAND xp2gdl90_tests --gtest_filter=FlightData*)
add_test(NAME NetworkLayerTests COMMAND xp2gdl90_tests --gtest_filter=NetworkLayer*)
add_test(NAME PluginLifecycleTests COMMAND xp2gdl90_tests --gtest_filter=PluginLifecycle*)
add_test(NAME IntegrationTests COMMAND xp2gdl90_tests --gtest_filter=Integration*)

# Set test properties
set_tests_properties(
    GDL90EncodingTests FlightDataTests NetworkLayerTests PluginLifecycleTests IntegrationTests
    PROPERTIES
    TIMEOUT 30
    FAIL_REGULAR_EXPRESSION "FAILED|ERROR"
)

# Memory leak detection on supported platforms
if(UNIX AND NOT APPLE)
    find_program(VALGRIND_EXECUTABLE valgrind)
    if(VALGRIND_EXECUTABLE)
        add_test(NAME MemoryLeakCheck
                COMMAND ${VALGRIND_EXECUTABLE} 
                --tool=memcheck 
                --leak-check=full 
                --show-leak-kinds=all
                --track-origins=yes
                --error-exitcode=1
                $<TARGET_FILE:xp2gdl90_tests>)
    endif()
endif()

# Coverage support (GCC/Clang only)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
    if(ENABLE_COVERAGE)
        target_compile_options(xp2gdl90_tests PRIVATE --coverage -O0)
        target_link_options(xp2gdl90_tests PRIVATE --coverage)
    endif()
endif()

# Custom target for running tests with verbose output
add_custom_target(test_verbose
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS xp2gdl90_tests
    COMMENT "Running tests with verbose output"
)

# Custom target for generating coverage report
if(CMAKE_COMPILER_IS_GNUCXX AND ENABLE_COVERAGE)
    find_program(GCOV_EXECUTABLE gcov)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        add_custom_target(coverage
            COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --remove coverage.info '/usr/*' --output-file coverage.info
            COMMAND ${LCOV_EXECUTABLE} --list coverage.info
            COMMAND ${GENHTML_EXECUTABLE} -o coverage coverage.info
            DEPENDS xp2gdl90_tests
            COMMENT "Generating code coverage report"
        )
    endif()
endif()
