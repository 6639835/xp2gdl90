#!/bin/bash

# XP2GDL90 Plugin Build Script (Linux)
# Usage: ./build_linux.sh [clean]

set -e  # Exit on error

PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR="$PROJECT_DIR/build"

echo "=== XP2GDL90 Plugin Build Script (Linux) ==="
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
    echo "Error: CMake not found. Please install CMake first:"
    echo "Ubuntu/Debian: sudo apt-get install cmake build-essential"
    echo "CentOS/RHEL: sudo yum install cmake gcc-c++ make"
    echo "Fedora: sudo dnf install cmake gcc-c++ make"
    exit 1
fi

# Check if C++ compiler is installed
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "Error: C++ compiler not found. Please install:"
    echo "Ubuntu/Debian: sudo apt-get install build-essential"
    echo "CentOS/RHEL: sudo yum groupinstall \"Development Tools\""
    echo "Fedora: sudo dnf groupinstall \"Development Tools\""
    exit 1
fi

# Check if X-Plane SDK exists
if [ ! -d "$PROJECT_DIR/SDK" ]; then
    echo "Error: X-Plane SDK not found. Please ensure SDK directory exists."
    echo "The SDK should be located at: $PROJECT_DIR/SDK"
    exit 1
fi

echo "Running CMake configuration..."
# Fix potential path issues by removing trailing slash
SOURCE_DIR="${PROJECT_DIR%/}"
cmake -DCMAKE_BUILD_TYPE=Release "$SOURCE_DIR"

echo "Starting compilation..."
cmake --build . --config Release

echo ""
echo "=== Build Complete ==="

# Check output file
XPL_FILE="$BUILD_DIR/lin.xpl"
if [ -f "$XPL_FILE" ]; then
    echo "[SUCCESS] Generated: $XPL_FILE"
    
    # Display file information
    echo ""
    echo "File information:"
    FILE_SIZE=$(stat -c%s "$XPL_FILE" 2>/dev/null || stat -f%z "$XPL_FILE" 2>/dev/null || echo "0")
    FILE_SIZE_KB=$((FILE_SIZE / 1024))
    ls -la "$XPL_FILE"
    echo "File size: ${FILE_SIZE_KB} KB"
    
    # Basic sanity check
    if [ "$FILE_SIZE" -lt 10240 ]; then
        echo ""
        echo "[WARNING] Plugin file is unusually small ($FILE_SIZE bytes)"
        echo "This might indicate a build issue."
    fi
    
    echo ""
    echo "Library dependencies:"
    ldd "$XPL_FILE" 2>/dev/null || echo "Unable to display library dependencies"
    
    echo ""
    echo "[INSTALLATION] Copy lin.xpl to:"
    echo "  X-Plane/Resources/plugins/xp2gdl90/lin.xpl"
    echo "  No additional files needed - plugin is self-contained"
else
    echo "[ERROR] Output file not found: $XPL_FILE"
    exit 1
fi
