cmake_minimum_required(VERSION 3.10)
project(xp2gdl90)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the X-Plane SDK
set(XPLANE_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDK")

# Platform-specific settings
if(WIN32)
    set(PLUGIN_OUTPUT_FILENAME "win.xpl")
    add_definitions(-DIBM=1)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    if(CMAKE_CL_64)
        set(PLUGIN_OUTPUT_FILENAME "win_x64.xpl")
        set(XPLANE_PLATFORM_SUFFIX "_64")
    endif()
elseif(APPLE)
    set(PLUGIN_OUTPUT_FILENAME "mac.xpl")
    add_definitions(-DAPL=1)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64;arm64")
        set(PLUGIN_OUTPUT_FILENAME "mac_x64.xpl")
    endif()
elseif(UNIX)
    set(PLUGIN_OUTPUT_FILENAME "lin.xpl")
    add_definitions(-DLIN=1)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PLUGIN_OUTPUT_FILENAME "lin_x64.xpl")
        set(XPLANE_PLATFORM_SUFFIX "_64")
    endif()
endif()

# Include directories
include_directories(
    "${XPLANE_SDK_PATH}/CHeaders/XPLM"
    "${XPLANE_SDK_PATH}/CHeaders/Widgets"
    "${XPLANE_SDK_PATH}/CHeaders/Wrappers"
)

# Source files
set(SOURCES
    src/xp2gdl90.cpp
)

# Create the plugin library
add_library(xp2gdl90 SHARED ${SOURCES})

# Set the output name and properties
set_target_properties(xp2gdl90 PROPERTIES
    OUTPUT_NAME ${PLUGIN_OUTPUT_FILENAME}
    PREFIX ""
    SUFFIX ""
)

# Platform-specific linking
if(WIN32)
    # Link against X-Plane libraries and Windows socket library
    target_link_libraries(xp2gdl90
        "${XPLANE_SDK_PATH}/Libraries/Win/XPLM${XPLANE_PLATFORM_SUFFIX}.lib"
        "${XPLANE_SDK_PATH}/Libraries/Win/XPWidgets${XPLANE_PLATFORM_SUFFIX}.lib"
        ws2_32
    )
    
    # Use the exports definition file for proper Windows linking
    set_target_properties(xp2gdl90 PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/win_exports.def"
    )
    
elseif(APPLE)
    # Link against X-Plane frameworks
    find_library(XPLM_LIBRARY XPLM "${XPLANE_SDK_PATH}/Libraries/Mac")
    find_library(XPWIDGETS_LIBRARY XPWidgets "${XPLANE_SDK_PATH}/Libraries/Mac")
    
    target_link_libraries(xp2gdl90
        ${XPLM_LIBRARY}
        ${XPWIDGETS_LIBRARY}
    )
    
    # Set macOS-specific properties
    set_target_properties(xp2gdl90 PROPERTIES
        MACOSX_RPATH FALSE
    )
    
elseif(UNIX)
    # Link against X-Plane libraries
    target_link_libraries(xp2gdl90
        "${XPLANE_SDK_PATH}/Libraries/Lin/XPLM${XPLANE_PLATFORM_SUFFIX}.so"
        "${XPLANE_SDK_PATH}/Libraries/Lin/XPWidgets${XPLANE_PLATFORM_SUFFIX}.so"
    )
    
    # Set Linux-specific properties
    set_target_properties(xp2gdl90 PROPERTIES
        LINK_FLAGS "-shared -rdynamic -nodefaultlibs -undefined_warning"
    )
endif()

# Compiler-specific settings
if(MSVC)
    # MSVC specific flags
    target_compile_options(xp2gdl90 PRIVATE
        /Wall
        /wd4996  # Disable deprecated function warnings
        /wd4514  # Unreferenced inline function removed
        /wd4710  # Function not inlined
        /wd4711  # Function selected for automatic inline expansion
        /wd4820  # Padding added after data member
    )
else()
    # GCC/Clang specific flags
    target_compile_options(xp2gdl90 PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fvisibility=hidden
        -fPIC
    )
    
    if(APPLE)
        target_compile_options(xp2gdl90 PRIVATE
            -mmacosx-version-min=10.14
        )
    endif()
endif()

# Add preprocessor definitions
target_compile_definitions(xp2gdl90 PRIVATE
    XPLM301=1
    XPLM300=1
    XPLM210=1
    XPLM200=1
)

# Create output directory structure
set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/xp2gdl90")
file(MAKE_DIRECTORY "${PLUGIN_OUTPUT_DIR}")

# Copy plugin to output directory after build
add_custom_command(TARGET xp2gdl90 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PLUGIN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:xp2gdl90> "${PLUGIN_OUTPUT_DIR}/"
    COMMENT "Copying plugin to output directory"
)

# Installation rules
install(TARGETS xp2gdl90 
    DESTINATION xp2gdl90
)



# Print build information
message(STATUS "Building XP2GDL90 plugin")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Output filename: ${PLUGIN_OUTPUT_FILENAME}")
message(STATUS "X-Plane SDK path: ${XPLANE_SDK_PATH}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")