# XP2GDL90 C++ Plugin

A native X-Plane plugin that broadcasts flight data in GDL-90 format to FDPRO, eliminating the need for external UDP configuration.

## Overview

This C++ plugin replaces the Python UDP-based solution by:
- Reading flight data directly from X-Plane using the SDK
- Encoding data in standard GDL-90 format
- Broadcasting to FDPRO via UDP
- Supporting both ownship position and traffic targets
- Requiring no X-Plane Data Output configuration

## Features

- **Direct SDK Integration**: No UDP data output configuration needed in X-Plane
- **GDL-90 Compliance**: Full GDL-90 message format implementation
- **Real-time Broadcasting**: 1Hz heartbeat, 2Hz position reports, 2Hz traffic reports
- **Traffic Support**: Automatic detection of AI aircraft and multiplayer targets
- **Cross-platform**: Works on Windows, macOS, and Linux
- **Low overhead**: Native C++ performance

## Building the Plugin

### Prerequisites

1. **CMake** (3.10 or later)
   ```bash
   # macOS
   brew install cmake
   
   # Ubuntu/Debian
   sudo apt install cmake build-essential
   
   # Windows
   # Download from https://cmake.org/download/
   ```

2. **X-Plane SDK** (should already be in `SDK/` directory)

### Build Steps

#### macOS/Linux
```bash
./build.sh
```

#### Windows
```batch
build_windows.bat
```

#### Manual Build
```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### Build Output

The plugin will be built as:
- **macOS**: `build/xp2gdl90/mac_x64.xpl`
- **Linux**: `build/xp2gdl90/lin_x64.xpl` 
- **Windows**: `build/xp2gdl90/win_x64.xpl`

## Installation

1. **Create plugin directory**:
   ```
   X-Plane/Resources/plugins/xp2gdl90/
   ```

2. **Copy the plugin file**:
   - Copy the appropriate `.xpl` file to the plugin directory
   - Ensure the filename matches your platform (mac_x64.xpl, lin_x64.xpl, or win_x64.xpl)

3. **Restart X-Plane**

## Usage

### Basic Operation

1. **Start X-Plane** with the plugin installed
2. **Load an aircraft** and ensure you're in flight
3. **Start FDPRO** and configure it to receive GDL-90 data on port 4000
4. The plugin automatically starts broadcasting when enabled

### Plugin Status

Monitor the X-Plane Log.txt file for plugin status:
```
XP2GDL90: Plugin started successfully
XP2GDL90: Plugin enabled - starting GDL-90 broadcast
XP2GDL90: Sent heartbeat (8 bytes)
XP2GDL90: Sent position report (34 bytes): LAT=37.524000, LON=-122.063000, ALT=100 ft
```

### Configuration

The plugin uses these default settings:
- **Target IP**: 127.0.0.1 (localhost)
- **Target Port**: 4000 (FDPRO default)
- **ICAO Address**: 0xABCDEF (ownship)
- **Traffic Enabled**: Yes (automatic detection)

To modify these settings, edit the constants in `src/xp2gdl90.cpp` and rebuild.

### Traffic Targets

The plugin automatically detects and reports:
- **AI Aircraft**: Configure in X-Plane → Aircraft & Situations → AI Aircraft
- **Multiplayer Aircraft**: Join multiplayer session or enable AI traffic
- **TCAS Targets**: Any aircraft visible to your aircraft's TCAS system

## GDL-90 Messages

The plugin transmits these standard GDL-90 messages:

### Heartbeat (ID 0x00)
- Frequency: 1 Hz
- Contains UTC timestamp and status information

### Ownship Report (ID 0x0A)  
- Frequency: 2 Hz
- Contains your aircraft's position, altitude, speed, track, and vertical speed

### Traffic Report (ID 0x14)
- Frequency: 2 Hz per target
- Contains position and movement data for each detected aircraft
- Maximum 63 traffic targets supported

## Troubleshooting

### Plugin Not Loading
1. Check X-Plane Log.txt for error messages
2. Ensure the correct .xpl file for your platform is installed
3. Verify X-Plane SDK compatibility

### No Data Being Sent
1. Check that FDPRO is listening on port 4000
2. Verify firewall settings allow UDP traffic
3. Ensure aircraft is loaded and in flight (not on ground with engines off)

### Traffic Not Appearing
1. Enable AI aircraft in X-Plane settings
2. Join multiplayer session
3. Verify TCAS system is active on your aircraft

### Network Issues
1. Check Windows Firewall / macOS Firewall settings
2. Verify FDPRO is configured to receive on port 4000
3. Test with network monitoring tools (Wireshark, etc.)

## Debugging

Enable debug output by monitoring X-Plane's Log.txt file:
```
tail -f "X-Plane 12/Log.txt" | grep XP2GDL90
```

## Comparison with Python Version

| Feature | Python Version | C++ Plugin |
|---------|----------------|------------|
| X-Plane Setup | Requires Data Output configuration | No setup required |
| Performance | Higher CPU usage, UDP overhead | Native performance |
| Reliability | Network dependent | Direct SDK access |
| Dependencies | Python runtime + libraries | None (compiled binary) |
| Platform Support | Cross-platform with Python | Native cross-platform |

## Development

### Source Structure
```
src/
  xp2gdl90.cpp       # Main plugin implementation
CMakeLists.txt       # Build configuration
build.sh            # macOS/Linux build script
build_windows.bat   # Windows build script
```

### Key Components
- **Plugin Lifecycle**: Standard X-Plane plugin interface
- **GDL-90 Encoder**: Complete message format implementation
- **Dataref Reading**: Direct access to flight data
- **UDP Broadcasting**: Network transmission to FDPRO
- **Flight Loop**: Timed execution for periodic transmission

### Customization

To modify the plugin behavior:
1. Edit configuration constants in `xp2gdl90.cpp`
2. Rebuild using the build scripts
3. Reinstall the updated plugin

## License

This project follows the same license terms as the original Python implementation.
