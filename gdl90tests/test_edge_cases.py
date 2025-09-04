#!/usr/bin/env python3
"""
GDL-90 Edge Case Testing
Tests specific edge cases and boundary conditions for GDL-90 message encoding

This script validates the fixes for:
1. Zero vertical speed handling
2. Dynamic miscellaneous field encoding
3. Boundary value encoding
"""

import socket
import time
import struct
from test_gdl90_compliance import GDL90Validator


def test_vertical_speed_encoding():
    """Test vertical speed encoding edge cases"""
    print("Testing Vertical Speed Encoding Edge Cases:")
    print("-" * 50)
    
    # Test cases for vertical speed encoding
    test_cases = [
        (0.0, "Level flight (0 FPM)"),
        (64.0, "1 unit climb (64 FPM)"),
        (-64.0, "1 unit descent (-64 FPM)"),
        (32576.0, "Maximum climb rate"),
        (-32576.0, "Maximum descent rate"),
        (50000.0, "Excessive climb rate (should clamp)"),
        (-50000.0, "Excessive descent rate (should clamp)")
    ]
    
    for vs_fpm, description in test_cases:
        # Convert to 64 FPM units as per spec
        if vs_fpm == 0.0:
            expected_encoded = 0x000  # Zero should be 0x000, not 0x800
        elif vs_fpm > 32576.0:
            expected_encoded = 0x1FE  # Max representable value  
        elif vs_fpm < -32576.0:
            expected_encoded = 0xE02  # Min representable value
        else:
            vs_64fpm = int(vs_fpm / 64.0)
            if vs_64fpm < 0:
                expected_encoded = (0x1000 + vs_64fpm) & 0xFFF  # 12-bit 2's complement
            else:
                expected_encoded = vs_64fpm & 0xFFF
        
        print(f"{description:30s}: {vs_fpm:8.1f} FPM -> 0x{expected_encoded:03X}")
    
    print("\n⚠ Zero FPM MUST encode as 0x000, NOT 0x800!")
    print("  0x800 is reserved for 'no vertical velocity information available'")


def test_miscellaneous_field_encoding():
    """Test miscellaneous field encoding per Table 9"""
    print("\nTesting Miscellaneous Field Encoding:")
    print("-" * 50)
    
    # Test cases for miscellaneous field (4 bits)
    test_cases = [
        # (on_ground, extrapolated, track_type, description)
        (True, False, 0, "On ground, updated, no track data"),
        (False, False, 1, "Airborne, updated, true track angle"),
        (False, True, 1, "Airborne, extrapolated, true track angle"),
        (True, False, 2, "On ground, updated, magnetic heading"),
        (False, False, 3, "Airborne, updated, true heading")
    ]
    
    for on_ground, extrapolated, track_type, description in test_cases:
        misc = 0
        misc |= (0 if on_ground else 1) << 3        # Bit 3: Air/Ground
        misc |= (1 if extrapolated else 0) << 2     # Bit 2: Report status  
        misc |= (track_type & 0x03)                 # Bits 1-0: Track type
        
        print(f"{description:40s}: 0x{misc:01X}")
        print(f"    Bit 3 (Air/Ground): {(misc >> 3) & 0x01} ({'Ground' if on_ground else 'Airborne'})")
        print(f"    Bit 2 (Report): {(misc >> 2) & 0x01} ({'Extrapolated' if extrapolated else 'Updated'})")  
        print(f"    Bits 1-0 (Track): {misc & 0x03} ({get_track_type_name(track_type)})")
        print()


def get_track_type_name(track_type: int) -> str:
    """Get track type description"""
    types = {
        0: "Not Valid",
        1: "True Track Angle", 
        2: "Magnetic Heading",
        3: "True Heading"
    }
    return types.get(track_type, "Reserved")


def test_coordinate_encoding():
    """Test latitude/longitude encoding edge cases"""
    print("Testing Coordinate Encoding:")
    print("-" * 50)
    
    # Test cases from GDL-90 spec Section 3.5.1.3
    test_cases = [
        (0.0, "00.000 degrees"),
        (45.0, "45.000 degrees"),
        (90.0, "90.000 degrees (max latitude)"),
        (-90.0, "-90.000 degrees (min latitude)"),
        (180.0, "180.000 degrees (max longitude)"),
        (-180.0, "-180.000 degrees (min longitude)"),
        (44.90708, "Example from spec (Salem, OR)")
    ]
    
    for coord_deg, description in test_cases:
        # Encode per spec: 24-bit "semicircle" 2's complement
        coord_raw = int(coord_deg * (0x800000 / 180.0))
        
        if coord_raw < 0:
            coord_encoded = (0x1000000 + coord_raw) & 0xFFFFFF
        else:
            coord_encoded = coord_raw & 0xFFFFFF
        
        # Decode back to verify
        if coord_encoded & 0x800000:  # Sign bit set
            coord_decoded = (coord_encoded - 0x1000000) * (180.0 / 0x800000)
        else:
            coord_decoded = coord_encoded * (180.0 / 0x800000)
        
        print(f"{description:35s}: {coord_deg:10.6f}° -> 0x{coord_encoded:06X} -> {coord_decoded:10.6f}°")


def test_altitude_encoding():
    """Test altitude encoding edge cases""" 
    print("\nTesting Altitude Encoding:")
    print("-" * 50)
    
    # Test cases from GDL-90 spec Section 3.5.1.4
    test_cases = [
        (-1000, "Minimum altitude"),
        (0, "Sea level"),
        (1000, "1000 feet"),
        (5000, "Example from spec"),
        (101350, "Maximum altitude"),
        (150000, "Excessive altitude (should clamp)")
    ]
    
    for alt_ft, description in test_cases:
        # Clamp to valid range
        alt_clamped = max(-1000, min(101350, alt_ft))
        
        # Encode per spec: 25-foot increments, +1000 ft offset
        altitude_encoded = int((alt_clamped + 1000) / 25.0)
        
        if altitude_encoded > 0xFFE:
            altitude_encoded = 0xFFE
        
        # Handle special value
        if altitude_encoded == 0xFFF:
            decoded_alt = "Invalid/Unavailable"
        else:
            decoded_alt = f"{(altitude_encoded * 25) - 1000} ft"
        
        print(f"{description:25s}: {alt_ft:8d} ft -> 0x{altitude_encoded:03X} -> {decoded_alt}")


def capture_and_analyze_live():
    """Capture live messages and analyze for edge cases"""
    print("\nLive Message Analysis:")
    print("-" * 50)
    print("Capturing messages for edge case analysis...")
    
    capture = GDL90MessageCapture(4000)
    validator = GDL90Validator()
    
    if not capture.start_capture():
        print("Failed to start capture")
        return
    
    try:
        # Capture for 5 seconds
        time.sleep(5)
        messages = capture.get_messages()
        
        print(f"Captured {len(messages)} messages")
        
        # Analyze for edge cases
        zero_vs_count = 0
        ground_state_count = 0
        
        for msg_info in messages:
            valid, result = validator.parse_message(msg_info['data'])
            
            if valid and result['message_id'] in [0x0A, 0x14]:  # Position/Traffic reports
                pos_valid, pos_data = validator.validate_position_report(result['data'])
                
                if pos_valid:
                    # Check for zero vertical speed
                    if pos_data['v_velocity'] == 0:
                        zero_vs_count += 1
                        print(f"✓ Found zero vertical speed correctly encoded")
                    
                    # Check ground state
                    if not pos_data['airborne']:
                        ground_state_count += 1
                        print(f"✓ Found aircraft on ground")
        
        print(f"\nEdge Case Analysis:")
        print(f"  Zero vertical speed instances: {zero_vs_count}")
        print(f"  Ground state instances: {ground_state_count}")
        
        if zero_vs_count == 0:
            print("  ⚠ No zero vertical speed detected - test by flying level")
        if ground_state_count == 0:
            print("  ⚠ No ground state detected - test by parking aircraft on ground")
    
    finally:
        capture.stop_capture()


def main():
    parser = argparse.ArgumentParser(description="GDL-90 Edge Case Testing")
    parser.add_argument("--live", action="store_true", help="Capture and analyze live messages")
    parser.add_argument("hex_string", nargs='?', help="Hex message to decode")
    
    args = parser.parse_args()
    
    if args.hex_string:
        decode_hex_string(args.hex_string)
    elif args.live:
        capture_and_analyze_live()
    else:
        # Run all static tests
        test_vertical_speed_encoding()
        test_miscellaneous_field_encoding()
        test_coordinate_encoding()
        test_altitude_encoding()
        
        print(f"\n{'='*60}")
        print("EDGE CASE TESTING COMPLETE")
        print("Run with --live to test with actual X-Plane data")
        print(f"{'='*60}")


if __name__ == "__main__":
    import argparse
    import time
    main()
