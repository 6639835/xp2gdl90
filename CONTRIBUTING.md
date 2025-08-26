# Contributing to XP2GDL90

Thank you for your interest in contributing to the XP2GDL90 plugin! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Building the Plugin](#building-the-plugin)
- [Testing](#testing)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Reporting Issues](#reporting-issues)
- [Feature Requests](#feature-requests)

## Code of Conduct

This project and everyone participating in it is governed by our [Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## Getting Started

### Prerequisites

Before you begin, ensure you have the following installed:

1. **CMake** (3.10 or later)
   ```bash
   # macOS
   brew install cmake
   
   # Ubuntu/Debian
   sudo apt install cmake build-essential
   
   # Windows - Download from https://cmake.org/download/
   ```

2. **C++ Compiler**
   - **macOS**: Xcode Command Line Tools (`xcode-select --install`)
   - **Linux**: GCC/G++ (`sudo apt install build-essential`)
   - **Windows**: Visual Studio 2019 or later

3. **Git** for version control

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/yourusername/xp2gdl90.git
   cd xp2gdl90
   ```

3. Add the upstream repository:
   ```bash
   git remote add upstream https://github.com/originalowner/xp2gdl90.git
   ```

## Development Environment

### Project Structure

```
xp2gdl90/
├── src/                    # Source code
│   └── xp2gdl90.cpp       # Main plugin implementation
├── SDK/                   # X-Plane SDK (included)
├── build/                 # Build output directory
├── CMakeLists.txt         # CMake configuration
├── build.sh              # macOS/Linux build script
├── build_windows.bat     # Windows build script
└── build_all.py          # Cross-platform build script
```

### X-Plane SDK

The X-Plane SDK is included in the `SDK/` directory. The plugin uses:
- **XPLM**: Core X-Plane plugin interface
- **XPWidgets**: UI components (for future configuration interface)

### Key Components

- **Plugin Lifecycle**: Standard X-Plane plugin interface (start, enable, disable, stop)
- **GDL-90 Implementation**: Message encoding and UDP transmission
- **Flight Data Access**: Reading aircraft position, speed, and traffic data
- **Network Layer**: UDP socket management for broadcasting to FDPRO

## Building the Plugin

### Quick Build

Use the provided build scripts:

```bash
# macOS/Linux
./build.sh

# Windows
build_windows.bat

# Cross-platform Python script
python build_all.py
```

### Manual Build

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

### Build Output

The plugin will be generated as:
- **macOS**: `build/mac.xpl`
- **Linux**: `build/lin.xpl` 
- **Windows**: `build/win.xpl`

## Testing

### Testing in X-Plane

1. **Install the plugin**:
   ```
   X-Plane/Resources/plugins/xp2gdl90/[platform].xpl
   ```

2. **Start X-Plane** and load an aircraft

3. **Monitor the log** (`X-Plane/Log.txt`):
   ```bash
   tail -f "X-Plane 12/Log.txt" | grep XP2GDL90
   ```

4. **Verify GDL-90 output** using:
   - **FDPRO**: Configure to receive on port 4000
   - **Network tools**: Wireshark, netcat, etc.
   - **Test receiver**: Simple UDP listener on port 4000

### Test Cases

Ensure your changes work with:
- **Different aircraft types**: GA, commercial, helicopters
- **Various flight phases**: Ground, takeoff, cruise, landing
- **Traffic scenarios**: AI aircraft, multiplayer sessions
- **Network conditions**: Localhost and remote IPs

## Coding Standards

### C++ Style

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line for functions, control structures
- **Naming**:
  - Variables: `camelCase`
  - Functions: `camelCase`
  - Constants: `UPPER_CASE`
  - Classes: `PascalCase`
- **Comments**: Use `//` for single line, `/* */` for blocks
- **Headers**: Include standard headers first, then X-Plane SDK headers

### Example Code Style

```cpp
// Configuration constants
const char* TARGET_IP = "127.0.0.1";
const int TARGET_PORT = 4000;

// Function implementation
void sendGdl90Message(const uint8_t* message, size_t length) {
    if (udpSocket == INVALID_SOCKET) {
        XPLMDebugString("XP2GDL90: Socket not initialized\n");
        return;
    }
    
    int result = sendto(udpSocket, 
                       reinterpret_cast<const char*>(message), 
                       static_cast<int>(length), 
                       0, 
                       reinterpret_cast<const sockaddr*>(&targetAddr), 
                       sizeof(targetAddr));
    
    if (result == SOCKET_ERROR) {
        XPLMDebugString("XP2GDL90: Failed to send message\n");
    }
}
```

### Logging Guidelines

- Use `XPLMDebugString()` for all debug output
- Prefix all messages with `"XP2GDL90: "`
- Include relevant data in error messages
- Use appropriate log levels (info, warning, error)

### Memory Management

- Avoid memory leaks - clean up all allocated resources
- Use RAII principles where possible
- Initialize all variables
- Check return values from system calls

## Submitting Changes

### Workflow

1. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make your changes** following the coding standards

3. **Test thoroughly** on your target platform(s)

4. **Commit with descriptive messages**:
   ```bash
   git commit -m "Add support for custom ICAO addresses
   
   - Allow configuration of ownship ICAO address
   - Add validation for 24-bit addresses
   - Update documentation"
   ```

5. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

6. **Create a Pull Request** with:
   - Clear description of changes
   - Screenshots/logs if applicable
   - Test results on different platforms
   - Reference to related issues

### Pull Request Guidelines

- **Title**: Concise description of the change
- **Description**: 
  - What changed and why
  - How to test the changes
  - Any breaking changes
  - Platform-specific considerations
- **Testing**: Include test results on relevant platforms
- **Documentation**: Update README.md if behavior changes

## Reporting Issues

### Bug Reports

When reporting bugs, please include:

1. **Environment**:
   - Operating system and version
   - X-Plane version
   - Plugin version
   - Aircraft being used

2. **Steps to reproduce**:
   - Detailed steps to trigger the issue
   - Expected vs actual behavior

3. **Logs**:
   - Relevant X-Plane Log.txt entries
   - Network capture if applicable

4. **Additional context**:
   - Screenshots if relevant
   - Configuration details

### Issue Template

```
**Environment:**
- OS: macOS 13.5
- X-Plane: 12.0.9
- Plugin Version: 1.0.0
- Aircraft: Cessna 172

**Problem:**
Brief description of the issue

**Steps to Reproduce:**
1. Step one
2. Step two
3. Step three

**Expected Behavior:**
What should happen

**Actual Behavior:**
What actually happens

**Logs:**
```
Relevant log entries
```

**Additional Information:**
Any other relevant details
```

## Feature Requests

We welcome feature requests! Please:

1. **Check existing issues** to avoid duplicates
2. **Describe the use case** - why is this feature needed?
3. **Propose a solution** if you have ideas
4. **Consider implementation complexity** and maintenance burden
5. **Think about backwards compatibility**

### Good Feature Request Example

```
**Feature Request: Configurable Update Rates**

**Use Case:**
Different EFB applications may require different update rates for optimal performance. 
FDPRO works well with 2Hz, but other applications might benefit from 5Hz or 10Hz updates.

**Proposed Solution:**
Add configuration options for:
- Heartbeat rate (default: 1Hz)
- Position report rate (default: 2Hz)  
- Traffic report rate (default: 2Hz)

**Implementation Ideas:**
- Add configuration file support (xp2gdl90.cfg)
- Add X-Plane menu for runtime configuration
- Validate rates (1-10Hz reasonable range)

**Backwards Compatibility:**
Default rates remain unchanged, new feature is opt-in.
```

## Development Tips

### Debugging

1. **Use debug builds** during development:
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   ```

2. **Monitor X-Plane logs** in real-time:
   ```bash
   tail -f "X-Plane 12/Log.txt" | grep XP2GDL90
   ```

3. **Network debugging** with Wireshark:
   - Filter: `udp.port == 4000`
   - Analyze GDL-90 message format

### Common Pitfalls

- **Socket cleanup**: Always close sockets in plugin stop
- **Dataref validity**: Check dataref handles before use
- **Platform differences**: Test networking code on all platforms
- **Flight loop timing**: Ensure consistent update rates
- **Error handling**: Check all system call return values

### Resources

- [X-Plane SDK Documentation](https://developer.x-plane.com/sdk/)
- [GDL-90 Specification](https://www.faa.gov/air_traffic/technology/adsb/archival/)
- [CMake Documentation](https://cmake.org/documentation/)

## Questions?

If you have questions about contributing:

1. Check the [README.md](README.md) for basic information
2. Search existing [issues](../../issues)
3. Create a new issue with the "question" label
4. Join our community discussions

Thank you for contributing to XP2GDL90!
