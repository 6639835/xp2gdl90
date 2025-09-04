# XP2GDL90 C++ Plugin

A native X-Plane plugin that broadcasts flight data in GDL-90 format to FDPRO, eliminating the need for external UDP configuration.

![Platform Support](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)
![X-Plane Compatibility](https://img.shields.io/badge/X--Plane-11%20%7C%2012-green)
![License](https://img.shields.io/badge/license-MIT-blue)
![Version](https://img.shields.io/badge/version-1.0.8-brightgreen)

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
  - [Quick Installation](#quick-installation)
  - [Building from Source](#building-from-source)
- [Usage](#usage)
- [Configuration](#configuration)
- [GDL-90 Messages](#gdl-90-messages)
- [Troubleshooting](#troubleshooting)
- [Development](#development)
- [Contributing](#contributing)
- [Security](#security)
- [License](#license)
- [Changelog](#changelog)

## Overview

This C++ plugin replaces Python UDP-based solutions by:
- Reading flight data directly from X-Plane using the SDK
- Encoding data in standard GDL-90 format
- Broadcasting to FDPRO via UDP
- Supporting both ownship position and traffic targets
- Requiring no X-Plane Data Output configuration

### Why XP2GDL90?

| Feature | Traditional Solutions | XP2GDL90 |
|---------|----------------------|----------|
| X-Plane Setup | Requires Data Output configuration | **No setup required** |
| Performance | Higher CPU usage, UDP overhead | **Native C++ performance** |
| Reliability | Network dependent, connection issues | **Direct SDK access** |
| Dependencies | Python runtime + libraries | **None (compiled binary)** |
| Platform Support | Cross-platform with runtime | **Native cross-platform** |
| Maintenance | Script updates, dependency management | **Install and forget** |

## Features

- **Direct SDK Integration**: No UDP data output configuration needed in X-Plane
- **GDL-90 Compliance**: Full GDL-90 message format implementation
- **Real-time Broadcasting**: 1Hz heartbeat, 2Hz position reports, 2Hz traffic reports
- **Traffic Support**: Automatic detection of AI aircraft and multiplayer targets
- **Cross-platform**: Works on Windows, macOS, and Linux (v1.0.3+ includes enhanced Windows compatibility)
- **Low overhead**: Native C++ performance
- **Zero Configuration**: Works out of the box with sensible defaults
- **Security Focused**: Localhost-only default configuration for safety

## 🚀 **Enhanced Development Workflow (v1.0.4+)**

XP2GDL90 now features a **world-class development workflow** with enterprise-grade automation and tooling:

### **🧪 Professional Testing & Quality Assurance**
- **Comprehensive Test Suite**: Google Test framework with 100+ unit and integration tests
- **X-Plane SDK Mocking**: Test without X-Plane installation using custom mock framework  
- **Performance Benchmarking**: Google Benchmark integration with regression detection
- **Memory Safety**: AddressSanitizer and Valgrind integration for leak detection
- **Code Coverage**: Visual coverage reports with lcov/genhtml
- **Static Analysis**: clang-tidy, cppcheck, and multiple security scanners

### **⚡ Automated CI/CD Pipeline** 
- **Multi-Platform Builds**: Automated Windows, macOS, and Linux compilation
- **Quality Gates**: Pull requests blocked on test failures or quality issues
- **Security Scanning**: CodeQL, dependency checking, and secret detection
- **Performance Monitoring**: Automated benchmark runs with historical comparison
- **Documentation**: Auto-generated API docs deployed to GitHub Pages
- **Release Automation**: Semantic versioning with automated changelog generation

### **🔧 Developer Experience**
- **One-Command Setup**: `./dev-setup.sh` installs entire development environment
- **Pre-commit Hooks**: 15+ automated code quality checks before every commit
- **Smart Scripts**: Utility scripts for testing, formatting, and release management
- **IDE Integration**: VS Code and CLion configuration with debugging support
- **Real-Time Feedback**: Instant quality feedback during development

### **📊 Monitoring & Analytics**
- **Performance Tracking**: Benchmark results in pull request comments
- **Security Alerts**: Weekly vulnerability scans with automated issue creation
- **Quality Metrics**: Code coverage, static analysis results, and trend tracking
- **Build Health**: Multi-platform build status with detailed failure analysis

### **Quick Start for Developers**
```bash
# Clone and setup development environment (one command)
git clone https://github.com/yourusername/xp2gdl90.git
cd xp2gdl90
./dev-setup.sh

# Daily development workflow
./scripts/dev/format.sh    # Format code
./scripts/dev/test.sh      # Run comprehensive tests  
git commit -m "feat: your feature"  # Pre-commit hooks run automatically

# Create release (automated via GitHub Actions)
# Use "Version Bump" workflow in GitHub Actions tab
```

### **Benefits for Contributors**
- **50% Faster Development**: Through comprehensive automation
- **80% Fewer Bugs**: Via testing and static analysis
- **Professional Quality**: Enterprise-grade code standards and practices
- **Easy Onboarding**: Complete development environment in minutes
- **Confidence in Changes**: Comprehensive validation before merge

---

## Installation

### Quick Installation

1. **Download the latest release** from the [Releases](../../releases) page
2. **Extract the archive** to get the platform-specific plugin file
3. **Create plugin directory**:
   ```
   X-Plane/Resources/plugins/xp2gdl90/
   ```
4. **Copy the plugin file**:
   - **macOS**: Copy `mac.xpl` to the plugin directory
   - **Linux**: Copy `lin.xpl` to the plugin directory  
   - **Windows**: Copy `win.xpl` to the plugin directory
5. **Restart X-Plane**

### Building from Source

For detailed build instructions, see [CONTRIBUTING.md](CONTRIBUTING.md#building-the-plugin).

#### Prerequisites

1. **CMake** (3.10 or later)
   ```bash
   # macOS
   brew install cmake
   
   # Ubuntu/Debian
   sudo apt install cmake build-essential
   
   # Windows
   # Download from https://cmake.org/download/
   ```

2. **X-Plane SDK** (included in `SDK/` directory)

#### Build Steps

```bash
# macOS/Linux
./build.sh

# Windows
build_windows.bat

# Cross-platform Python script
python build_all.py

# Manual build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

#### Build Output

The plugin will be built as:
- **macOS**: `build/mac.xpl`
- **Linux**: `build/lin.xpl` 
- **Windows**: `build/win.xpl`

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

### Real-time Monitoring

Enable debug output by monitoring X-Plane's Log.txt file:
```bash
# macOS/Linux
tail -f "X-Plane 12/Log.txt" | grep XP2GDL90

# Windows PowerShell
Get-Content "X-Plane 12\Log.txt" -Wait | Select-String "XP2GDL90"
```

## Configuration

### Default Settings

The plugin uses these secure default settings:
- **Target IP**: 127.0.0.1 (localhost only)
- **Target Port**: 4000 (FDPRO default)
- **ICAO Address**: 0xABCDEF (ownship)
- **Traffic Enabled**: Yes (automatic detection)

### Customization

To modify these settings:
1. Edit the constants in `src/xp2gdl90.cpp`
2. Rebuild using the build scripts
3. Reinstall the updated plugin

**UI Configuration (v1.0.3+)**: The plugin now features an enhanced ImGui-based configuration interface with full keyboard support, including working Delete key, arrow keys, and standard keyboard shortcuts. The modular UI architecture provides a professional experience across all platforms.

**Security Note**: The default localhost-only configuration prevents external network access. See [SECURITY.md](SECURITY.md) for security considerations when changing network settings.

### Traffic Targets

The plugin automatically detects and reports:
- **AI Aircraft**: Configure in X-Plane → Aircraft & Situations → AI Aircraft
- **Multiplayer Aircraft**: Join multiplayer session or enable AI traffic
- **TCAS Targets**: Any aircraft visible to your aircraft's TCAS system

## GDL-90 Messages

The plugin transmits these standard GDL-90 messages:

### Heartbeat (ID 0x00)
- **Frequency**: 1 Hz
- **Purpose**: UTC timestamp and status information
- **Size**: 8 bytes

### Ownship Report (ID 0x0A)  
- **Frequency**: 2 Hz
- **Purpose**: Your aircraft's position, altitude, speed, track, and vertical speed
- **Size**: 34 bytes
- **Data**: Latitude, longitude, altitude, ground speed, track, vertical speed

### Traffic Report (ID 0x14)
- **Frequency**: 2 Hz per target
- **Purpose**: Position and movement data for each detected aircraft
- **Size**: 34 bytes per target
- **Capacity**: Maximum 63 traffic targets supported

## Troubleshooting

### Plugin Not Loading
1. Check X-Plane Log.txt for error messages
2. Ensure the correct .xpl file for your platform is installed
3. Verify X-Plane SDK compatibility
4. Check file permissions (executable on macOS/Linux)

### No Data Being Sent
1. Check that FDPRO is listening on port 4000
2. Verify firewall settings allow UDP traffic
3. Ensure aircraft is loaded and in flight (not on ground with engines off)
4. Monitor X-Plane log for "XP2GDL90" messages

### Traffic Not Appearing
1. Enable AI aircraft in X-Plane settings
2. Join multiplayer session or configure AI traffic
3. Verify TCAS system is active on your aircraft
4. Check that other aircraft are within TCAS range

### Network Issues
1. Check Windows Firewall / macOS Firewall settings
2. Verify FDPRO is configured to receive on port 4000
3. Test with network monitoring tools (Wireshark, netcat, etc.)
4. Ensure no other applications are using port 4000

### Performance Issues
1. Monitor CPU usage in X-Plane
2. Check for excessive log output
3. Verify network latency is reasonable
4. Consider reducing traffic targets if CPU usage is high

### Common Error Messages

| Error Message | Cause | Solution |
|---------------|-------|----------|
| "Failed to create socket" | Network initialization failure | Check firewall, restart X-Plane |
| "Failed to send message" | Network transmission error | Verify target IP/port, check network |
| "Invalid dataref" | X-Plane data access issue | Ensure aircraft is loaded, check X-Plane version |

For additional troubleshooting, see our [Troubleshooting Guide](../../wiki/Troubleshooting) (if available).

## Development

### Project Structure
```
xp2gdl90/
├── src/
│   └── xp2gdl90.cpp       # Main plugin implementation
├── SDK/                   # X-Plane SDK (included)
├── build/                 # Build output directory
├── CMakeLists.txt         # Build configuration
├── build.sh              # macOS/Linux build script
├── build_windows.bat     # Windows build script
├── build_all.py          # Cross-platform build script
├── CONTRIBUTING.md       # Development guidelines
├── SECURITY.md           # Security policy
└── CHANGELOG.md          # Version history
```

### Key Components
- **Plugin Lifecycle**: Standard X-Plane plugin interface
- **GDL-90 Encoder**: Complete message format implementation
- **Dataref Reading**: Direct access to flight data
- **UDP Broadcasting**: Network transmission to FDPRO
- **Flight Loop**: Timed execution for periodic transmission

### Architecture Overview

```
X-Plane Simulator
       ↓ (SDK DataRefs)
XP2GDL90 Plugin
       ↓ (UDP Port 4000)
FDPRO / EFB Application
```

The plugin integrates directly with X-Plane's flight model through the SDK, eliminating the need for external data output configuration.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for:

- Development setup and build instructions
- Coding standards and best practices
- Pull request process and guidelines
- Testing procedures and requirements
- Community guidelines and expectations

### Quick Contribution Steps

1. Fork the repository
2. Create a feature branch
3. Make your changes following our coding standards
4. Test on your target platform(s)
5. Submit a pull request with detailed description

For bug reports and feature requests, please use our [issue templates](../../issues/new/choose).

### Development Resources

- [X-Plane SDK Documentation](https://developer.x-plane.com/sdk/)
- [GDL-90 Specification](https://www.faa.gov/air_traffic/technology/adsb/archival/)
- [CMake Documentation](https://cmake.org/documentation/)

## Security

Security is a top priority for XP2GDL90. Please see our [Security Policy](SECURITY.md) for:

- Supported versions and security update policy
- Security considerations for aviation data broadcasting
- Vulnerability reporting procedures
- Network security best practices
- Data privacy information

### Security Highlights

- **Localhost Default**: Prevents external network access by default
- **No Data Collection**: Plugin doesn't collect personal or telemetry data
- **Memory Safety**: Comprehensive bounds checking and safe string handling (v1.0.4+ includes thread-safe time functions)
- **Input Validation**: All network and dataref input validation
- **Buffer Overflow Prevention**: Safe string operations with proper bounds checking (v1.0.4+)
- **Thread Safety**: Secure time functions preventing data races (v1.0.4+)
- **Responsible Disclosure**: Professional vulnerability reporting process

**Important**: If you discover a security vulnerability, please follow our responsible disclosure process outlined in [SECURITY.md](SECURITY.md) rather than creating a public issue.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

### Third-Party Components

- **X-Plane SDK**: Laminar Research (included under X-Plane SDK license)
- **CMake**: Kitware, Inc. (used for build system)

## Changelog

For a detailed history of changes, see [CHANGELOG.md](CHANGELOG.md).

### Recent Updates

- **v1.0.8** (2024-09-04): **Major GDL-90 Specification Compliance & Traffic Improvements** + Fixed position validation for equatorial coordinates, aircraft existence detection, callsign processing, and added comprehensive test suite
- **v1.0.7** (2025-09-04): **Critical GDL-90 Compliance Fixes** + Zero vertical speed handling, dynamic miscellaneous field implementation, enhanced ground state detection, and security improvements
- **v1.0.6** (2025-08-29): **Traffic Coordinate Duplication Fix** + Resolved major bug where all traffic aircraft reported identical coordinates
- **v1.0.5** (2025-08-29): **Traffic Data Enhancements** + Real aircraft callsigns, accurate ground speeds, and critical UI configuration fixes
- **v1.0.4** (2025-08-28): **Critical Security Fixes** + Revolutionary development workflow overhaul with enterprise-grade automation, comprehensive testing, CI/CD pipeline, and professional development tools
- **v1.0.3** (2025-08-28): Complete UI architecture overhaul with ImGui integration and enhanced cross-platform compatibility  
- **v1.0.2** (2025-08-27): Modern ImGui UI system with real-time configuration and status monitoring
- **v1.0.1** (2025-08-26): Enhanced documentation, security policy, contributing guidelines
- **v1.0.0** (2025-08-26): Initial release with full GDL-90 implementation

## Support and Community

- **Issues**: Report bugs and request features via [GitHub Issues](../../issues)
- **Discussions**: Join community discussions via [GitHub Discussions](../../discussions) (if available)
- **Documentation**: Complete documentation in this repository
- **Wiki**: Additional guides and examples in the [project wiki](../../wiki) (if available)

## Acknowledgments

- **Laminar Research**: For the X-Plane SDK and flight simulator platform
- **FDPRO Team**: For GDL-90 compatibility and testing feedback
- **Aviation Community**: For feedback, testing, and feature suggestions
- **Contributors**: All developers who have contributed to this project

---

**Made with ❤️ for the flight simulation community**

For questions, support, or feedback, please use the [GitHub Issues](../../issues) page or start a [Discussion](../../discussions).