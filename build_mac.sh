#!/bin/bash
# Build script for macOS

echo "========================================="
echo "Building xp2gdl90 for macOS"
echo "========================================="

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo ""
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Configuration failed!"
    exit 1
fi

# Build the plugin
echo ""
echo "Building plugin..."
cmake --build . --config Release

if [ $? -ne 0 ]; then
    echo ""
    echo "[ERROR] Build failed!"
    exit 1
fi

echo ""
echo "========================================="
echo "[SUCCESS] Build successful!"
echo "========================================="
echo "Output: build/mac.xpl"
echo ""
echo "To install the plugin, copy the following to your X-Plane/Resources/plugins/ directory:"
echo "  - build/mac.xpl"
echo "  - xp2gdl90.ini"
echo "========================================="

