# XP2GDL90 - X-Plane to GDL90 Bridge

[![Build Status](https://github.com/6639835/xp2gdl90/workflows/Build%20XP2GDL90/badge.svg)](https://github.com/6639835/xp2gdl90/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance X-Plane 12 plugin that broadcasts real-time flight data in GDL90 format via UDP. Compatible with ForeFlight, Garmin Pilot, WingX, and other Electronic Flight Bag (EFB) applications.

## Features

- **Real-time Position Broadcasting**: Ownship position, altitude, speed, and heading
- **High Performance**: Native C++ plugin, minimal CPU overhead
- **Cross-Platform**: Windows, macOS (Universal), and Linux support
- **Fully Configurable**: INI-based configuration for all settings
- **Standards Based**: GDL90 framing/CRC/byte-stuffing per Rev A (message subset below)
- **Easy Setup**: No external dependencies, works out of the box

## Quick Start

### Installation

1. **Download** the latest release from [Releases](https://github.com/6639835/xp2gdl90/releases)
2. **Extract** the ZIP file
3. **Copy** the `xp2gdl90` folder to:
   ```
   X-Plane 12/Resources/plugins/xp2gdl90/
   ```
4. **Configure** your settings by editing `xp2gdl90.ini`
5. **Start** X-Plane 12

### Basic Configuration

Edit `X-Plane 12/Resources/plugins/xp2gdl90/xp2gdl90.ini`:

```ini
[Network]
target_ip = 192.168.1.100    # Your iPad/tablet IP address
target_port = 4000           # GDL90 default port

[Ownship]
icao_address = 0xABCDEF      # Your aircraft's ICAO address
callsign = N12345            # Fallback callsign (auto-reads from aircraft)
emitter_category = 1         # 1 = Light aircraft
```

### Finding Your iPad's IP Address

**iOS (iPad/iPhone):**
1. Open **Settings** → **Wi-Fi**
2. Tap the **(i)** icon next to your connected network
3. Note the **IP Address** (e.g., 192.168.1.100)

**Android:**
1. Open **Settings** → **Network & Internet** → **Wi-Fi**
2. Tap your connected network
3. Note the **IP address**

## Compatible Applications

| Application | Port | Tested |
|------------|------|--------|
| ForeFlight | 4000 | Yes |
| Garmin Pilot | 4000 | Yes |
| WingX Pro | 4000 | Not tested |
| FltPlan Go | 4000 | Not tested |

## Building from Source

### Prerequisites

- **CMake** 3.16 or higher
- **C++17 compiler**:
  - Windows: Visual Studio 2019+ or MSVC
  - macOS: Xcode Command Line Tools
  - Linux: GCC 7+ or Clang 6+
- **X-Plane SDK** (included in `SDK/` directory)

### Build Instructions

#### Windows

```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### macOS

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

#### Linux

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The plugin will be built in `build/` directory:
- Windows: `win.xpl`
- macOS: `mac.xpl` (Universal binary)
- Linux: `lin.xpl`

## Configuration Reference

### Network Settings

```ini
[Network]
target_ip = 192.168.1.100    # Target device IP
                             # - Specific IP: 192.168.1.100
                             # - Broadcast: 192.168.1.255
                             # - Global: 255.255.255.255
target_port = 4000           # UDP port (default: 4000)
```

### Ownship Settings

```ini
[Ownship]
icao_address = 0xABCDEF      # 24-bit ICAO address (hex)
callsign = N12345            # Fallback callsign (auto-reads from X-Plane)
emitter_category = 1         # Aircraft type:
                             # 0 = No info
                             # 1 = Light (<15,500 lbs)
                             # 2 = Small (15,500-75,000 lbs)
                             # 3 = Large (75,000-300,000 lbs)
                             # 5 = Heavy (>300,000 lbs)
                             # 7 = Rotorcraft
                             # 9 = Glider
```

### Update Rates

```ini
[Update Rates]
heartbeat_rate = 1.0         # Heartbeats per second (default: 1)
position_rate = 2.0          # Position updates/sec (default: 2)
```

### Accuracy Settings

```ini
[Accuracy]
nic = 11                     # Navigation Integrity Category
                             # 11 = HPL < 7.5m (very high precision)
                             # RECOMMENDED: Use 11 for best EFB compatibility
                             # Many EFB apps filter out low accuracy reports
nacp = 11                    # Navigation Accuracy Category
                             # 11 = HFOM < 3m (very high precision)
                             # RECOMMENDED: Use 11 for best EFB compatibility
```

## Troubleshooting

### Plugin doesn't load

1. Check `X-Plane 12/Log.txt` for error messages (look for `[XP2GDL90]` entries)
2. Ensure plugin is in correct folder: `Resources/plugins/xp2gdl90/`
3. Verify plugin structure:
   ```
   xp2gdl90/
   ├── mac.xpl (macOS)
   └── xp2gdl90.ini
   ```
   Or for Windows/Linux:
   ```
   xp2gdl90/
   ├── 64/
   │   ├── win.xpl (Windows)
   │   └── lin.xpl (Linux)
   └── xp2gdl90.ini
   ```

### Aircraft not showing up in EFB

**Important:** Many EFB applications (especially ForeFlight and Garmin Pilot) **filter out low-accuracy GPS reports**. The default configuration uses `nic=11` and `nacp=11` (highest accuracy) to ensure compatibility with all EFB apps.

**Checklist:**
1. Verify `nic = 11` and `nacp = 11` in `xp2gdl90.ini`
2. Confirm plugin is enabled (check Plugins menu → XP2GDL90)
3. Ensure GPS is available in X-Plane (not in a hangar, GPS panel powered on)
4. Check X-Plane's `Log.txt` for `[XP2GDL90] Plugin enabled` message

### EFB app not receiving data

1. **Network connection**: Ensure X-Plane computer and EFB device are on the **same Wi-Fi network**
2. **IP address**: Confirm `target_ip` in config matches your EFB device's IP
   - Don't use `127.0.0.1` or `localhost` if EFB is on a different device
3. **Firewall**: Ensure **UDP port 4000 is allowed** on both devices
4. **Try broadcast mode**: Set `target_ip = 255.255.255.255` to broadcast to all devices
5. **Restart**: Restart both X-Plane and the EFB app

### Performance issues

If you experience frame rate drops, lower the update rates:

```ini
[Update Rates]
position_rate = 1.0       # 1 update per second
heartbeat_rate = 0.5      # Once every 2 seconds
```

### Advanced Debugging

If you need to troubleshoot at a deeper level, you can enable debug logging:

```ini
[Debug]
debug_logging = true     # Enable detailed logging (may impact performance)
log_messages = true      # Log raw message hex dumps
```

Then check `X-Plane 12/Log.txt` for detailed output.

## GDL90 Protocol Details

This plugin currently transmits a subset of GDL90 messages:

| Message ID | Name | Description |
|------------|------|-------------|
| 0x00 | Heartbeat | Status and timing information (1 Hz) |
| 0x0A | Ownship Report | Own aircraft position and status |

Other GDL90 message types (e.g., Traffic, Uplink Data, Ownship Geometric Altitude) are not transmitted yet.

### Message Format

All transmitted messages follow the GDL90 framing rules:
- CRC-16-CCITT error checking
- Byte stuffing for 0x7D and 0x7E characters
- Flag bytes (0x7E) for message framing

## Technical Details

### Architecture

```
X-Plane DataRefs
       ↓
GDL90 Encoder (C++)
       ↓
UDP Broadcaster
       ↓
Network (UDP)
       ↓
EFB Application
```

### Performance

- **CPU Usage**: < 0.1% on modern systems
- **Memory**: ~1 MB
- **Network Bandwidth**: ~2-5 KB/s

### X-Plane DataRefs Used

| DataRef | Purpose |
|---------|---------|
| `sim/flightmodel/position/latitude` | Ownship latitude |
| `sim/flightmodel/position/longitude` | Ownship longitude |
| `sim/flightmodel/position/elevation` | Ownship altitude (MSL) |
| `sim/flightmodel/position/groundspeed` | Ownship ground speed |
| `sim/flightmodel/position/true_psi` | Ownship true track angle |
| `sim/flightmodel/position/vh_ind_fpm` | Vertical speed (fpm) |
| `sim/flightmodel/failures/onground_any` | Airborne status |
| `sim/aircraft/view/acf_tailnum` | Aircraft tail number (callsign) |

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

### Development Setup

1. Clone the repository with submodules:
   ```bash
   git clone --recursive https://github.com/6639835/xp2gdl90.git
   ```

2. Build using instructions above

3. Test in X-Plane 12

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [X-Plane SDK](https://developer.x-plane.com/sdk/) - Laminar Research
- [GDL90 Specification](https://www.faa.gov/) - Garmin/FAA
- Inspired by the original Python implementation

## Support

- **Issues**: [GitHub Issues](https://github.com/6639835/xp2gdl90/issues)
- **Documentation**: [Wiki](https://github.com/6639835/xp2gdl90/wiki)
- **Discussions**: [GitHub Discussions](https://github.com/6639835/xp2gdl90/discussions)

## Roadmap

- [ ] Geometric altitude support (XPLM 4.1 feature)
- [ ] GUI configuration panel
- [ ] Traffic Report support (AI/multiplayer aircraft)
- [ ] FIS-B weather data integration
- [ ] Multiple EFB simultaneous broadcasting

---

**Made with love for the flight simulation community**
