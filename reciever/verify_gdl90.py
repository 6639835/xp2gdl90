#!/usr/bin/env python3
"""
GDL-90 Message Verification Tool
Quick verification of individual GDL-90 messages for testing and debugging
"""

import sys
import json
from gdl90_decoder import GDL90Decoder

def verify_message(hex_data: str, expected_values: dict = None):
    """Verify a single GDL-90 message"""
    
    print("=== GDL-90 Message Verification ===\n")
    
    try:
        # Convert hex string to bytes
        data = bytes.fromhex(hex_data.replace(" ", "").replace("0x", ""))
        
        # Decode the message
        decoder = GDL90Decoder()
        result = decoder.decode_frame(data)
        
        print(f"Input (hex): {hex_data}")
        print(f"Input (bytes): {data.hex().upper()}")
        print(f"Length: {len(data)} bytes\n")
        
        # Display decode results
        print("=== Decode Results ===")
        print(json.dumps(result, indent=2))
        
        # Verify against expected values if provided
        if expected_values and result.get("crc_valid"):
            print("\n=== Verification ===")
            verify_against_expected(result, expected_values)
            
        return result
        
    except Exception as e:
        print(f"Error: {e}")
        return None

def verify_against_expected(decoded: dict, expected: dict):
    """Compare decoded values against expected values"""
    
    passed = 0
    failed = 0
    
    for field, expected_value in expected.items():
        if field in decoded:
            actual_value = decoded[field]
            
            # Handle different data types
            if isinstance(expected_value, float):
                # For floating point, check within tolerance
                tolerance = 0.000001
                match = abs(actual_value - expected_value) < tolerance
            else:
                match = actual_value == expected_value
                
            status = "✓ PASS" if match else "✗ FAIL"
            print(f"{field}: {status}")
            print(f"  Expected: {expected_value}")
            print(f"  Actual:   {actual_value}")
            
            if match:
                passed += 1
            else:
                failed += 1
        else:
            print(f"{field}: ✗ FAIL (field not found)")
            failed += 1
            
    print(f"\nVerification Summary: {passed} passed, {failed} failed")

def create_test_message():
    """Create a test GDL-90 message for verification"""
    
    # This is a sample heartbeat message 
    # Message ID 0x00, followed by status bytes and timestamp, CRC, and frame flags
    
    sample_message = "7E 00 81 01 00 00 00 00 BC 9C 7E"
    
    print("Sample GDL-90 Heartbeat Message:")
    print(f"Hex: {sample_message}")
    print("\nExpected values:")
    print("- Message Type: Heartbeat (0x00)")
    print("- Status Byte 1: 0x81") 
    print("- Status Byte 2: 0x01")
    print("- Timestamp: 0 seconds")
    print("- Message Count: 0")
    print("- CRC: 0x9CBC")
    
    return sample_message

def main():
    if len(sys.argv) < 2:
        print("GDL-90 Message Verification Tool")
        print("\nUsage:")
        print("  python3 verify_gdl90.py <hex_data>")
        print("  python3 verify_gdl90.py --sample")
        print("\nExamples:")
        print("  python3 verify_gdl90.py '7E 00 81 01 00 00 00 00 8C 73 7E'")
        print("  python3 verify_gdl90.py --sample")
        sys.exit(1)
        
    if sys.argv[1] == "--sample":
        # Test with sample message
        sample_hex = create_test_message()
        expected = {
            "message_type": "Heartbeat",
            "crc_valid": True,
            "status_byte_1": "0x81",
            "timestamp": 0
        }
        verify_message(sample_hex, expected)
    else:
        # Verify user-provided message
        hex_data = sys.argv[1]
        
        # If expected values provided as JSON
        expected = None
        if len(sys.argv) > 2:
            try:
                expected = json.loads(sys.argv[2])
            except:
                print("Warning: Could not parse expected values JSON")
                
        verify_message(hex_data, expected)

if __name__ == "__main__":
    main()
