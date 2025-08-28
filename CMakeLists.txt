cmake_minimum_required(VERSION 3.10)
project(xp2gdl90)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the X-Plane SDK
set(XPLANE_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/SDK")

# Platform-specific settings
if(WIN32)
    # Windows configuration
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    add_definitions(-DAPL=0 -DIBM=1 -DLIN=0)
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
    set(XPLANE_PLATFORM_SUFFIX "_64")
elseif(APPLE)
    # macOS configuration
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    add_definitions(-DAPL=1 -DIBM=0 -DLIN=0)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
elseif(UNIX)
    # Linux configuration
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".xpl")
    add_definitions(-DAPL=0 -DIBM=0 -DLIN=1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -fvisibility=hidden")
    set(XPLANE_PLATFORM_SUFFIX "_64")
endif()

# Check for OpenGL availability (required for ImGui UI)
find_package(OpenGL QUIET)
if(OpenGL_FOUND OR OPENGL_FOUND)
    set(HAVE_OPENGL TRUE)
    message(STATUS "OpenGL found - ImGui UI will be enabled")
else()
    set(HAVE_OPENGL FALSE)
    message(WARNING "OpenGL not found - ImGui UI will be disabled, building core functionality only")
endif()

# Include directories
include_directories(
    "${XPLANE_SDK_PATH}/CHeaders/XPLM"
    "${XPLANE_SDK_PATH}/CHeaders/Widgets"
    "${XPLANE_SDK_PATH}/CHeaders/Wrappers"
)

# Main plugin source files
set(PLUGIN_SOURCES
    src/xp2gdl90.cpp
)

# Conditionally add ImGui sources if OpenGL is available
set(SOURCES ${PLUGIN_SOURCES})

if(HAVE_OPENGL)
    # Add ImGui include directories
    include_directories(
        "src/imgui"
        "src/ui"
        "src/imgwindow"
    )
    
    # ImGui source files
    set(IMGUI_SOURCES
        src/imgui/imgui.cpp
        src/imgui/imgui_demo.cpp
        src/imgui/imgui_draw.cpp
        src/imgui/imgui_tables.cpp
        src/imgui/imgui_widgets.cpp
        src/imgui/imgui_impl_opengl2.cpp
        src/imgui/imgui_impl_xplane.cpp
    )
    
    # ImgWindow system source files
    set(IMGWINDOW_SOURCES
        src/imgwindow/img_font_atlas.cpp
        src/imgwindow/img_window.cpp
        src/imgwindow/xp_img_window.cpp
    )
    
    # UI source files
    set(UI_SOURCES
        src/ui/ImGuiManager.cpp
        src/ui/ConfigWindow.cpp
        src/ui/StatusWindow.cpp
    )
    
    # Add ImGui sources to build
    list(APPEND SOURCES ${IMGUI_SOURCES} ${IMGWINDOW_SOURCES} ${UI_SOURCES})
    
    # Define preprocessor macro to enable ImGui in the code
    add_definitions(-DHAVE_IMGUI=1)
else()
    # Define preprocessor macro to disable ImGui in the code
    add_definitions(-DHAVE_IMGUI=0)
endif()

# Create the plugin library
add_library(xp2gdl90 SHARED ${SOURCES})

# Set platform-specific output names and properties
if(WIN32)
    set_target_properties(xp2gdl90 PROPERTIES
        OUTPUT_NAME "win"
        SUFFIX ".xpl"
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
    )
elseif(APPLE)
    set_target_properties(xp2gdl90 PROPERTIES
        OUTPUT_NAME "mac"
        SUFFIX ".xpl"
        PREFIX ""
    )
elseif(UNIX)
    set_target_properties(xp2gdl90 PROPERTIES
        OUTPUT_NAME "lin"
        SUFFIX ".xpl"
        PREFIX ""
    )
endif()

# Platform-specific linking
if(WIN32)
    # Link against X-Plane libraries, Windows socket library, and OpenGL for ImGui
    target_link_libraries(xp2gdl90
        "${XPLANE_SDK_PATH}/Libraries/Win/XPLM${XPLANE_PLATFORM_SUFFIX}.lib"
        "${XPLANE_SDK_PATH}/Libraries/Win/XPWidgets${XPLANE_PLATFORM_SUFFIX}.lib"
        ws2_32
        opengl32
    )
    
    # Use Windows symbol export file
    set_target_properties(xp2gdl90 PROPERTIES
        LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/win_exports.def"
    )
    
elseif(APPLE)
    # Link X-Plane frameworks and OpenGL for ImGui
    target_link_libraries(xp2gdl90
        "${XPLANE_SDK_PATH}/Libraries/Mac/XPLM.framework"
        "${XPLANE_SDK_PATH}/Libraries/Mac/XPWidgets.framework"
        "-framework OpenGL"
    )
    
    # Set framework search paths
    set_target_properties(xp2gdl90 PROPERTIES
        FRAMEWORK_SEARCH_PATHS "${XPLANE_SDK_PATH}/Libraries/Mac"
        LINK_FLAGS "-F${XPLANE_SDK_PATH}/Libraries/Mac"
    )
    
    # Ensure necessary symbols are exported
    set_target_properties(xp2gdl90 PROPERTIES
        LINK_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/mac_exports.txt"
    )
    
elseif(UNIX)
    # Link against X-Plane libraries  
    target_link_libraries(xp2gdl90
        "${XPLANE_SDK_PATH}/Libraries/Lin/XPLM${XPLANE_PLATFORM_SUFFIX}.so"
        "${XPLANE_SDK_PATH}/Libraries/Lin/XPWidgets${XPLANE_PLATFORM_SUFFIX}.so"
        dl  # Dynamic link library
    )
    
    # Use Linux symbol export file
    set_target_properties(xp2gdl90 PROPERTIES
        LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linux_exports.txt"
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
    XPLM200=1
    XPLM210=1
    XPLM300=1
    XPLM301=1
    XPLM400=1
    XPLM410=1
)



# Print build information
message(STATUS "Building XP2GDL90 plugin")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Output filename: ${PLUGIN_OUTPUT_FILENAME}")
message(STATUS "X-Plane SDK path: ${XPLANE_SDK_PATH}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")