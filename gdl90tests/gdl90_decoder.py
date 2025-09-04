#!/usr/bin/env python3
"""
GDL-90 Message Decoder
Standalone utility to decode GDL-90 messages for debugging and validation

Usage:
    python gdl90_decoder.py "7E 00 81 41 DB D0 08 02 B3 8B 7E"  # Decode hex string
    python gdl90_decoder.py --capture                             # Live capture mode
"""

import sys
import argparse
from test_gdl90_compliance import GDL90Validator, GDL90MessageCapture


def decode_hex_string(hex_string: str):
    """Decode a GDL-90 message from hex string"""
    try:
        # Remove spaces and convert to bytes
        hex_clean = hex_string.replace(' ', '').replace(',', '')
        message_bytes = bytes.fromhex(hex_clean)
        
        print(f"Raw message: {message_bytes.hex().upper()}")
        print(f"Length: {len(message_bytes)} bytes")
        
        validator = GDL90Validator()
        valid, result = validator.parse_message(message_bytes)
        
        if not valid:
            print(f"✗ Invalid message: {result.get('error')}")
            return
        
        msg_id = result['message_id']
        data = result['data']
        
        print(f"✓ Valid GDL-90 message")
        print(f"Message ID: 0x{msg_id:02X} ({get_message_name(msg_id)})")
        print(f"Data length: {len(data)} bytes")
        print(f"CRC: Valid")
        
        # Decode specific message types
        if msg_id == 0x00:  # Heartbeat
            hb_valid, hb_result = validator.validate_heartbeat(data)
            if hb_valid:
                print("\nHeartbeat Details:")
                print(f"  GPS Valid: {hb_result['gps_valid']}")
                print(f"  UAT Initialized: {hb_result['uat_initialized']}")
                print(f"  UTC OK: {hb_result['utc_ok']}")
                print(f"  Timestamp: {hb_result['timestamp']} seconds since midnight")
                print(f"  Status Byte 1: {hb_result['status1']}")
                print(f"  Status Byte 2: {hb_result['status2']}")
        
        elif msg_id in [0x0A, 0x14]:  # Position/Traffic Report
            pos_valid, pos_result = validator.validate_position_report(data)
            if pos_valid:
                report_type = "Ownship Report" if msg_id == 0x0A else "Traffic Report"
                print(f"\n{report_type} Details:")
                print(f"  ICAO Address: {pos_result['icao_address']}")
                print(f"  Callsign: '{pos_result['callsign']}'")
                print(f"  Position: {pos_result['latitude']:.6f}°, {pos_result['longitude']:.6f}°")
                print(f"  Altitude: {pos_result['altitude']} ft")
                print(f"  Aircraft State: {'Airborne' if pos_result['airborne'] else 'On Ground'}")
                print(f"  Horizontal Speed: {pos_result['h_velocity']} knots" if pos_result['h_velocity'] is not None else "  Horizontal Speed: No data")
                print(f"  Vertical Speed: {pos_result['v_velocity']} fpm" if pos_result['v_velocity'] is not None else "  Vertical Speed: No data")
                print(f"  Track/Heading: {pos_result['track']:.1f}°")
                print(f"  Emitter Category: {pos_result['emitter_category']} ({get_emitter_category_name(pos_result['emitter_category'])})")
                print(f"  NIC/NACp: {pos_result['nic']}/{pos_result['nacp']}")
            else:
                print(f"✗ {report_type} decode failed: {pos_result.get('error')}")
        
        else:
            print(f"\nMessage type 0x{msg_id:02X} - Raw data:")
            print(f"  {data.hex().upper()}")
    
    except ValueError as e:
        print(f"✗ Invalid hex string: {e}")
    except Exception as e:
        print(f"✗ Decode error: {e}")


def get_message_name(msg_id: int) -> str:
    """Get message name from ID per GDL-90 spec Table 2"""
    message_names = {
        0x00: "Heartbeat",
        0x02: "Initialization", 
        0x07: "Uplink Data",
        0x09: "Height Above Terrain",
        0x0A: "Ownship Report",
        0x0B: "Ownship Geometric Altitude",
        0x14: "Traffic Report",
        0x1E: "Basic Report",
        0x1F: "Long Report"
    }
    return message_names.get(msg_id, "Unknown")


def get_emitter_category_name(category: int) -> str:
    """Get emitter category name from GDL-90 spec Table 11"""
    categories = {
        0: "No aircraft type information",
        1: "Light (< 15,500 lbs)",
        2: "Small (15,500 to 75,000 lbs)",
        3: "Large (75,000 to 300,000 lbs)",
        4: "High Vortex Large",
        5: "Heavy (> 300,000 lbs)",
        6: "Highly Maneuverable",
        7: "Rotorcraft",
        9: "Glider/sailplane", 
        10: "Lighter than air",
        11: "Parachutist/sky diver",
        12: "Ultra light/hang glider/paraglider",
        14: "Unmanned aerial vehicle",
        15: "Space/transatmospheric vehicle",
        17: "Surface vehicle — emergency",
        18: "Surface vehicle — service",
        19: "Point Obstacle",
        20: "Cluster Obstacle",
        21: "Line Obstacle"
    }
    return categories.get(category, f"Reserved ({category})")


def live_capture_mode():
    """Run live capture and decode mode"""
    print("Starting live GDL-90 message capture and decode...")
    print("Press Ctrl+C to stop")
    
    capture = GDL90MessageCapture(4000)
    validator = GDL90Validator()
    
    if not capture.start_capture():
        print("Failed to start capture")
        return
    
    try:
        last_count = 0
        while True:
            messages = capture.get_messages()
            
            # Process new messages
            for msg_info in messages[last_count:]:
                print(f"\n[{time.strftime('%H:%M:%S')}] New message from {msg_info['source']}")
                
                valid, result = validator.parse_message(msg_info['data'])
                if valid:
                    msg_id = result['message_id']
                    print(f"Type: 0x{msg_id:02X} ({get_message_name(msg_id)})")
                    
                    if msg_id == 0x0A:  # Ownship Report
                        pos_valid, pos_data = validator.validate_position_report(result['data'])
                        if pos_valid:
                            print(f"Position: {pos_data['latitude']:.6f}°, {pos_data['longitude']:.6f}°")
                            print(f"Altitude: {pos_data['altitude']} ft")
                            print(f"Speed: {pos_data['h_velocity']} kt, VS: {pos_data['v_velocity']} fpm")
                else:
                    print(f"Invalid: {result.get('error')}")
            
            last_count = len(messages)
            time.sleep(0.5)
            
    except KeyboardInterrupt:
        print("\nStopping capture...")
    finally:
        capture.stop_capture()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="GDL-90 Message Decoder")
    parser.add_argument("hex_string", nargs='?', help="Hex string to decode (e.g. '7E 00 81...')")
    parser.add_argument("--capture", action="store_true", help="Live capture and decode mode")
    
    args = parser.parse_args()
    
    if args.capture:
        live_capture_mode()
    elif args.hex_string:
        decode_hex_string(args.hex_string)
    else:
        print("Usage:")
        print("  Decode hex string: python gdl90_decoder.py '7E 00 81 41 DB D0 08 02 B3 8B 7E'")
        print("  Live capture mode: python gdl90_decoder.py --capture")
