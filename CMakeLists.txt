cmake_minimum_required(VERSION 3.16)
project(xp2gdl90 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Keep a consistent dynamic CRT across all targets/libraries.
    function(set_msvc_runtime target_name)
        set_property(TARGET ${target_name} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endfunction()
endif()

# Plugin name
set(PLUGIN_NAME "xp2gdl90")

# SDK Path
set(SDK_PATH "${CMAKE_SOURCE_DIR}/SDK")

# Source files
set(CORE_SOURCES
    src/gdl90_encoder.cpp
    src/udp_broadcaster.cpp
    src/config.cpp
)

set(PLUGIN_SOURCES
    src/main.cpp
)

set(HEADERS
    include/xp2gdl90/gdl90_encoder.h
    include/xp2gdl90/udp_broadcaster.h
    include/xp2gdl90/config.h
)

# Core library (shared between plugin and tests)
add_library(xp2gdl90_core STATIC ${CORE_SOURCES} ${HEADERS})
target_include_directories(xp2gdl90_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(MSVC)
    set_msvc_runtime(xp2gdl90_core)
endif()
if(APPLE)
    set_target_properties(xp2gdl90_core PROPERTIES
        OSX_ARCHITECTURES "x86_64;arm64"
    )
endif()

# Platform-specific configurations
if(APPLE)
    # ====== macOS Configuration ======
    message(STATUS "Configuring for macOS...")
    
    # Set minimum macOS version
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    
    # Universal binary (Intel + Apple Silicon)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    
    # X-Plane plugin settings
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    
    # Compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wformat -Wformat-security")
    
    # X-Plane specific definitions
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${SDK_PATH}/CHeaders/Widgets
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})
    
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
        "-framework XPWidgets"
        "-framework CoreFoundation"
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
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${SDK_PATH}/CHeaders/Widgets
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})
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
        ${SDK_PATH}/Libraries/Win/XPWidgets_64.lib
        ws2_32  # Windows sockets
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
    add_definitions(-DXPLM200=1 -DXPLM300=1 -DXPLM400=1 -DXPLM410=1 -DXPLM411=1)
    
    # Include directories
    include_directories(
        ${SDK_PATH}/CHeaders/XPLM
        ${SDK_PATH}/CHeaders/Widgets
        ${CMAKE_SOURCE_DIR}/include
    )
    
    # Create plugin library
    add_library(${PLUGIN_NAME} MODULE ${PLUGIN_SOURCES})
    
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
        ${SDK_PATH}/Libraries/Lin/XPWidgets_64.so
        dl  # Dynamic link library
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

# Copy config file
install(FILES "${CMAKE_SOURCE_DIR}/xp2gdl90.ini"
    DESTINATION "${CMAKE_SOURCE_DIR}/build/xp2gdl90"
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
        tests/test_main.cpp
        tests/test_config.cpp
        tests/test_gdl90_encoder.cpp
        tests/test_udp_broadcaster.cpp
    )
    target_link_libraries(xp2gdl90_tests PRIVATE xp2gdl90_core)
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
