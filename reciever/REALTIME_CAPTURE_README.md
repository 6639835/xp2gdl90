# Real-time GDL-90 Capture and Decoder

## Overview

The `realtime_gdl90_capture.py` script combines UDP packet capture, GDL-90 decoding, and data saving into one efficient real-time tool. Unlike the separate capture→decode→save workflow, this script processes packets as they arrive and provides live feedback.

## Features

- ✅ **Real-time UDP capture** from XP2GDL90 plugin
- ✅ **Live GDL-90 decoding** with full message parsing
- ✅ **CRC validation** for data integrity
- ✅ **Auto-save** every 10 seconds (configurable)
- ✅ **Live status display** showing decoded messages
- ✅ **Comprehensive statistics** (success rates, message types)
- ✅ **JSON output format** for easy analysis
- ✅ **Error handling** and graceful recovery

## Usage

### Basic Usage
```bash
# Capture for 30 seconds (default settings)
python3 realtime_gdl90_capture.py 30
```

### Advanced Usage
```bash
# Custom IP, port, and output file
python3 realtime_gdl90_capture.py 60 192.168.1.100 4000 flight_data.json

# Quick 10-second test
python3 realtime_gdl90_capture.py 10

# Extended monitoring session
python3 realtime_gdl90_capture.py 300 127.0.0.1 4000 extended_flight.json
```

### Parameters
1. **Duration (required)**: Capture time in seconds
2. **IP (optional)**: Source IP address (default: 127.0.0.1)
3. **Port (optional)**: UDP port to listen on (default: 4000)
4. **Output file (optional)**: JSON output filename (default: realtime_capture.json)

## Live Output Example

```
=== Real-time GDL-90 Capture Started ===
Listening on: 127.0.0.1:4000
Duration: 30 seconds
Output file: realtime_capture.json
Auto-save every: 10 seconds

[14:23:15.123] ✓ Heartbeat | Count: 42 | UTC: 14:23:15
[14:23:15.456] ✓ Ownship Position Report | PYTHON1 | (37.621311, -122.378968) | 1247ft
[14:23:15.789] ✓ Traffic Report | UAL123 | (37.774900, -122.419400) | 35000ft
[14:23:16.012] ✗ Traffic Report | TRF042 | (50.031545, 8.564808) | 0ft
    └── Auto-saved 25 packets
```

## Output Format

The script saves data in JSON format with three main sections:

### 1. Capture Info
```json
{
  "capture_info": {
    "timestamp": "2024-01-15T14:23:45.123456",
    "capture_duration": 147,
    "source": "127.0.0.1:4000",
    "decoder_version": "realtime_v1.0"
  }
}
```

### 2. Statistics
```json
{
  "statistics": {
    "total_packets": 147,
    "valid_packets": 145,
    "crc_errors": 2,
    "decode_errors": 0,
    "message_types": {
      "Heartbeat": 10,
      "Ownship Position Report": 18,
      "Traffic Report": 119
    }
  }
}
```

### 3. Decoded Packets
```json
{
  "packets": [
    {
      "message_type": "Traffic Report",
      "capture_timestamp": 1705317825.123,
      "latitude": 50.031545,
      "longitude": 8.564808,
      "altitude": 0,
      "horizontal_velocity": "0",
      "vertical_velocity": "No data",
      "track": 0.0,
      "callsign": "\"TRF042\"",
      "icao_address": "0x10002A",
      "crc_valid": true,
      "crc_received": "0x1234",
      "crc_calculated": "0x1234"
    }
  ]
}
```

## Message Types Supported

| Message ID | Type | Description |
|------------|------|-------------|
| 0x00 | Heartbeat | Status and timing information |
| 0x0A | Ownship Position Report | Your aircraft's position |
| 0x14 | Traffic Report | Other aircraft positions |

## Troubleshooting

### No packets received
- Verify X-Plane is running with XP2GDL90 plugin loaded
- Check plugin is broadcasting to correct IP:port
- Ensure firewall allows UDP traffic on specified port

### CRC errors
- Check for network transmission issues
- Verify plugin version compatibility
- Look for interference from other UDP traffic

### High decode errors
- Plugin may be sending malformed data
- Check plugin configuration
- Try updating plugin to latest version

## Comparison with Other Scripts

| Script | Capture | Decode | Save | Real-time | Use Case |
|--------|---------|--------|------|-----------|----------|
| `test_udp_capture.py` | ✅ | ❌ | ✅ (raw) | ❌ | Raw packet capture |
| `gdl90_decoder.py` | ❌ | ✅ | ❌ | ❌ | Post-processing |
| `realtime_gdl90_capture.py` | ✅ | ✅ | ✅ (decoded) | ✅ | **Complete solution** |

## Tips

1. **Monitor live**: Watch the real-time output to see what's happening
2. **Use shorter durations**: Start with 10-30 seconds for testing
3. **Check auto-saves**: Data is saved every 10 seconds to prevent loss
4. **Analyze JSON**: Use `jq` or Python to analyze the output file
5. **Stop early**: Use Ctrl+C to stop capture before duration expires

## Example Analysis

After capture, you can analyze the data:

```bash
# Count message types
cat realtime_capture.json | jq '.statistics.message_types'

# Find unique callsigns
cat realtime_capture.json | jq '.packets[].callsign' | sort | uniq

# Get coordinates for all traffic
cat realtime_capture.json | jq '.packets[] | select(.message_type=="Traffic Report") | {callsign, latitude, longitude}'
```
