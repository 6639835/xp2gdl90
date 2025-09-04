#!/usr/bin/env python3
"""
GDL-90 Compliance Test Suite
Tests the XP2GDL90 implementation against the GDL-90 specification (560-1058-00 Rev A)

This test suite validates:
1. Message structure and CRC calculation
2. Heartbeat message format compliance
3. Ownship/Traffic report format compliance  
4. Data encoding accuracy
5. Edge cases and boundary conditions
"""

import struct
import socket
import time
import threading
from typing import List, Tuple, Dict, Any
import json


class GDL90Validator:
    """Validates GDL-90 messages according to specification 560-1058-00 Rev A"""
    
    # CRC-16-CCITT table from GDL-90 specification
    CRC16_TABLE = [
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    ]
    
    @classmethod
    def calculate_crc(cls, data: bytes) -> int:
        """Calculate CRC-16-CCITT per GDL-90 specification Section 2.2.3"""
        crc = 0
        for byte in data:
            crc = cls.CRC16_TABLE[crc >> 8] ^ (crc << 8) ^ byte
            crc &= 0xFFFF
        return crc
    
    @classmethod
    def unstuff_bytes(cls, data: bytes) -> bytes:
        """Remove byte stuffing per GDL-90 specification Section 2.2.1"""
        result = bytearray()
        i = 0
        while i < len(data):
            if data[i] == 0x7D and i + 1 < len(data):
                # Unstuff: remove escape char and XOR next byte with 0x20
                result.append(data[i + 1] ^ 0x20)
                i += 2
            else:
                result.append(data[i])
                i += 1
        return bytes(result)
    
    @classmethod  
    def parse_message(cls, raw_message: bytes) -> Tuple[bool, Dict[str, Any]]:
        """Parse a GDL-90 message and validate format"""
        if len(raw_message) < 5:  # Minimum: Flag + ID + CRC + Flag
            return False, {"error": "Message too short"}
        
        if raw_message[0] != 0x7E or raw_message[-1] != 0x7E:
            return False, {"error": "Invalid flag bytes"}
        
        # Remove flag bytes
        content = raw_message[1:-1]
        
        # Remove byte stuffing
        unstuffed = cls.unstuff_bytes(content)
        
        if len(unstuffed) < 3:  # Minimum: ID + CRC
            return False, {"error": "Unstuffed message too short"}
        
        # Extract message components
        message_id = unstuffed[0]
        message_data = unstuffed[1:-2]
        received_crc = struct.unpack('<H', unstuffed[-2:])[0]  # LSB first
        
        # Calculate expected CRC
        calculated_crc = cls.calculate_crc(unstuffed[:-2])
        
        if received_crc != calculated_crc:
            return False, {
                "error": "CRC mismatch",
                "calculated": f"0x{calculated_crc:04X}",
                "received": f"0x{received_crc:04X}"
            }
        
        return True, {
            "message_id": message_id,
            "data": message_data,
            "crc_valid": True,
            "length": len(message_data)
        }
    
    @classmethod
    def validate_heartbeat(cls, message_data: bytes) -> Tuple[bool, Dict[str, Any]]:
        """Validate heartbeat message per Table 3"""
        if len(message_data) != 6:  # Should be 6 bytes (7 total - 1 for ID)
            return False, {"error": f"Invalid heartbeat length: {len(message_data)}, expected 6"}
        
        status1 = message_data[0]
        status2 = message_data[1] 
        timestamp = struct.unpack('<H', message_data[2:4])[0]  # LSB first
        msg_counts = struct.unpack('>H', message_data[4:6])[0]  # MSB first
        
        # Add timestamp bit 16 from status2 bit 7
        timestamp_full = timestamp | ((status2 & 0x80) << 9)
        
        return True, {
            "status1": f"0x{status1:02X}",
            "status2": f"0x{status2:02X}",
            "timestamp": timestamp_full,
            "message_counts": msg_counts,
            "gps_valid": bool(status1 & 0x80),
            "uat_initialized": bool(status1 & 0x01),
            "utc_ok": bool(status2 & 0x01)
        }
    
    @classmethod
    def validate_position_report(cls, message_data: bytes) -> Tuple[bool, Dict[str, Any]]:
        """Validate ownship position report per Table 8"""
        if len(message_data) != 27:  # Should be 27 bytes (28 total - 1 for ID)
            return False, {"error": f"Invalid position report length: {len(message_data)}, expected 27"}
        
        # Parse according to GDL-90 spec Table 8 format
        status_addr = message_data[0]
        alert_status = (status_addr >> 4) & 0x0F
        addr_type = status_addr & 0x0F
        
        icao_addr = struct.unpack('>I', b'\x00' + message_data[1:4])[0]  # 24-bit big endian
        
        # Latitude: 24-bit signed binary fraction
        lat_raw = struct.unpack('>I', b'\x00' + message_data[4:7])[0]
        if lat_raw & 0x800000:  # Sign bit set
            lat_raw = lat_raw - 0x1000000  # Convert from unsigned to signed
        latitude = lat_raw * (180.0 / 0x800000)
        
        # Longitude: 24-bit signed binary fraction  
        lon_raw = struct.unpack('>I', b'\x00' + message_data[7:10])[0]
        if lon_raw & 0x800000:  # Sign bit set
            lon_raw = lon_raw - 0x1000000  # Convert from unsigned to signed
        longitude = lon_raw * (180.0 / 0x800000)
        
        # Altitude: 12-bit offset integer
        alt_misc = struct.unpack('>H', message_data[10:12])[0]
        altitude_raw = (alt_misc >> 4) & 0x0FFF
        misc = alt_misc & 0x0F
        
        if altitude_raw == 0xFFF:
            altitude = None  # Invalid/unavailable
        else:
            altitude = (altitude_raw * 25) - 1000
        
        # Miscellaneous field breakdown
        air_ground = bool((misc >> 3) & 0x01)
        report_updated = not bool((misc >> 2) & 0x01)
        track_type = misc & 0x03
        
        # Navigation categories
        nic_nacp = message_data[12]
        nic = (nic_nacp >> 4) & 0x0F
        nacp = nic_nacp & 0x0F
        
        # Velocities: 3 bytes total (12 bits each)
        vel_data = struct.unpack('>I', b'\x00' + message_data[13:16])[0]  # 24-bit big endian
        h_velocity = (vel_data >> 12) & 0x0FFF
        v_velocity_raw = vel_data & 0x0FFF
        
        # Handle vertical velocity special values and 2's complement
        if v_velocity_raw == 0x800:
            v_velocity = None  # No data available
        else:
            if v_velocity_raw & 0x800:  # Negative (sign bit set)
                v_velocity = (v_velocity_raw - 0x1000) * 64  # 2's complement
            else:
                v_velocity = v_velocity_raw * 64
        
        # Handle horizontal velocity special values
        if h_velocity == 0xFFF:
            h_velocity = None  # No data available
        elif h_velocity == 0xFFE:
            h_velocity = 4094  # >= 4094 knots
            
        # Track/heading
        track = message_data[16] * (360.0 / 256)
        
        # Emitter category
        emitter_category = message_data[17]
        
        # Call sign (8 bytes)
        callsign = message_data[18:26].decode('ascii', errors='replace').rstrip()
        
        # Emergency code
        emergency_spare = message_data[26]
        emergency_code = (emergency_spare >> 4) & 0x0F
        spare = emergency_spare & 0x0F
        
        return True, {
            "alert_status": alert_status,
            "addr_type": addr_type,
            "icao_address": f"0x{icao_addr:06X}",
            "latitude": latitude,
            "longitude": longitude, 
            "altitude": altitude,
            "airborne": air_ground,
            "report_updated": report_updated,
            "track_type": track_type,
            "nic": nic,
            "nacp": nacp,
            "h_velocity": h_velocity,
            "v_velocity": v_velocity,
            "track": track,
            "emitter_category": emitter_category,
            "callsign": callsign,
            "emergency_code": emergency_code,
            "spare": spare
        }


class GDL90MessageCapture:
    """Captures GDL-90 messages from UDP broadcast"""
    
    def __init__(self, port: int = 4000):
        self.port = port
        self.socket = None
        self.running = False
        self.messages = []
        self.capture_thread = None
    
    def start_capture(self):
        """Start capturing UDP messages"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.socket.bind(('', self.port))
            self.socket.settimeout(1.0)
            
            self.running = True
            self.capture_thread = threading.Thread(target=self._capture_loop)
            self.capture_thread.start()
            
            print(f"Started GDL-90 message capture on port {self.port}")
            return True
            
        except Exception as e:
            print(f"Failed to start capture: {e}")
            return False
    
    def stop_capture(self):
        """Stop capturing messages"""
        self.running = False
        if self.capture_thread:
            self.capture_thread.join()
        if self.socket:
            self.socket.close()
        print("Stopped GDL-90 message capture")
    
    def _capture_loop(self):
        """Main capture loop"""
        while self.running:
            try:
                data, addr = self.socket.recvfrom(1024)
                timestamp = time.time()
                self.messages.append({
                    'timestamp': timestamp,
                    'source': addr,
                    'data': data,
                    'hex': data.hex().upper()
                })
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"Capture error: {e}")
                break
    
    def get_messages(self) -> List[Dict]:
        """Get captured messages"""
        return self.messages.copy()
    
    def clear_messages(self):
        """Clear captured messages"""
        self.messages.clear()


def run_compliance_tests():
    """Run comprehensive GDL-90 compliance tests"""
    print("=" * 60)
    print("GDL-90 COMPLIANCE TEST SUITE")
    print("=" * 60)
    
    # Test 1: CRC Calculation Validation
    print("\n1. Testing CRC Calculation...")
    
    # Test with known example from spec (Section 2.2.4)
    # "The byte sequence [0x7E 0x00 0x81 0x41 0xDB 0xD0 0x08 0x02 0xB3 0x8B 0x7E]"
    # represents a Heartbeat message including the Flags and the CRC value.
    spec_example = bytes([0x7E, 0x00, 0x81, 0x41, 0xDB, 0xD0, 0x08, 0x02, 0xB3, 0x8B, 0x7E])
    
    validator = GDL90Validator()
    valid, result = validator.parse_message(spec_example)
    
    if valid:
        print("✓ Spec example CRC validation PASSED")
        print(f"  Message ID: 0x{result['message_id']:02X}")
        print(f"  Data length: {result['length']} bytes")
    else:
        print("✗ Spec example CRC validation FAILED")
        print(f"  Error: {result.get('error', 'Unknown')}")
    
    # Test 2: Message Structure Tests
    print("\n2. Testing Message Structure...")
    
    test_cases = [
        # Invalid messages
        (b'', "Empty message"),
        (b'\x7E', "Single flag byte"),
        (b'\x7E\x00\x7E', "Message too short"),
        (b'\xFF\x00\x00\x00\xFF', "Invalid flag bytes"),
    ]
    
    for test_msg, description in test_cases:
        valid, result = validator.parse_message(test_msg)
        if not valid:
            print(f"✓ {description} correctly rejected: {result.get('error')}")
        else:
            print(f"✗ {description} incorrectly accepted")
    
    # Test 3: Live Message Capture and Validation
    print("\n3. Testing Live GDL-90 Messages...")
    print("Starting message capture for 10 seconds...")
    
    capture = GDL90MessageCapture(4000)
    
    if not capture.start_capture():
        print("✗ Failed to start message capture")
        return
    
    # Capture for 10 seconds
    time.sleep(10)
    capture.stop_capture()
    
    messages = capture.get_messages()
    print(f"Captured {len(messages)} messages")
    
    if not messages:
        print("⚠ No messages captured. Ensure XP2GDL90 is running and sending to port 4000")
        return
    
    # Analyze captured messages
    message_types = {}
    valid_count = 0
    
    for msg_info in messages[-20:]:  # Analyze last 20 messages
        raw_data = msg_info['data']
        valid, parsed = validator.parse_message(raw_data)
        
        if valid:
            valid_count += 1
            msg_id = parsed['message_id']
            
            if msg_id not in message_types:
                message_types[msg_id] = []
            message_types[msg_id].append(parsed)
            
            print(f"✓ Valid message ID 0x{msg_id:02X} ({len(parsed['data'])} bytes)")
            
            # Detailed validation for specific message types
            if msg_id == 0x00:  # Heartbeat
                hb_valid, hb_result = validator.validate_heartbeat(parsed['data'])
                if hb_valid:
                    print(f"  Heartbeat: GPS={hb_result['gps_valid']}, "
                          f"UTC={hb_result['utc_ok']}, TS={hb_result['timestamp']}")
                else:
                    print(f"  ✗ Heartbeat validation failed: {hb_result.get('error')}")
            
            elif msg_id == 0x0A:  # Ownship Report
                pos_valid, pos_result = validator.validate_position_report(parsed['data'])
                if pos_valid:
                    print(f"  Ownship: LAT={pos_result['latitude']:.6f}, "
                          f"LON={pos_result['longitude']:.6f}, "
                          f"ALT={pos_result['altitude']} ft, "
                          f"Airborne={pos_result['airborne']}")
                    
                    # Test edge cases
                    if pos_result['v_velocity'] is not None and pos_result['v_velocity'] == 0:
                        print("  ✓ Zero vertical speed correctly encoded (not as 'no data')")
                else:
                    print(f"  ✗ Ownship validation failed: {pos_result.get('error')}")
            
            elif msg_id == 0x14:  # Traffic Report  
                traffic_valid, traffic_result = validator.validate_position_report(parsed['data'])
                if traffic_valid:
                    print(f"  Traffic: {traffic_result['callsign']}, "
                          f"ALT={traffic_result['altitude']} ft, "
                          f"Airborne={traffic_result['airborne']}")
        else:
            print(f"✗ Invalid message: {parsed.get('error')}")
    
    # Test Summary
    print(f"\n4. Test Summary:")
    print(f"Valid messages: {valid_count}/{len(messages[-20:])}")
    print(f"Message types found: {list(message_types.keys())}")
    
    # Expected message types per GDL-90 spec Table 2
    expected_types = {
        0x00: "Heartbeat (required)",
        0x0A: "Ownship Report (required)", 
        0x14: "Traffic Report (optional)"
    }
    
    for msg_id, description in expected_types.items():
        if msg_id in message_types:
            count = len(message_types[msg_id])
            print(f"✓ {description}: {count} messages")
        else:
            print(f"⚠ {description}: Not found")
    
    # Test 4: Data Encoding Validation
    print(f"\n5. Data Encoding Validation:")
    
    if 0x0A in message_types:
        ownship_msgs = message_types[0x0A]
        if ownship_msgs:
            latest_ownship = ownship_msgs[-1]
            pos_valid, pos_data = validator.validate_position_report(latest_ownship['data'])
            
            if pos_valid:
                # Check for reasonable values
                lat, lon, alt = pos_data['latitude'], pos_data['longitude'], pos_data['altitude']
                
                if -90 <= lat <= 90:
                    print(f"✓ Latitude in valid range: {lat:.6f}°")
                else:
                    print(f"✗ Latitude out of range: {lat:.6f}°")
                
                if -180 <= lon <= 180:
                    print(f"✓ Longitude in valid range: {lon:.6f}°")
                else:
                    print(f"✗ Longitude out of range: {lon:.6f}°")
                
                if alt is not None and -1000 <= alt <= 101350:
                    print(f"✓ Altitude in valid range: {alt} ft")
                else:
                    print(f"✗ Altitude out of range: {alt} ft")
                
                # Check miscellaneous field encoding  
                track_type_map = {0: "Not Valid", 1: "True Track", 2: "Magnetic Heading", 3: "True Heading"}
                print(f"✓ Aircraft state: {'Airborne' if pos_data['airborne'] else 'On Ground'}")
                print(f"✓ Track type: {track_type_map.get(pos_data['track_type'], 'Unknown')}")
    
    print("\n" + "=" * 60)
    print("COMPLIANCE TEST COMPLETE")
    print("=" * 60)


def test_edge_cases():
    """Test edge cases and boundary conditions"""
    print("\n" + "=" * 60)
    print("EDGE CASE TESTING")
    print("=" * 60)
    
    # These would require mock data injection or a test harness
    # For now, document the edge cases that should be tested
    
    edge_cases = [
        "Zero vertical speed (0 FPM) - should encode as 0x000, not 0x800",
        "Maximum altitude (101,350 ft) - should encode as 0xFFE",
        "Minimum altitude (-1,000 ft) - should encode as 0x000", 
        "Invalid altitude - should encode as 0xFFF",
        "Maximum horizontal velocity (4,094+ kt) - should encode as 0xFFE",
        "No horizontal velocity data - should encode as 0xFFF",
        "Ground state aircraft - misc field bit 3 should be 0",
        "Airborne aircraft - misc field bit 3 should be 1",
        "Latitude/longitude boundary values (+/-90°, +/-180°)",
        "Call sign formatting (8 characters, space-padded)"
    ]
    
    print("Edge cases to validate in live testing:")
    for i, case in enumerate(edge_cases, 1):
        print(f"{i:2d}. {case}")
    
    print(f"\n⚠ Run this script while flying in X-Plane to test these cases")


if __name__ == "__main__":
    try:
        run_compliance_tests()
        test_edge_cases()
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
    except Exception as e:
        print(f"\nTest failed with error: {e}")
        import traceback
        traceback.print_exc()
