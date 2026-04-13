# XP2GDL90

[![Build Status](https://github.com/6639835/xp2gdl90/workflows/Build%20XP2GDL90/badge.svg)](https://github.com/6639835/xp2gdl90/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

`xp2gdl90` is an X-Plane 12 plugin that broadcasts simulator data over UDP in GDL90 format for EFB apps such as ForeFlight, Garmin Pilot, WingX, and FltPlan Go.

The current codebase includes:

- Standard GDL90 heartbeat, ownship report, and ownship geometric altitude messages
- ForeFlight ID and AHRS extension messages
- ForeFlight auto-discovery with fallback to a manual target IP/port
- Traffic broadcasting from TCAS targets, with legacy multiplayer fallback
- An in-sim ImGui settings and status window
- Cross-platform builds for macOS, Windows, and Linux
- A unit test target and coverage script

## Release Installation

Download the latest release from [GitHub Releases](https://github.com/6639835/xp2gdl90/releases), extract it, and copy the `xp2gdl90` folder into:

```text
X-Plane 12/Resources/plugins/
```

The packaged release layout is:

```text
xp2gdl90/
├── mac.xpl
├── 64/
│   ├── win.xpl
│   └── lin.xpl
├── README.md
└── LICENSE
```

After launching X-Plane, open:

```text
Plugins -> XP2GDL90 -> Settings...
```

You can also toggle broadcasting from:

```text
Plugins -> XP2GDL90 -> Enable Broadcasting
```

Settings are stored in X-Plane's preferences directory as `xp2gdl90.json`, typically:

```text
Output/preferences/xp2gdl90.json
```

## Quick Setup

For a basic manual setup:

1. Install the plugin.
2. Open `Plugins -> XP2GDL90 -> Settings...`.
3. Set `Target IP` to the tablet or EFB device IP.
4. Leave `Target Port` at `4000` unless your app requires something else.
5. Keep `NIC` and `NACp` at `11`.
6. Save the settings.
7. Confirm the Status tab shows packets being sent.

For ForeFlight, the plugin can also listen for discovery broadcasts on UDP `63093` and temporarily switch the broadcast target to the discovered host and port.

## Build From Source

The repository uses the X-Plane SDK from `SDK/` and Dear ImGui as a git submodule under `third_party/imgui`.

Clone with submodules:

```bash
git clone --recursive https://github.com/6639835/xp2gdl90.git
cd xp2gdl90
```

### Prerequisites

- CMake 3.16+
- C++17 compiler
- X-Plane SDK headers and libraries in `SDK/` (already vendored here)
- OpenGL development libraries on Linux

Platform notes:

- macOS: Xcode Command Line Tools
- Windows: Visual Studio 2022 or a compatible MSVC toolchain
- Linux: GCC 7+ or Clang 6+ plus OpenGL development packages

### Configure And Build

Generic CMake flow:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Expected output locations:

- macOS: `build/mac.xpl`
- Linux: `build/lin.xpl`
- Windows: `build/win.xpl` or `build/Release/win.xpl` depending on generator

Helper scripts are also included:

- macOS: `./build_mac.sh`
- Linux: `./build_linux.sh`
- Windows: `build_win.bat`

## Testing

Enable the test target with:

```bash
cmake -S . -B build -DXP2GDL90_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target xp2gdl90_tests --config Debug
ctest --test-dir build --output-on-failure
```

A coverage helper is available at `scripts/coverage.sh`. It configures a separate coverage build under `build/coverage`, runs `ctest`, and reports line and function coverage for `src/`. By default it enforces at least `97.5%` line coverage and `100%` function coverage, and you can override those thresholds with `MIN_LINES_PERCENT` and `MIN_FUNCTIONS_PERCENT`.

## Runtime Behavior

The plugin currently sends:

- `0x00` Heartbeat at the configured `heartbeat_rate`
- `0x0A` Ownship Report at the configured `position_rate`
- `0x0B` Ownship Geometric Altitude at 1 Hz
- `0x14` Traffic Report at 1 Hz
- `0x65/0x00` ForeFlight ID at 1 Hz
- `0x65/0x01` ForeFlight AHRS at 5 Hz

Current behavior from the implementation:

- ForeFlight auto-discovery is optional and listens on the configured broadcast port
- Manual `target_ip` and `target_port` are used as the fallback target
- The effective callsign uses the aircraft tail number when available, otherwise the configured fallback callsign
- Ownship report altitude uses `sim/cockpit2/gauges/indicators/altitude_ft_pilot` when available
- AHRS heading can be transmitted as true or magnetic heading
- Traffic is sourced from TCAS target datarefs when available, otherwise from legacy multiplayer datarefs

Weather uplink data is not transmitted.

## Configuration

The on-disk settings file is JSON:

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

Field reference:

| Key | Type | Notes |
| --- | --- | --- |
| `target_ip` | string | Manual UDP target. Can be a unicast address, subnet broadcast, or `255.255.255.255`. |
| `target_port` | number | Manual UDP destination port. |
| `foreflight_auto_discovery` | boolean | Enables the listener for ForeFlight discovery broadcasts. |
| `foreflight_broadcast_port` | number | Discovery listen port. Default is `63093`. |
| `icao_address` | number | Stored in JSON as a decimal 24-bit value. The UI accepts hex such as `0xABCDEF`. |
| `callsign` | string | Fallback only. Trimmed to 8 characters. |
| `emitter_category` | number | Valid range `0-39`. |
| `device_name` | string | ForeFlight device name. Trimmed to 8 characters. |
| `device_long_name` | string | ForeFlight long name. Trimmed to 16 characters. |
| `internet_policy` | number | `0=Unrestricted`, `1=Expensive`, `2=Disallowed`. |
| `ahrs_use_magnetic_heading` | boolean | `false` sends true heading, `true` converts to magnetic heading. |
| `heartbeat_rate` | number | Must be greater than `0`. |
| `position_rate` | number | Must be greater than `0`. |
| `nic` | number | Valid range `0-11`. `11` is recommended for EFB compatibility. |
| `nacp` | number | Valid range `0-11`. `11` is recommended for EFB compatibility. |
| `debug_logging` | boolean | Enables plugin debug logging to `Log.txt`. |
| `log_messages` | boolean | Enables raw message logging. |

## In-Sim UI

The settings window currently exposes these tabs:

- `Status`
- `Network`
- `Ownship`
- `Device`
- `Rates`
- `Accuracy`
- `Debug`

The window supports `Apply`, `Save`, `Revert`, and `Defaults`.

## Data Sources

Key X-Plane datarefs used by the plugin include:

- `sim/flightmodel/position/latitude`
- `sim/flightmodel/position/longitude`
- `sim/flightmodel/position/elevation`
- `sim/cockpit2/gauges/indicators/altitude_ft_pilot`
- `sim/flightmodel/position/groundspeed`
- `sim/flightmodel/position/true_psi`
- `sim/flightmodel/position/theta`
- `sim/flightmodel/position/phi`
- `sim/flightmodel/position/psi`
- `sim/flightmodel/position/indicated_airspeed`
- `sim/flightmodel/position/true_airspeed`
- `sim/flightmodel/position/vh_ind_fpm`
- `sim/flightmodel/failures/onground_any`
- `sim/aircraft/view/acf_tailnum`
- `sim/cockpit2/tcas/targets/*`
- `sim/multiplayer/position/planeN_*`

## Troubleshooting

### Plugin does not load

- Check X-Plane's `Log.txt` for lines prefixed with `[XP2GDL90]`
- Verify the plugin directory is `Resources/plugins/xp2gdl90/`
- Verify the correct platform binary exists inside the folder
- If building from source, make sure the ImGui submodule is present

### The EFB does not show the aircraft

- Make sure the X-Plane machine and EFB device are on the same network
- Confirm the target IP is correct if you are not relying on ForeFlight auto-discovery
- Keep `NIC=11` and `NACp=11`
- Check the Status tab to confirm heartbeat and position packets are increasing
- Check `Log.txt` for UDP send errors

### ForeFlight auto-discovery does not work

- Confirm `ForeFlight auto discovery` is enabled
- Confirm the discovery port matches the app environment; default is `63093`
- Check that firewall rules allow UDP traffic
- Use a manual target IP/port as fallback

### Settings changes do not persist

- Use `Save`, not only `Apply`
- Check whether `xp2gdl90.json` was created in the X-Plane preferences directory
- Check `Log.txt` for a settings write error

## Repository Layout

```text
.
├── CMakeLists.txt
├── SDK/
├── docs/
├── include/xp2gdl90/
├── scripts/
├── src/
├── tests/
└── third_party/imgui/
```

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

## Support

- Issues: [GitHub Issues](https://github.com/6639835/xp2gdl90/issues)
- Actions: [GitHub Actions](https://github.com/6639835/xp2gdl90/actions)
