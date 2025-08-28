# XP2GDL90 Test Suite

This test suite validates the complete GDL-90 encoding pipeline of the XP2GDL90 plugin by capturing UDP output, decoding according to the official GDL-90 specification, and verifying data accuracy.

## Test Scripts Overview

### 🎯 `run_gdl90_tests.py` - Main Test Runner
Comprehensive end-to-end test suite that orchestrates all testing phases.

**Usage:**
```bash
# Full test (30 second capture)
./run_gdl90_tests.py

# Quick test (10 second capture) 
./run_gdl90_tests.py --quick

# Generate test cases only
./run_gdl90_tests.py --generate-only

# Decode specific hex data
./run_gdl90_tests.py --decode "7E 00 81 01 00 00 00 00 8C 73 7E"
```

### 📡 `test_udp_capture.py` - UDP Packet Capture
Captures GDL-90 UDP packets from the plugin for analysis.

**Usage:**
```bash
# Capture for 30 seconds on default port
./test_udp_capture.py 30

# Capture on specific IP/port
./test_udp_capture.py 60 127.0.0.1 4000
```

### 🔓 `gdl90_decoder.py` - GDL-90 Message Decoder
Decodes GDL-90 messages according to the official specification.

**Usage:**
```bash
# Decode captured packets from file
./gdl90_decoder.py captured_gdl90.bin

# Test decoder with sample data
./gdl90_decoder.py --test
```

### 📋 `test_data_generator.py` - Test Case Generator
Generates comprehensive test cases with known input/output values.

**Usage:**
```bash
# Generate and display test cases
./test_data_generator.py
```

### ✅ `verify_gdl90.py` - Message Verification Tool
Quick verification tool for individual GDL-90 messages.

**Usage:**
```bash
# Verify specific hex message
./verify_gdl90.py "7E 14 00 10 00 01 1A C0 4E A9 01 F4 02 37 9B BA 09 1F F7 BE 01 4E 37 33 37 42 41 20 20 00 8C 73 7E"

# Test with sample message
./verify_gdl90.py --sample
```

## Complete Test Workflow

### Prerequisites
1. **X-Plane 12** running with **XP2GDL90 plugin** loaded
2. **AI traffic enabled** in X-Plane (Aircraft & Situations → AI Aircraft)
3. **Plugin configured** to broadcast to `127.0.0.1:4000`
4. **Python 3.6+** with no additional dependencies required

### Step-by-Step Testing

#### 1. Quick Verification Test
```bash
# Test the decoder with sample data
./verify_gdl90.py --sample
```

#### 2. Live Plugin Test
```bash
# Run complete test suite (will guide you through setup)
./run_gdl90_tests.py --quick
```

#### 3. Full Validation Test
```bash
# Extended test with comprehensive analysis
./run_gdl90_tests.py
```

### Test Outputs

#### Generated Files
- **`test_cases.json`** - Generated test cases with expected values
- **`test_capture.bin`** - Captured UDP packets in binary format
- **`test_report.json`** - Comprehensive test results and analysis

#### Test Report Contents
- **Packet Statistics** - Total packets, CRC errors, message types
- **Message Analysis** - Detailed breakdown of each GDL-90 message
- **Encoding Validation** - Verification of position/speed/altitude encoding
- **Recommendations** - Issues found and suggested fixes

## Understanding Test Results

### ✅ Good Results
```
Captured packets: 45
Valid packets: 45  
CRC errors: 0
Message types:
  Heartbeat: 30
  Ownship Position: 10
  Traffic Report: 5
```

### ⚠️ Potential Issues
```
Captured packets: 0
→ Plugin not running or not broadcasting

CRC errors: 15/45 (33%)
→ Network transmission issues

No traffic reports found
→ AI traffic not enabled in X-Plane
```

## Validation Details

### Position Encoding Verification
The test suite validates that positions are encoded correctly according to GDL-90 spec:
- **Resolution**: 180° / 2²³ = ~0.0000214576° (~2.38m at equator)
- **Range**: ±90° latitude, ±180° longitude
- **Format**: 24-bit signed binary fraction

### Altitude Encoding Verification  
- **Resolution**: 25-foot increments
- **Offset**: +1000 feet
- **Range**: -1000 to +101,350 feet
- **Invalid**: 0xFFF value

### Speed Encoding Verification
- **Horizontal**: 1 knot resolution, 0-4094 knots
- **Vertical**: 64 fpm resolution, ±131,008 fpm
- **No data flags**: 0xFFF (horizontal), 0x800 (vertical)

### CRC Validation
All messages are validated using CRC-16-CCITT with polynomial 0x1021.

## Troubleshooting

### Common Issues

**No packets captured:**
- Verify X-Plane is running with plugin loaded
- Check plugin configuration (IP: 127.0.0.1, Port: 4000)
- Ensure no firewall blocking UDP traffic

**CRC errors:**
- Network transmission issues
- Plugin encoding problems
- Capture timing issues

**Invalid coordinates:**
- X-Plane aircraft outside valid range
- Dataref reading errors
- Unit conversion problems

**Missing traffic:**
- AI traffic not enabled in X-Plane
- TCAS datarefs not available
- Traffic outside TCAS range

### Manual Testing

For debugging specific issues, use individual test scripts:

```bash
# Test UDP capture only
./test_udp_capture.py 10

# Decode specific message
./verify_gdl90.py "7E 00 81 01 00 00 8C 73 7E"

# Generate reference test cases
./test_data_generator.py
```

## Test Data Examples

### Sample Heartbeat Message
```
7E 00 81 01 00 00 00 00 8C 73 7E
│  │  │  │  │     │  │  │     │
│  │  │  │  │     │  │  └─────┴─ CRC-16 + End Flag
│  │  │  │  │     └──┴───────── Message Count  
│  │  │  │  └─────────────────── Timestamp (low 16 bits)
│  │  │  └────────────────────── Status Byte 2
│  │  └───────────────────────── Status Byte 1
│  └──────────────────────────── Message ID (Heartbeat)
└─────────────────────────────── Start Flag
```

### Sample Traffic Report Message
```
7E 14 00 10 00 01 [24-bit lat] [24-bit lon] [alt+misc] [nic+nacp] 
[h_vel+v_vel] [track] [emitter] [8-byte callsign] [emergency] [crc] 7E
```

The test suite provides complete validation of this complex encoding to ensure your EFB receives accurate traffic data!
