#!/bin/bash

# X-Plane Plugin Installation Script
# Usage: ./install.sh [X-PLANE_PATH]

PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
PLUGIN_NAME="OpenALSharedExample"

echo "=== X-Plane OpenAL Shared Example Plugin Installation Script ==="

# Check if plugin exists
XPL_FILE="$PROJECT_DIR/build/mac.xpl"
if [ ! -f "$XPL_FILE" ]; then
    echo "[ERROR] Plugin not found at: $XPL_FILE"
    echo "Please run ./build.sh first to build the plugin."
    exit 1
fi

# Determine X-Plane installation path
XPLANE_PATH=""
if [ -n "$1" ]; then
    XPLANE_PATH="$1"
    echo "Using provided X-Plane path: $XPLANE_PATH"
elif [ -d "/Applications/X-Plane 12" ]; then
    XPLANE_PATH="/Applications/X-Plane 12"
    echo "Found X-Plane 12 at: $XPLANE_PATH"
elif [ -d "/Applications/X-Plane 11" ]; then
    XPLANE_PATH="/Applications/X-Plane 11"
    echo "Found X-Plane 11 at: $XPLANE_PATH"
else
    echo "[ERROR] X-Plane installation not found."
    echo "Usage: $0 [X-PLANE_PATH]"
    echo "Example: $0 \"/Applications/X-Plane 12\""
    exit 1
fi

# Verify X-Plane path
if [ ! -d "$XPLANE_PATH" ]; then
    echo "[ERROR] X-Plane path does not exist: $XPLANE_PATH"
    exit 1
fi

if [ ! -d "$XPLANE_PATH/Resources" ]; then
    echo "[ERROR] Not a valid X-Plane installation (missing Resources directory)"
    exit 1
fi

# Create plugin directory
PLUGIN_DIR="$XPLANE_PATH/Resources/plugins/$PLUGIN_NAME"
echo "Creating plugin directory: $PLUGIN_DIR"
mkdir -p "$PLUGIN_DIR"

# Copy plugin file
echo "Copying plugin file..."
cp "$XPL_FILE" "$PLUGIN_DIR/"

# Copy sample sound file if it exists
SOUND_FILE="$PROJECT_DIR/sound.wav"
if [ -f "$SOUND_FILE" ]; then
    echo "Copying sample sound file..."
    cp "$SOUND_FILE" "$PLUGIN_DIR/"
else
    echo "Note: No sound.wav file found. You'll need to provide your own sound file."
fi

# Verify installation
if [ -f "$PLUGIN_DIR/mac.xpl" ]; then
    echo ""
    echo "[SUCCESS] Plugin installed successfully!"
    echo "Plugin location: $PLUGIN_DIR/mac.xpl"
    echo ""
    echo "Installation complete. Please restart X-Plane to load the plugin."
    echo ""
    echo "To use the plugin:"
    echo "1. Copy a sound.wav file to the plugin directory: $PLUGIN_DIR/"
    echo "2. Load any aircraft in X-Plane"
    echo "3. The plugin will automatically play sound when aircraft is loaded"
    echo "4. Each playback will have a slightly higher pitch"
    echo "5. Check X-Plane's Log.txt for plugin status messages"
else
    echo "[ERROR] Failed to install plugin"
    exit 1
fi