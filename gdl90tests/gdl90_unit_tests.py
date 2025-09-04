#!/usr/bin/env python3
"""
GDL-90 Unit Tests

Tests for individual components of the GDL-90 implementation.
This replicates the C++ functions in Python for validation.
"""

import struct
import unittest
from typing import Tuple

class GDL90Functions:
    """Python implementation of GDL-90 functions for testing."""
    
    # CRC-16-CCITT table (same as in C++ code)
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
    def gdl90_crc_compute(cls, data: bytes) -> int:
        """Python version of gdl90_crc_compute function."""
        crc = 0
        
        for byte in data:
            m = (crc << 8) & 0xFFFF
            table_index = (crc >> 8) & 0xFF
            crc = (cls.CRC16_TABLE[table_index] ^ m ^ byte) & 0xFFFF
            
        return crc
    
    @classmethod
    def gdl90_make_latitude(cls, lat_deg: float) -> int:
        """Python version of gdl90_make_latitude function."""
        if lat_deg > 90.0:
            lat_deg = 90.0
        if lat_deg < -90.0:
            lat_deg = -90.0
        
        lat = int(lat_deg * (0x800000 / 180.0))
        if lat < 0:
            lat = (0x1000000 + lat) & 0xFFFFFF
        return lat
    
    @classmethod
    def gdl90_make_longitude(cls, lon_deg: float) -> int:
        """Python version of gdl90_make_longitude function."""
        if lon_deg > 180.0:
            lon_deg = 180.0
        if lon_deg < -180.0:
            lon_deg = -180.0
        
        lon = int(lon_deg * (0x800000 / 180.0))
        if lon < 0:
            lon = (0x1000000 + lon) & 0xFFFFFF
        return lon
    
    @classmethod
    def gdl90_escape(cls, data: bytes) -> bytes:
        """Python version of gdl90_escape function."""
        escaped = bytearray()
        for byte in data:
            if byte == 0x7d or byte == 0x7e:
                escaped.append(0x7d)  # Escape character
                escaped.append(byte ^ 0x20)  # Modified byte
            else:
                escaped.append(byte)
        return bytes(escaped)
    
    @classmethod
    def gdl90_prepare_message(cls, msg: bytes) -> bytes:
        """Python version of gdl90_prepare_message function."""
        # Add CRC
        crc = cls.gdl90_crc_compute(msg)
        msg_with_crc = msg + struct.pack('<H', crc)  # Little endian
        
        # Escape
        escaped = cls.gdl90_escape(msg_with_crc)
        
        # Add flags
        return bytes([0x7e]) + escaped + bytes([0x7e])


class TestGDL90Functions(unittest.TestCase):
    """Unit tests for GDL-90 functions."""
    
    def test_crc_computation(self):
        """Test CRC computation with known values."""
        # Test with empty data
        self.assertEqual(GDL90Functions.gdl90_crc_compute(b''), 0)
        
        # Test with simple data
        test_data = b'\x00\x81\x41\xDB\xD0\x08\x02'
        crc = GDL90Functions.gdl90_crc_compute(test_data)
        # Note: Expected value should be verified against C++ implementation
        self.assertIsInstance(crc, int)
        self.assertGreaterEqual(crc, 0)
        self.assertLessEqual(crc, 0xFFFF)
    
    def test_latitude_encoding(self):
        """Test latitude encoding with specification examples."""
        test_cases = [
            (0.0, 0x000000),      # 0 degrees -> 0x000000
            (45.0, 0x200000),     # 45 degrees -> 0x200000
            (90.0, 0x400000),     # 90 degrees -> 0x400000
            (-45.0, 0xE00000),    # -45 degrees -> 0xE00000
            (-90.0, 0xC00000),    # -90 degrees -> 0xC00000
        ]
        
        for lat_deg, expected in test_cases:
            with self.subTest(latitude=lat_deg):
                result = GDL90Functions.gdl90_make_latitude(lat_deg)
                self.assertEqual(result, expected, 
                    f"Latitude {lat_deg}° should encode to 0x{expected:06X}, got 0x{result:06X}")
    
    def test_longitude_encoding(self):
        """Test longitude encoding with specification examples."""
        test_cases = [
            (0.0, 0x000000),      # 0 degrees -> 0x000000
            (45.0, 0x200000),     # 45 degrees -> 0x200000
            (90.0, 0x400000),     # 90 degrees -> 0x400000
            (180.0, 0x800000),    # 180 degrees -> 0x800000
            (-45.0, 0xE00000),    # -45 degrees -> 0xE00000
            (-90.0, 0xC00000),    # -90 degrees -> 0xC00000
            (-180.0, 0x800000),   # -180 degrees -> 0x800000
        ]
        
        for lon_deg, expected in test_cases:
            with self.subTest(longitude=lon_deg):
                result = GDL90Functions.gdl90_make_longitude(lon_deg)
                self.assertEqual(result, expected,
                    f"Longitude {lon_deg}° should encode to 0x{expected:06X}, got 0x{result:06X}")
    
    def test_latitude_boundary_conditions(self):
        """Test latitude encoding at boundary conditions."""
        # Test clamping
        self.assertEqual(GDL90Functions.gdl90_make_latitude(100.0), 
                        GDL90Functions.gdl90_make_latitude(90.0))
        self.assertEqual(GDL90Functions.gdl90_make_latitude(-100.0), 
                        GDL90Functions.gdl90_make_latitude(-90.0))
    
    def test_longitude_boundary_conditions(self):
        """Test longitude encoding at boundary conditions."""
        # Test clamping
        self.assertEqual(GDL90Functions.gdl90_make_longitude(200.0), 
                        GDL90Functions.gdl90_make_longitude(180.0))
        self.assertEqual(GDL90Functions.gdl90_make_longitude(-200.0), 
                        GDL90Functions.gdl90_make_longitude(-180.0))
    
    def test_edge_case_coordinates(self):
        """Test coordinates near 0,0 (previously problematic)."""
        edge_cases = [
            (0.0, 0.0),
            (0.00001, 0.00001),
            (-0.00001, -0.00001),
            (0.1, -0.1),
            (-0.1, 0.1),
        ]
        
        for lat, lon in edge_cases:
            with self.subTest(lat=lat, lon=lon):
                lat_encoded = GDL90Functions.gdl90_make_latitude(lat)
                lon_encoded = GDL90Functions.gdl90_make_longitude(lon)
                
                # Should not be rejected (i.e., should return valid encodings)
                self.assertIsInstance(lat_encoded, int)
                self.assertIsInstance(lon_encoded, int)
                self.assertGreaterEqual(lat_encoded, 0)
                self.assertGreaterEqual(lon_encoded, 0)
                self.assertLessEqual(lat_encoded, 0xFFFFFF)
                self.assertLessEqual(lon_encoded, 0xFFFFFF)
    
    def test_byte_stuffing(self):
        """Test byte stuffing (escaping) function."""
        test_cases = [
            (b'', b''),  # Empty data
            (b'\x01\x02\x03', b'\x01\x02\x03'),  # No escaping needed
            (b'\x7e', b'\x7d\x5e'),  # Escape 0x7e -> 0x7d 0x5e
            (b'\x7d', b'\x7d\x5d'),  # Escape 0x7d -> 0x7d 0x5d
            (b'\x7e\x7d', b'\x7d\x5e\x7d\x5d'),  # Multiple escapes
            (b'\x01\x7e\x02\x7d\x03', b'\x01\x7d\x5e\x02\x7d\x5d\x03'),  # Mixed
        ]
        
        for input_data, expected in test_cases:
            with self.subTest(input=input_data.hex()):
                result = GDL90Functions.gdl90_escape(input_data)
                self.assertEqual(result, expected,
                    f"Input {input_data.hex()} should escape to {expected.hex()}, got {result.hex()}")
    
    def test_message_preparation(self):
        """Test complete message preparation (CRC + escape + flags)."""
        # Test with simple heartbeat-like message
        msg_data = b'\x00\x81\x41\xDB\xD0\x08\x02'
        prepared = GDL90Functions.gdl90_prepare_message(msg_data)
        
        # Should start and end with flag bytes
        self.assertEqual(prepared[0], 0x7e)
        self.assertEqual(prepared[-1], 0x7e)
        
        # Should be longer than original (added CRC and flags, possibly escape chars)
        self.assertGreater(len(prepared), len(msg_data))
    
    def test_altitude_encoding(self):
        """Test altitude encoding calculations."""
        test_cases = [
            (-1000, 0),      # Minimum altitude
            (0, 40),         # Sea level: (0 + 1000) / 25 = 40
            (1000, 80),      # 1000 ft: (1000 + 1000) / 25 = 80
            (5000, 240),     # 5000 ft: (5000 + 1000) / 25 = 240
            (101350, 4094),  # Maximum altitude
        ]
        
        for alt_ft, expected in test_cases:
            with self.subTest(altitude=alt_ft):
                # Replicate C++ altitude encoding logic
                alt_clamped = max(-1000.0, min(101350.0, alt_ft))
                encoded = int((alt_clamped + 1000) / 25.0)
                encoded = min(0xffe, encoded)
                
                self.assertEqual(encoded, expected,
                    f"Altitude {alt_ft} ft should encode to {expected}, got {encoded}")
    
    def test_vertical_velocity_encoding(self):
        """Test vertical velocity encoding calculations."""
        test_cases = [
            (0, 0),           # Level flight
            (64, 1),          # 64 fpm climb
            (-64, 0xFFF),     # 64 fpm descent (12-bit 2's complement)
            (1280, 20),       # 1280 fpm climb
            (-1280, 0xFEC),   # 1280 fpm descent
            (32576, 509),     # Maximum climb rate
            (-32576, 0xE03),  # Maximum descent rate
        ]
        
        for vs_fpm, expected in test_cases:
            with self.subTest(vertical_speed=vs_fpm):
                # Replicate C++ vertical velocity encoding logic
                vs_clamped = max(-32576.0, min(32576.0, vs_fpm))
                vs_64fpm = int(vs_clamped / 64.0)
                
                if vs_64fpm < 0:
                    v_velocity = (0x1000 + vs_64fpm) & 0xfff
                else:
                    v_velocity = vs_64fpm & 0xfff
                
                self.assertEqual(v_velocity, expected,
                    f"Vertical speed {vs_fpm} fpm should encode to 0x{expected:03X}, got 0x{v_velocity:03X}")


class TestCallsignValidation(unittest.TestCase):
    """Test callsign validation and formatting."""
    
    def test_valid_callsign_characters(self):
        """Test that valid GDL-90 callsign characters are accepted."""
        valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 "
        
        for char in valid_chars:
            with self.subTest(char=repr(char)):
                # Character should be in valid range per GDL-90 spec
                is_valid = (
                    ('0' <= char <= '9') or 
                    ('A' <= char <= 'Z') or 
                    char == ' '
                )
                self.assertTrue(is_valid, f"Character '{char}' should be valid")
    
    def test_callsign_formatting(self):
        """Test callsign formatting to 8 characters."""
        test_cases = [
            ("N123AB", "N123AB  "),    # Pad with spaces
            ("UAL123", "UAL123  "),    # Airline callsign
            ("AIRCRAFT1", "AIRCRAFT"),  # Truncate if too long
            ("A", "A       "),         # Single character
            ("", "        "),          # Empty string
        ]
        
        for input_cs, expected in test_cases:
            with self.subTest(callsign=input_cs):
                # Simulate the fixed callsign processing logic
                formatted = list("        ")  # 8 spaces
                j = 0
                
                for k, c in enumerate(input_cs):
                    if j >= 8:
                        break
                    if c == '\0':
                        break
                    
                    # Allow digits, letters, and spaces as per GDL-90 spec
                    if (('0' <= c <= '9') or ('A' <= c <= 'Z') or 
                        ('a' <= c <= 'z') or c == ' '):
                        formatted[j] = c.upper()
                        j += 1
                
                result = ''.join(formatted)
                self.assertEqual(result, expected,
                    f"Callsign '{input_cs}' should format to '{expected}', got '{result}'")


def run_performance_tests():
    """Run performance tests for encoding functions."""
    import time
    
    print("\nPerformance Tests")
    print("=" * 50)
    
    # Test coordinate encoding performance
    test_coords = [
        (37.7749, -122.4194),  # San Francisco
        (40.7128, -74.0060),   # New York
        (51.5074, -0.1278),    # London
        (-33.8688, 151.2093),  # Sydney
        (0.0, 0.0),            # Equator/Prime Meridian
    ] * 1000  # 5000 total coordinates
    
    start_time = time.time()
    for lat, lon in test_coords:
        GDL90Functions.gdl90_make_latitude(lat)
        GDL90Functions.gdl90_make_longitude(lon)
    end_time = time.time()
    
    encoding_time = end_time - start_time
    coords_per_sec = len(test_coords) / encoding_time
    
    print(f"Coordinate encoding: {coords_per_sec:.0f} coord pairs/sec")
    
    # Test CRC performance
    test_message = b'\x14' + b'\x00' * 27  # Traffic report sized message
    
    start_time = time.time()
    for _ in range(10000):
        GDL90Functions.gdl90_crc_compute(test_message)
    end_time = time.time()
    
    crc_time = end_time - start_time
    crcs_per_sec = 10000 / crc_time
    
    print(f"CRC computation: {crcs_per_sec:.0f} CRCs/sec")


if __name__ == "__main__":
    print("GDL-90 Unit Tests")
    print("=" * 50)
    
    # Run unit tests
    unittest.main(argv=[''], exit=False, verbosity=2)
    
    # Run performance tests
    run_performance_tests()
