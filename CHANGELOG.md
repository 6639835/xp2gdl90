# Changelog

All notable changes to the XP2GDL90 plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] - 2025-08-26

### Added
- **Contributing Guidelines**: Comprehensive CONTRIBUTING.md with development workflow, coding standards, and submission guidelines
- **Security Policy**: Detailed SECURITY.md with vulnerability reporting procedures and security best practices
- **Documentation Standards**: Enhanced project documentation structure following industry best practices

### Documentation Improvements
- **Contributing Workflow**: Step-by-step guide for developers including build setup, testing procedures, and pull request guidelines
- **Security Framework**: Complete security policy covering network security, plugin architecture, data privacy, and vulnerability disclosure
- **Code Standards**: Detailed coding standards with C++ style guidelines, naming conventions, and best practices
- **Issue Templates**: Structured templates for bug reports, feature requests, and security vulnerability reporting
- **Development Setup**: Comprehensive instructions for setting up development environment across all supported platforms

### Changed
- **Changelog Format**: Updated to detailed format with comprehensive version history and proper categorization
- **Project Structure**: Improved documentation organization for better maintainability and contributor onboarding
- **Security Awareness**: Enhanced security documentation to address aviation data broadcasting concerns

### Technical Improvements
- **Documentation Quality**: Professional-grade documentation following open source best practices
- **Contributor Experience**: Streamlined process for new contributors with clear guidelines and expectations
- **Security Posture**: Formalized security policy and procedures for responsible vulnerability disclosure

## [1.0.0] - 2025-08-26

### Added
- **Initial Release**: Complete XP2GDL90 C++ plugin for X-Plane integration
- **Native SDK Integration**: Direct X-Plane SDK access eliminates need for Data Output configuration
- **GDL-90 Protocol Implementation**: Full compliance with GDL-90 message format specification
- **Real-time Data Broadcasting**: UDP transmission to FDPRO and compatible applications on port 4000
- **Heartbeat Messages**: 1Hz status and timing synchronization messages (Message ID 0x00)
- **Ownship Position Reports**: 2Hz aircraft position, altitude, speed, and track data (Message ID 0x0A)
- **Traffic Target Reports**: 2Hz position reports for AI aircraft and multiplayer targets (Message ID 0x14)
- **Automatic Traffic Detection**: Real-time detection of AI aircraft, multiplayer targets, and TCAS contacts
- **Cross-Platform Support**: Native builds for Windows, macOS (Intel + Apple Silicon), and Linux
- **Zero Configuration**: No X-Plane setup required - works immediately after installation

### Flight Data Integration
- **Position Data**: Latitude, longitude, altitude (GPS and barometric)
- **Movement Data**: Ground speed, track angle, vertical speed
- **Aircraft State**: On-ground detection, emergency status, navigation integrity
- **Traffic Awareness**: TCAS integration for comprehensive traffic picture
- **Timing Synchronization**: UTC timestamp integration for precise timing

### Network Features
- **UDP Broadcasting**: Efficient datagram transmission to localhost and remote targets
- **Configurable Targeting**: Default localhost (127.0.0.1) with easy IP modification
- **Port Management**: Standard FDPRO port 4000 with customizable configuration
- **Error Handling**: Robust network error detection and recovery
- **Performance Optimization**: Minimal CPU overhead with efficient data transmission

### Build System
- **CMake Integration**: Modern CMake 3.10+ build system for all platforms
- **Cross-Platform Scripts**: Automated build scripts for each operating system
  - `build.sh` for macOS and Linux
  - `build_windows.bat` for Windows
  - `build_all.py` for cross-platform automation
- **SDK Integration**: Seamless X-Plane SDK detection and linking
- **Symbol Export**: Proper platform-specific symbol export configuration
- **Universal Binaries**: macOS universal binary support (Intel + Apple Silicon)

### Plugin Architecture
- **Standard Plugin Interface**: Full X-Plane plugin lifecycle implementation
- **Dataref Access**: Efficient reading of flight and traffic data
- **Flight Loop Integration**: Precise timing control for message transmission
- **Memory Management**: Robust resource allocation and cleanup
- **Error Handling**: Comprehensive error detection and logging

### Technical Features
- **GDL-90 Message Encoding**: Complete implementation of all required message types
  - Flag byte validation and checksum calculation
  - Proper coordinate encoding (24-bit signed integers)
  - Altitude encoding with geometric and barometric height
  - Navigation integrity and accuracy indicators
- **Traffic Management**: Support for up to 63 simultaneous traffic targets
- **ICAO Address Management**: Configurable aircraft identification codes
- **Platform Networking**: Native socket implementation for each operating system
- **Debug Logging**: Comprehensive logging via X-Plane debug system

### Performance Characteristics
- **Low CPU Usage**: Optimized C++ implementation with minimal overhead
- **Efficient Memory Usage**: Stack-based allocation with minimal dynamic memory
- **Precise Timing**: Hardware-accurate timing for regulatory compliance
- **Network Efficiency**: Minimal bandwidth usage with optimal message sizing
- **Scalable Traffic**: Efficient handling of multiple aircraft targets

### Compatibility
- **X-Plane Versions**: Compatible with X-Plane 11 and X-Plane 12
- **Operating Systems**: Windows 10+, macOS 10.14+, Linux (Ubuntu 18.04+)
- **Architecture Support**: 64-bit Intel, Apple Silicon (M1/M2)
- **FDPRO Integration**: Full compatibility with FDPRO GDL-90 input
- **EFB Applications**: Compatible with any GDL-90 capable Electronic Flight Bag

### Documentation
- **Comprehensive README**: Complete installation and usage documentation
- **Build Instructions**: Detailed build process for all platforms
- **Troubleshooting Guide**: Common issues and solutions
- **GDL-90 Reference**: Message format documentation and examples
- **Network Configuration**: Setup instructions for various network configurations

### Quality Assurance
- **Cross-Platform Testing**: Verified on Windows, macOS, and Linux
- **Aircraft Compatibility**: Tested with various aircraft types and configurations
- **Network Validation**: Verified GDL-90 message format compliance
- **Performance Testing**: CPU and memory usage validation
- **Integration Testing**: FDPRO and EFB application compatibility verification

[Unreleased]: https://github.com/username/xp2gdl90/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/username/xp2gdl90/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/username/xp2gdl90/releases/tag/v1.0.0