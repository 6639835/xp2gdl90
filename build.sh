#!/bin/bash

# X-Plane Plugin Build Script
# Usage: ./build.sh [clean]

set -e  # Exit on error

PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="$PROJECT_DIR/build"

echo "=== X-Plane XP2GDL90 Plugin Build Script ==="
echo "Project Directory: $PROJECT_DIR"
echo "Build Directory: $BUILD_DIR"

# Check if cleanup is needed
if [ "$1" = "clean" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Check if CMake is installed
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake not found. Please install CMake first."
    echo "brew install cmake"
    exit 1
fi

# Check if X-Plane SDK exists
if [ ! -d "$PROJECT_DIR/SDK" ]; then
    echo "Error: X-Plane SDK not found. Please ensure SDK directory exists."
    echo "Download the SDK from https://developer.x-plane.com/"
    echo "Extract it to $PROJECT_DIR/SDK/"
    exit 1
fi

echo "Running CMake configuration..."
cmake -DCMAKE_BUILD_TYPE=Release "$PROJECT_DIR"

echo "Starting compilation..."
cmake --build . --config Release

echo ""
echo "=== Build Complete ==="

# Check output file based on platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    XPL_FILE="$BUILD_DIR/xp2gdl90/mac_x64.xpl"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    XPL_FILE="$BUILD_DIR/xp2gdl90/lin_x64.xpl"
else
    XPL_FILE="$BUILD_DIR/xp2gdl90/win_x64.xpl"
fi

if [ -f "$XPL_FILE" ]; then
    echo "[SUCCESS] Generated: $XPL_FILE"
    
    # Display file information
    echo ""
    echo "File information:"
    ls -la "$XPL_FILE"
    
    echo ""
    echo "Architecture information:"
    file "$XPL_FILE"
    
    echo ""
    echo "Installation Instructions:"
    echo "1. Create directory: X-Plane/Resources/plugins/xp2gdl90/"
    echo "2. Copy $(basename $XPL_FILE) to that directory"
    echo "3. Restart X-Plane"
    echo ""
    echo "Plugin Features:"
    echo "- Reads flight data directly from X-Plane using SDK"
    echo "- Encodes data to GDL-90 format"
    echo "- Broadcasts to FDPRO via UDP on port 4000"
    echo "- Supports ownship position and traffic targets"
    echo "- No external UDP configuration needed in X-Plane"
else
    echo "[ERROR] Output file not found: $XPL_FILE"
    echo "Looking for files in build directory:"
    find "$BUILD_DIR" -name "*.xpl" -ls
    exit 1
fi