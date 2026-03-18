# XP2GDL90 - X-Plane to GDL90 Bridge

[![Build Status](https://github.com/6639835/xp2gdl90/workflows/Build%20XP2GDL90/badge.svg)](https://github.com/6639835/xp2gdl90/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance X-Plane 12 plugin that broadcasts real-time flight data in GDL90 format via UDP. Compatible with ForeFlight, Garmin Pilot, WingX, and other Electronic Flight Bag (EFB) applications.

## Features

- **Real-time Position Broadcasting**: Ownship position, altitude, speed, and heading
- **ForeFlight Extended Spec Support**: Device ID, AHRS, and ForeFlight auto-discovery
- **Standard GDL90 Support**: Heartbeat, Ownship Report, Ownship Geometric Altitude, and Traffic Report
- **Traffic Report Broadcasting**: Remote multiplayer / xPilot traffic as GDL90 `0x14` reports with TCAS-derived address, emergency, and transponder-state handling
- **High Performance**: Native C++ plugin, minimal CPU overhead
- **Cross-Platform**: Windows, macOS (Universal), and Linux support
- **Fully Configurable**: In-sim settings UI backed by JSON config
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
4. **Configure** your settings in-sim: **Plugins → XP2GDL90 → Settings...**
5. **Start** X-Plane 12

### Basic Configuration

Open **Plugins → XP2GDL90 → Settings...** and set:
- Target IP / Port fallback
- ForeFlight auto-discovery / discovery port
- ICAO address, callsign fallback, emitter category
- Device name / internet policy / AHRS heading mode
- Update rates and accuracy values

ForeFlight device-ID broadcasts run automatically at 1 Hz, AHRS broadcasts run automatically at 5 Hz, standard GDL90 Ownship Geometric Altitude is sent at 1 Hz, and the plugin can auto-switch to ForeFlight unicast when it sees ForeFlight's discovery broadcast.

Settings are saved to X‑Plane’s preferences folder as `Output/preferences/xp2gdl90.json`.

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

Settings are stored as JSON in `Output/preferences/xp2gdl90.json`.

```json
{
  "target_ip": "192.168.1.100",
  "target_port": 4000,
  "foreflight_auto_discovery": true,
  "foreflight_broadcast_port": 63093,
  "icao_address": 11259375,
  "callsign": "N12345",
  "emitter_category": 1,
  "device_name": "XP2GDL90",
  "device_long_name": "XP2GDL90 AHRS",
  "internet_policy": 0,
  "ahrs_use_magnetic_heading": false,
  "heartbeat_rate": 1.0,
  "position_rate": 2.0,
  "nic": 11,
  "nacp": 11,
  "debug_logging": false,
  "log_messages": false
}
```

Field notes:
- `target_ip`: specific target, subnet broadcast, or `255.255.255.255`
- `icao_address`: 24-bit ICAO address stored as a decimal JSON number
- `emitter_category`: `0-39`; common values include `1` light, `2` small, `3` large, `5` heavy, `7` rotorcraft, `9` glider
- `internet_policy`: `0=Unrestricted`, `1=Expensive`, `2=Disallowed`
- `ahrs_use_magnetic_heading`: `false` uses true heading, `true` converts via `XPLMDegTrueToDegMagnetic`
- `heartbeat_rate` and `position_rate`: updates per second, must be greater than `0`
- `nic` and `nacp`: `0-11`; `11` is recommended for EFB compatibility

## Troubleshooting

### Plugin doesn't load

1. Check `X-Plane 12/Log.txt` for error messages (look for `[XP2GDL90]` entries)
2. Ensure plugin is in correct folder: `Resources/plugins/xp2gdl90/`
3. Verify plugin structure:
   ```
   xp2gdl90/
   ├── mac.xpl (macOS)
   └── xp2gdl90.json (created after first Save)
   ```
   Or for Windows/Linux:
   ```
   xp2gdl90/
   ├── 64/
   │   ├── win.xpl (Windows)
   │   └── lin.xpl (Linux)
   └── xp2gdl90.json (created after first Save)
   ```

### Aircraft not showing up in EFB

**Important:** Many EFB applications (especially ForeFlight and Garmin Pilot) **filter out low-accuracy GPS reports**. The default configuration uses `nic=11` and `nacp=11` (highest accuracy) to ensure compatibility with all EFB apps.

**Checklist:**
1. Verify `NIC = 11` and `NACp = 11` in **Plugins → XP2GDL90 → Settings...**
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

```json
{
  "position_rate": 1.0,
  "heartbeat_rate": 0.5
}
```

### Advanced Debugging

If you need to troubleshoot at a deeper level, you can enable debug logging:

```json
{
  "debug_logging": true,
  "log_messages": true
}
```

Then check `X-Plane 12/Log.txt` for detailed output.

## GDL90 Protocol Details

This plugin currently transmits a subset of GDL90 messages:

| Message ID | Name | Description |
|------------|------|-------------|
| 0x00 | Heartbeat | Status and timing information (1 Hz) |
| 0x0A | Ownship Report | Own aircraft position and status |
| 0x0B | Ownship Geometric Altitude | Standard GDL90 geometric altitude message (MSL capability advertised via ForeFlight ID mask) |
| 0x14 | Traffic Report | Multiplayer / TCAS traffic targets (1 Hz) |
| 0x65 / 0x00 | ForeFlight ID | Device name / capabilities extension |
| 0x65 / 0x01 | ForeFlight AHRS | Roll, pitch, and configurable true/magnetic heading at 5 Hz |

Weather / `Uplink Data (0x07)` is intentionally not transmitted, because this plugin does not have a complete native UAT weather source. Ownship geometric altitude remains a standard GDL90 message; this plugin advertises its MSL datum through the ForeFlight ID capabilities mask as described in the ForeFlight extension spec.

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
| `sim/cockpit2/gauges/indicators/pressure_alt_ft_pilot` | Ownship pressure altitude for standards-compliant Ownship Report altitude |
| `sim/flightmodel/position/groundspeed` | Ownship ground speed |
| `sim/flightmodel/position/true_psi` | Ownship true track angle |
| `sim/flightmodel/position/theta` | AHRS pitch |
| `sim/flightmodel/position/phi` | AHRS roll |
| `sim/flightmodel/position/psi` | AHRS heading |
| `sim/flightmodel/position/indicated_airspeed` | AHRS indicated airspeed |
| `sim/flightmodel/position/true_airspeed` | AHRS true airspeed |
| `sim/flightmodel/position/vh_ind_fpm` | Vertical speed (fpm) |
| `sim/flightmodel/failures/onground_any` | Airborne status |
| `sim/aircraft/view/acf_tailnum` | Aircraft tail number (callsign) |
| `sim/cockpit2/tcas/targets/modeS_id` | Remote traffic address / real-ICAO indicator |
| `sim/cockpit2/tcas/targets/modeC_code` | Remote traffic squawk for emergency mapping |
| `sim/cockpit2/tcas/targets/ssr_mode` | Remote traffic transponder mode / altitude capability |
| `sim/cockpit2/tcas/targets/*` | Remote traffic position / velocity / state (preferred) |
| `sim/multiplayer/position/planeN_*` | Legacy multiplayer traffic fallback |
| `sim/multiplayer/position/planeN_tailnum` | Shared remote traffic callsign / tail number |

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

- [ ] Multiple EFB simultaneous broadcasting

---

**Made with love for the flight simulation community**
