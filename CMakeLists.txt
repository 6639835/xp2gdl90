cmake_minimum_required(VERSION 3.16)
project(xp2gdl90 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(XP2GDL90_ENABLE_COVERAGE "Enable coverage instrumentation (tests only)" OFF)
option(XP2GDL90_ENABLE_SOCKET_OPS_TESTS "Expose socket ops helpers for tests" ON)

function(xp2gdl90_enable_coverage target_name)
    if(NOT XP2GDL90_ENABLE_COVERAGE)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target_name} PRIVATE
            -O0 -g -fprofile-instr-generate -fcoverage-mapping
        )
        get_target_property(_target_type ${target_name} TYPE)
        if(NOT _target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target_name} PRIVATE -fprofile-instr-generate)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
        get_target_property(_target_type ${target_name} TYPE)
        if(NOT _target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target_name} PRIVATE --coverage)
        endif()
    else()
        message(WARNING "Coverage requested but compiler '${CMAKE_CXX_COMPILER_ID}' is not supported.")
    endif()
endfunction()

if(MSVC)
    # Keep a consistent dynamic CRT across all targets/libraries.
    function(set_msvc_runtime target_name)
        set_property(TARGET ${target_name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endfunction()
endif()

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
endif()

# Plugin name
set(PLUGIN_NAME "xp2gdl90")

# SDK Path
set(SDK_PATH "${CMAKE_SOURCE_DIR}/SDK")

# Source files
set(CORE_SOURCES
    src/encoder_support.cpp
    src/foreflight_encoder.cpp
    src/foreflight_protocol.cpp
    src/gdl90_encoder.cpp
    src/protocol_utils.cpp
    src/settings.cpp
    src/settings_ui.cpp
    src/simple_json.cpp
    src/udp_receiver.cpp
    src/udp_broadcaster.cpp
)

set(PLUGIN_SOURCES
    src/main.cpp
)

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)

set(HEADERS
    include/xp2gdl90/foreflight_encoder.h
    include/xp2gdl90/foreflight_protocol.h
    include/xp2gdl90/gdl90_encoder.h
    include/xp2gdl90/protocol_utils.h
    include/xp2gdl90/settings.h
    include/xp2gdl90/settings_ui.h
    include/xp2gdl90/simple_json.h
    include/xp2gdl90/udp_receiver.h
    include/xp2gdl90/udp_broadcaster.h
)

# Core library (shared between plugin and tests)
add_library(xp2gdl90_core STATIC ${CORE_SOURCES} ${HEADERS})
target_include_directories(xp2gdl90_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
xp2gdl90_enable_coverage(xp2gdl90_core)
if(MSVC)
    set_msvc_runtime(xp2gdl90_core)
endif()
if(APPLE)
    set_target_properties(xp2gdl90_core PROPERTIES
        OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
    )
endif()

if(XP2GDL90_ENABLE_SOCKET_OPS_TESTS)
    target_compile_definitions(xp2gdl90_core PRIVATE XP2GDL90_ENABLE_SOCKET_OPS_TESTS=1)
endif()

# Platform-specific configurations
if(APPLE)
    # ====== macOS Configuration ======
    message(STATUS "Configuring for macOS...")
    
    # Set minimum macOS version
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    
    # X-Plane plugin settings
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    
    # Compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
    
    # X-Plane specific definitions
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${CMAKE_SOURCE_DIR}/include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES} ${IMGUI_SOURCES})
    
    # Set output name
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        OUTPUT_NAME "mac"
        SUFFIX ".xpl"
        PREFIX ""
    )
    
    # Link against XPLM framework
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        xp2gdl90_core
        "-F${SDK_PATH}/Libraries/Mac"
        "-framework XPLM"
        "-framework CoreFoundation"
        "-framework OpenGL"
    )
    
    # Export symbols
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        LINK_FLAGS "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/mac_exports.txt"
    )
    
elseif(WIN32)
    # ====== Windows Configuration ======
    message(STATUS "Configuring for Windows...")
    
    # X-Plane plugin settings
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    
    # Compiler flags
    if(MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /EHsc")
    endif()
    
    # X-Plane specific definitions
    add_definitions(-DAPL=0 -DIBM=1 -DLIN=0)
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${CMAKE_SOURCE_DIR}/include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES} ${IMGUI_SOURCES})
    if(MSVC)
        set_msvc_runtime(${PLUGIN_NAME})
    endif()
    
    # Set output name
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        OUTPUT_NAME "win"
        SUFFIX ".xpl"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    )
    
    # Runtime library set per-target for MSVC.
    
    # Link against XPLM library
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        xp2gdl90_core
        ${SDK_PATH}/Libraries/Win/XPLM_64.lib
        ws2_32  # Windows sockets
        opengl32
    )
    
    # Export symbols
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/win_exports.def"
    )
    
elseif(UNIX AND NOT APPLE)
    # ====== Linux Configuration ======
    message(STATUS "Configuring for Linux...")
    
    # X-Plane plugin settings
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    
    # Compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pipe")
    
    # X-Plane specific definitions
    add_definitions(-DAPL=0 -DIBM=0 -DLIN=1)
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM301=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${CMAKE_SOURCE_DIR}/include
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES} ${IMGUI_SOURCES})
    
    # Set output name
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        OUTPUT_NAME "lin"
        SUFFIX ".xpl"
        PREFIX ""
    )
    
    # Link against XPLM library
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        xp2gdl90_core
        ${SDK_PATH}/Libraries/Lin/XPLM_64.so
        dl  # Dynamic link library
        GL
    )
    
    # Export symbols
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linux_exports.txt"
    )
endif()

# Installation
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION "${CMAKE_SOURCE_DIR}/build/xp2gdl90"
    RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/build/xp2gdl90"
)

# Output compilation information
message(STATUS "Project: ${PLUGIN_NAME}")
message(STATUS "SDK Path: ${SDK_PATH}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
if(APPLE)
    message(STATUS "Target Architectures: ${CMAKE_OSX_ARCHITECTURES}")
    message(STATUS "macOS Deployment Target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

# Tests
option(XP2GDL90_BUILD_TESTS "Build XP2GDL90 tests" OFF)
if(XP2GDL90_BUILD_TESTS)
    enable_testing()
    add_executable(xp2gdl90_tests
        tests/test_foreflight_encoder.cpp
        tests/test_foreflight_protocol.cpp
        tests/test_main.cpp
        tests/test_gdl90_encoder.cpp
        tests/test_protocol_utils.cpp
        tests/test_settings.cpp
        tests/test_settings_ui.cpp
        tests/test_udp_broadcaster.cpp
    )
    target_link_libraries(xp2gdl90_tests PRIVATE xp2gdl90_core)
    xp2gdl90_enable_coverage(xp2gdl90_tests)
    if(XP2GDL90_ENABLE_SOCKET_OPS_TESTS)
        target_compile_definitions(xp2gdl90_tests PRIVATE XP2GDL90_ENABLE_SOCKET_OPS_TESTS=1)
    endif()
    if(MSVC)
        set_msvc_runtime(xp2gdl90_tests)
    endif()
    add_test(NAME xp2gdl90_tests COMMAND xp2gdl90_tests)
endif()

# Package creation
set(CPACK_PACKAGE_NAME "${PLUGIN_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
