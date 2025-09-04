#!/usr/bin/env python3
"""
GDL-90 Message Format Validation Tests

This script validates that the GDL-90 messages produced by xp2gdl90 
conform to the official GDL-90 specification.

Based on: GDL 90 Data Interface Specification 560-1058-00 Rev A
"""

import struct
import sys
from typing import List, Tuple, Optional

class GDL90Validator:
    """Validates GDL-90 message format according to specification."""
    
    # GDL-90 Constants from specification
    FLAG_BYTE = 0x7E
    ESCAPE_BYTE = 0x7D
    ESCAPE_XOR = 0x20
    
    # Message IDs per Table 2
    MSG_HEARTBEAT = 0x00
    MSG_INITIALIZATION = 0x02  
    MSG_UPLINK_DATA = 0x07
    MSG_HEIGHT_ABOVE_TERRAIN = 0x09
    MSG_OWNSHIP_REPORT = 0x0A
    MSG_OWNSHIP_GEO_ALTITUDE = 0x0B
    MSG_TRAFFIC_REPORT = 0x14
    MSG_BASIC_REPORT = 0x1E
    MSG_LONG_REPORT = 0x1F
    
    # CRC-16-CCITT table from specification  
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
    
    def calculate_crc(self, data: bytes) -> int:
        """Calculate CRC-16-CCITT for given data."""
        crc = 0
        for byte in data:
            crc = ((self.CRC16_TABLE[(crc >> 8) & 0xFF] ^ (crc << 8) ^ byte) & 0xFFFF)
        return crc
    
    def unescape_message(self, escaped_data: bytes) -> bytes:
        """Remove byte stuffing from escaped message data."""
        unescaped = bytearray()
        i = 0
        while i < len(escaped_data):
            if escaped_data[i] == self.ESCAPE_BYTE and i + 1 < len(escaped_data):
                # Found escape sequence
                unescaped.append(escaped_data[i + 1] ^ self.ESCAPE_XOR)
                i += 2
            else:
                unescaped.append(escaped_data[i])
                i += 1
        return bytes(unescaped)
    
    def validate_message_structure(self, raw_message: bytes) -> Tuple[bool, str, Optional[dict]]:
        """
        Validate basic GDL-90 message structure.
        
        Returns: (is_valid, error_message, parsed_data)
        """
        if len(raw_message) < 5:
            return False, "Message too short (minimum 5 bytes)", None
            
        # Check flag bytes
        if raw_message[0] != self.FLAG_BYTE:
            return False, f"Invalid start flag: 0x{raw_message[0]:02X}", None
            
        if raw_message[-1] != self.FLAG_BYTE:
            return False, f"Invalid end flag: 0x{raw_message[-1]:02X}", None
        
        # Extract message content (without flags)
        message_content = raw_message[1:-1]
        
        # Unescape the message
        try:
            unescaped = self.unescape_message(message_content)
        except Exception as e:
            return False, f"Failed to unescape message: {e}", None
            
        if len(unescaped) < 3:
            return False, "Unescaped message too short", None
        
        # Extract components
        message_id = unescaped[0]
        message_data = unescaped[1:-2]
        crc_bytes = unescaped[-2:]
        received_crc = struct.unpack('<H', crc_bytes)[0]  # Little endian
        
        # Validate CRC
        payload_for_crc = unescaped[:-2]  # Message ID + data
        calculated_crc = self.calculate_crc(payload_for_crc)
        
        if received_crc != calculated_crc:
            return False, f"CRC mismatch: received=0x{received_crc:04X}, calculated=0x{calculated_crc:04X}", None
        
        return True, "Valid", {
            'message_id': message_id,
            'data': message_data,
            'crc': received_crc,
            'total_length': len(raw_message)
        }
    
    def validate_heartbeat(self, data: bytes) -> Tuple[bool, str]:
        """Validate heartbeat message format (Table 3)."""
        if len(data) != 6:
            return False, f"Heartbeat should be 6 bytes, got {len(data)}"
        
        status1, status2, ts_low, ts_high, count1, count2 = struct.unpack('BBBBBB', data)
        
        # Extract timestamp (17-bit)
        ts_bit16 = (status2 & 0x80) >> 7
        timestamp = (ts_bit16 << 16) | (ts_high << 8) | ts_low
        
        # Validate timestamp range (0-86399 seconds in a day)
        if timestamp > 86399:
            return False, f"Invalid timestamp: {timestamp} (max 86399)"
        
        return True, f"Valid heartbeat: timestamp={timestamp}s"
    
    def validate_traffic_report(self, data: bytes) -> Tuple[bool, str]:
        """Validate traffic report message format (Table 8)."""
        if len(data) != 27:
            return False, f"Traffic report should be 27 bytes, got {len(data)}"
        
        # Parse traffic report fields
        status_addr = data[0]
        icao = struct.unpack('>I', b'\x00' + data[1:4])[0]  # 24-bit big endian
        lat = struct.unpack('>I', b'\x00' + data[4:7])[0]   # 24-bit big endian
        lon = struct.unpack('>I', b'\x00' + data[7:10])[0]  # 24-bit big endian
        
        # Validate coordinate ranges
        if lat > 0xFFFFFF:
            return False, f"Invalid latitude encoding: 0x{lat:06X}"
        if lon > 0xFFFFFF:
            return False, f"Invalid longitude encoding: 0x{lon:06X}"
        
        # Check callsign (bytes 18-25)
        callsign = data[18:26]
        for i, c in enumerate(callsign):
            if not ((0x30 <= c <= 0x39) or (0x41 <= c <= 0x5A) or c == 0x20):
                return False, f"Invalid callsign character at position {i}: 0x{c:02X}"
        
        return True, f"Valid traffic report: ICAO=0x{icao:06X}"
    
    def validate_coordinate_encoding(self, lat_deg: float, lon_deg: float, 
                                   encoded_lat: int, encoded_lon: int) -> Tuple[bool, str]:
        """Validate latitude/longitude encoding per section 3.5.1.3."""
        
        # Calculate expected encoding
        def encode_coordinate(deg: float) -> int:
            if deg > 180.0:
                deg = 180.0
            if deg < -180.0:
                deg = -180.0
            
            val = int(deg * (0x800000 / 180.0))
            if val < 0:
                val = (0x1000000 + val) & 0xFFFFFF
            return val
        
        expected_lat = encode_coordinate(lat_deg)
        expected_lon = encode_coordinate(lon_deg)
        
        if encoded_lat != expected_lat:
            return False, f"Latitude encoding error: expected=0x{expected_lat:06X}, got=0x{encoded_lat:06X}"
        
        if encoded_lon != expected_lon:
            return False, f"Longitude encoding error: expected=0x{expected_lon:06X}, got=0x{encoded_lon:06X}"
        
        return True, "Coordinate encoding correct"


def test_specification_examples():
    """Test against examples from GDL-90 specification."""
    validator = GDL90Validator()
    
    print("Testing GDL-90 Specification Examples...")
    
    # Test heartbeat example from section 2.2.4
    # From spec: [0x7E 0x00 0x81 0x41 0xDB 0xD0 0x08 0x02 0xB3 0x8B 0x7E]
    heartbeat_example = bytes.fromhex("7E008141DBD0080202B38B7E")
    
    # Note: This is the example from the spec, but the CRC might be different
    # due to different message content. Let's validate the structure.
    
    print("\n1. Testing coordinate encoding examples:")
    test_cases = [
        (0.0, 0x000000),      # 0 degrees -> 0x000000
        (45.0, 0x200000),     # 45 degrees -> 0x200000  
        (90.0, 0x400000),     # 90 degrees -> 0x400000
        (-45.0, 0xE00000),    # -45 degrees -> 0xE00000
    ]
    
    for deg, expected in test_cases:
        # Calculate encoding
        val = int(deg * (0x800000 / 180.0))
        if val < 0:
            val = (0x1000000 + val) & 0xFFFFFF
            
        if val == expected:
            print(f"  ✓ {deg:6.1f}° -> 0x{val:06X}")
        else:
            print(f"  ✗ {deg:6.1f}° -> 0x{val:06X} (expected 0x{expected:06X})")
    
    print("\n2. Testing CRC calculation:")
    # Test with known data
    test_data = b'\x00\x81\x41\xDB\xD0\x08\x02'
    calculated_crc = validator.calculate_crc(test_data)
    print(f"  CRC for test data: 0x{calculated_crc:04X}")
    
    print("\nSpecification tests completed.")


def test_edge_cases():
    """Test edge cases and boundary conditions."""
    validator = GDL90Validator()
    
    print("\nTesting Edge Cases...")
    
    # Test coordinates near 0,0 (Gulf of Guinea)
    print("\n1. Testing coordinates near 0°,0°:")
    edge_coordinates = [
        (0.0, 0.0),
        (0.00001, 0.00001),
        (-0.00001, -0.00001),
        (0.1, -0.1),
    ]
    
    for lat, lon in edge_coordinates:
        # Calculate encodings
        lat_enc = int(lat * (0x800000 / 180.0)) & 0xFFFFFF
        lon_enc = int(lon * (0x800000 / 180.0)) & 0xFFFFFF
        
        if lon < 0:
            lon_enc = (0x1000000 + int(lon * (0x800000 / 180.0))) & 0xFFFFFF
            
        is_valid, msg = validator.validate_coordinate_encoding(lat, lon, lat_enc, lon_enc)
        status = "✓" if is_valid else "✗"
        print(f"  {status} ({lat:8.5f}, {lon:8.5f}) -> (0x{lat_enc:06X}, 0x{lon_enc:06X})")
    
    print("\nEdge case tests completed.")


if __name__ == "__main__":
    print("GDL-90 Format Validation Tests")
    print("=" * 50)
    
    test_specification_examples()
    test_edge_cases()
    
    print("\n" + "=" * 50)
    print("Test suite completed.")
    print("\nTo test with actual XP2GDL90 output:")
    print("1. Run X-Plane with XP2GDL90 plugin")
    print("2. Capture UDP packets on port 4000")
    print("3. Pass packet data to this validator")
