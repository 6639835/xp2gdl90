# XP2GDL90 Benchmarks CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# Find Google Benchmark
find_package(benchmark REQUIRED)

# Create benchmark executable
set(BENCHMARK_SOURCES
    bench_gdl90_encoding.cpp
    bench_flight_data.cpp
    bench_network_layer.cpp
    bench_full_workflow.cpp
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/mocks
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/XPLM
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/Widgets
)

# Create benchmark executable
add_executable(xp2gdl90_benchmarks ${BENCHMARK_SOURCES})

# Link against Google Benchmark only
target_link_libraries(xp2gdl90_benchmarks 
    benchmark::benchmark
    benchmark::benchmark_main
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(xp2gdl90_benchmarks ws2_32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(xp2gdl90_benchmarks pthread)
endif()

# Compiler-specific settings
if(MSVC)
    target_compile_options(xp2gdl90_benchmarks PRIVATE /O2)
else()
    target_compile_options(xp2gdl90_benchmarks PRIVATE -O3 -DNDEBUG)
endif()

# Custom target for running benchmarks
add_custom_target(run_benchmarks
    COMMAND xp2gdl90_benchmarks --benchmark_format=json --benchmark_out=benchmark_results.json
    DEPENDS xp2gdl90_benchmarks
    COMMENT "Running performance benchmarks"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Custom target for benchmark comparison
add_custom_target(benchmark_compare
    COMMAND ${CMAKE_COMMAND} -E echo "Run 'python scripts/benchmark_compare.py' to compare results"
    COMMENT "Compare benchmark results"
)

# Install benchmarks (optional)
install(TARGETS xp2gdl90_benchmarks
    RUNTIME DESTINATION bin/benchmarks
)
