# Changelog

All notable changes to the XP2GDL90 plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.3] - 2025-08-28

### Major Improvements

#### 🏗️ **Complete UI Architecture Overhaul**
- **Advanced Window Management System**: Migrated from simple ImGuiManager to robust XPImgWindow architecture from xPilot project
- **Modular Window Classes**: Separated concerns with dedicated `ConfigWindow` and `StatusWindow` classes inheriting from `XPImgWindow`
- **Professional ImGui Integration**: Adopted industry-standard ImGui window lifecycle management with VR support, positioning modes, and window styles
- **Enhanced Maintainability**: Clean separation of UI logic from main plugin code for better testing and future development

#### ⌨️ **Completely Fixed Keyboard Input System**
- **Modern ImGui API**: Updated from deprecated `io.KeyMap` system to current `io.AddKeyEvent` API for ImGui 1.91.5+
- **Full Key Support**: Fixed Delete key, Backspace, arrow keys, Home/End, Page Up/Down, and all standard keyboard shortcuts
- **Proper Event Mapping**: Complete XPLM virtual key to ImGui key mapping with modifier key support (Ctrl, Alt, Shift)
- **Text Input Processing**: Correct handling of printable character input alongside special key events

#### 🖥️ **Windows Platform Compatibility**
- **Compilation Fixes**: Resolved critical Windows build failures caused by missing OpenGL extension headers
- **Type System Support**: Added proper `<cstdint>` includes for `uintptr_t`, `uint32_t` and other standard types
- **MSVC Compiler Support**: Comprehensive warning suppression for clean Windows builds (C5045, C5219, C5039, etc.)
- **Cross-Platform Headers**: Improved platform-specific header inclusion logic for Windows, macOS, and Linux

### Technical Enhancements

#### 🎛️ **X-Plane SDK Integration**
- **Extended SDK Support**: Added support for XPLM210 and XPLM301 features including VR functions and modern window decorations
- **DataRef Management**: Implemented proper DataRef access patterns with error handling and VR status detection
- **Flight Loop Integration**: Enhanced flight loop callback system for window mode changes and state management
- **Memory Safety**: Proper resource cleanup with RAII patterns and exception-safe design

#### 🖼️ **Font and Rendering System**
- **Font Atlas Management**: Professional font texture management with automatic binding and cleanup
- **OpenGL Integration**: Improved OpenGL texture handling with proper type casting and platform compatibility
- **Rendering Optimization**: Smart texture generation with fallback systems for different ImGui contexts
- **VR Compatibility**: Full support for X-Plane VR mode with appropriate window positioning and sizing

#### 🏛️ **Code Architecture**
- **Separation of Concerns**: Clear distinction between main plugin logic and UI presentation layers
- **Forward Declarations**: Proper use of forward declarations to minimize compilation dependencies
- **Namespace Organization**: Clean organization of classes and structures to prevent naming conflicts
- **Exception Safety**: Robust error handling throughout the UI system with graceful degradation

### Bug Fixes

#### 🔧 **Critical Fixes**
- **Structure Name Conflicts**: Resolved `FlightData` and `TrafficTarget` naming conflicts between UI and main plugin
- **API Compatibility**: Fixed deprecated ImGui API usage that caused compilation failures
- **Coordinate System**: Corrected window coordinate transformations between X-Plane and ImGui coordinate spaces
- **Window Lifecycle**: Fixed window creation, destruction, and state management issues

#### 🐛 **Platform-Specific Fixes**
- **Windows Headers**: Fixed missing `GL/glext.h` header issue on Windows systems
- **Type Definitions**: Added missing standard library includes for cross-platform type compatibility
- **Constructor Initialization**: Fixed member initialization order warnings and undefined behavior
- **Static Analysis**: Resolved various static analysis warnings and potential issues

### User Experience

#### 🎨 **Enhanced UI Features**
- **Professional Window Styling**: Consistent styling across all UI components with modern appearance
- **Flexible Window Modes**: Support for floating, pop-out, VR, and centered window positioning modes
- **Improved Controls**: Enhanced text input fields, checkboxes, and buttons with better user feedback
- **Status Monitoring**: Real-time connection status, traffic count, and message statistics display

#### 🎯 **Usability Improvements**
- **Keyboard Navigation**: Full keyboard navigation support with proper focus management
- **Window Management**: Intuitive window resizing, positioning, and state persistence
- **Visual Feedback**: Clear indicators for connection status and plugin activity
- **Error Reporting**: Better error messages and debugging information for troubleshooting

### Performance Optimizations

#### ⚡ **Rendering Performance**
- **Smart Updates**: Efficient UI update cycles tied to actual data changes
- **Texture Management**: Optimized font texture handling with minimal GPU memory usage
- **Event Processing**: Streamlined keyboard and mouse event processing
- **Memory Usage**: Reduced memory footprint through better object lifecycle management

### Development Experience

#### 🛠️ **Build System Improvements**
- **Warning Suppression**: Comprehensive MSVC warning management for clean professional builds
- **Header Organization**: Better organization of platform-specific includes and dependencies
- **Compilation Speed**: Improved compilation times through reduced header dependencies
- **Error Messages**: Clearer compilation errors and better diagnostic information

#### 📚 **Code Quality**
- **Documentation**: Extensive inline documentation for all new UI classes and methods
- **Testing**: Improved testability through modular architecture and clear interfaces
- **Maintainability**: Clean code structure following modern C++ best practices
- **Extensibility**: Architecture designed for easy addition of new UI components and features

### Removed

- **Legacy Code**: Removed deprecated ImGui API usage and outdated compatibility code
- **Unused Variables**: Cleaned up unused member variables and temporary code
- **Debug Code**: Removed temporary debugging code and diagnostic output

### Changed

- **UI Architecture**: Complete replacement of monolithic UI management with modular window system
- **Event Handling**: Modern event-driven architecture replacing polling-based systems
- **Resource Management**: RAII-based resource management throughout UI system
- **Code Organization**: Restructured codebase for better maintainability and testing

---
*Version 1.0.3 represents a major architectural upgrade with focus on long-term maintainability, user experience, and cross-platform compatibility. This release establishes a solid foundation for future UI enhancements and feature development.*

## [1.0.2] - 2025-08-27

### Added
- **Modern ImGui UI System**: Complete replacement of legacy XPWidgets with Dear ImGui for modern, responsive user interface
- **Real-time Configuration Window**: Live editing of FDPRO IP address, port settings, broadcast options, and traffic reporting
- **Status Monitor Window**: Real-time display of flight data, connection status, traffic count, and message statistics
- **Interactive Traffic Display**: Live table showing detected AI aircraft and multiplayer targets with position data
- **Menu Integration**: Native X-Plane menu system integration with "XP2GDL90" submenu for easy UI access
- **Cross-Platform UI Support**: ImGui implementation works consistently across Windows, macOS, and Linux

### Technical Improvements
- **OpenGL2 Backend**: Custom X-Plane ImGui backend for seamless rendering integration
- **Delayed Initialization**: Smart UI initialization system to avoid OpenGL context conflicts during plugin startup
- **Window Management**: Sophisticated window lifecycle management with proper show/hide states
- **Input Handling**: Complete mouse, keyboard, and scroll wheel event handling for ImGui widgets
- **Memory Optimization**: Efficient singleton pattern for UI manager with minimal resource usage
- **Debug Logging**: Comprehensive debug output for UI initialization and interaction tracking

### User Experience
- **Modern Visual Design**: Professional styling with rounded corners, modern colors, and improved typography
- **Responsive Layout**: Adaptive window sizing and positioning with user preferences persistence
- **Real-time Updates**: Live data refresh every 100ms for smooth status monitoring
- **Intuitive Controls**: Standard ImGui widgets (text inputs, checkboxes, buttons) for familiar user interaction
- **Connection Feedback**: Visual status indicators showing connection state and broadcasting activity

### Fixed
- **SDK Compatibility**: Resolved X-Plane SDK header dependency issues and type definitions
- **Compilation Errors**: Fixed multiple C++ compilation issues across different platform configurations
- **OpenGL Integration**: Resolved rendering context conflicts and initialization timing issues
- **Window Visibility**: Fixed invisible window issues preventing UI interaction
- **Memory Management**: Proper resource cleanup and leak prevention in UI lifecycle
- **Event Handling**: Correct X-Plane window event callback signatures and parameter handling

### Removed
- **Legacy XPWidgets**: Completely removed outdated XPWidgets UI system and dependencies
- **Static Configuration**: Eliminated need for manual configuration file editing
- **Platform-Specific UI**: Unified UI experience across all supported platforms

### Changed
- **Configuration Method**: Transitioned from code-based configuration to real-time UI editing
- **User Interaction**: Modernized from basic widgets to professional ImGui interface
- **Menu Structure**: Reorganized plugin menu with clear "Configuration Window" and "Status Monitor" options
- **Rendering Pipeline**: Updated to use modern OpenGL2 backend through ImGui integration

### Performance Enhancements
- **Conditional Rendering**: UI only renders when windows are visible, reducing unnecessary GPU usage
- **Optimized Updates**: Smart update frequency control to balance responsiveness with performance
- **Minimal Window Footprint**: Tiny off-screen rendering window to minimize visual impact
- **Efficient State Management**: Streamlined UI state synchronization with plugin core

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

[Unreleased]: https://github.com/username/xp2gdl90/compare/v1.0.3...HEAD
[1.0.3]: https://github.com/username/xp2gdl90/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/username/xp2gdl90/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/username/xp2gdl90/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/username/xp2gdl90/releases/tag/v1.0.0