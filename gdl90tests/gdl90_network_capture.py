#!/usr/bin/env python3
"""
GDL-90 Network Capture and Validation Tool

This script captures GDL-90 messages from XP2GDL90 plugin via UDP
and validates them against the GDL-90 specification.

Usage:
    python gdl90_network_capture.py [--port 4000] [--ip 127.0.0.1] [--count 100]
"""

import socket
import argparse
import time
import struct
from typing import List, Dict, Any
from gdl90_format_tests import GDL90Validator

class GDL90NetworkCapture:
    """Captures and analyzes GDL-90 messages from network."""
    
    def __init__(self, ip: str = "127.0.0.1", port: int = 4000):
        self.ip = ip
        self.port = port
        self.validator = GDL90Validator()
        self.stats = {
            'total_packets': 0,
            'valid_messages': 0,
            'invalid_messages': 0,
            'heartbeats': 0,
            'ownship_reports': 0,
            'traffic_reports': 0,
            'unknown_messages': 0,
            'crc_errors': 0,
            'format_errors': 0
        }
        
    def setup_socket(self) -> socket.socket:
        """Setup UDP socket for receiving GDL-90 messages."""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        try:
            sock.bind((self.ip, self.port))
            print(f"Listening for GDL-90 messages on {self.ip}:{self.port}")
            return sock
        except Exception as e:
            print(f"Failed to bind to {self.ip}:{self.port}: {e}")
            raise
    
    def decode_heartbeat(self, data: bytes) -> Dict[str, Any]:
        """Decode heartbeat message details."""
        if len(data) != 6:
            return {"error": "Invalid heartbeat length"}
        
        status1, status2, ts_low, ts_high, count1, count2 = struct.unpack('BBBBBB', data)
        
        # Parse status bits per Table 3
        gps_valid = bool(status1 & 0x80)
        maint_required = bool(status1 & 0x40) 
        ident = bool(status1 & 0x20)
        addr_type = bool(status1 & 0x10)
        gps_batt_low = bool(status1 & 0x08)
        ratcs = bool(status1 & 0x04)
        uat_init = bool(status1 & 0x01)
        
        ts_bit16 = bool(status2 & 0x80)
        csa_requested = bool(status2 & 0x40)
        csa_not_available = bool(status2 & 0x20)
        utc_ok = bool(status2 & 0x01)
        
        # Calculate timestamp
        timestamp = (int(ts_bit16) << 16) | (ts_high << 8) | ts_low
        
        # Parse message counts
        uplink_count = (count1 >> 3) & 0x1F
        basic_long_count = ((count1 & 0x03) << 8) | count2
        
        return {
            "timestamp": timestamp,
            "gps_valid": gps_valid,
            "maintenance_required": maint_required,
            "ident": ident,
            "address_type": addr_type,
            "gps_battery_low": gps_batt_low,
            "ratcs": ratcs,
            "uat_initialized": uat_init,
            "csa_requested": csa_requested,
            "csa_not_available": csa_not_available,
            "utc_ok": utc_ok,
            "uplink_count": uplink_count,
            "basic_long_count": basic_long_count
        }
    
    def decode_position_report(self, data: bytes, msg_type: str) -> Dict[str, Any]:
        """Decode ownship or traffic position report."""
        if len(data) != 27:
            return {"error": f"Invalid {msg_type} length: {len(data)}"}
        
        # Parse fields per Table 8
        status_addr = data[0]
        alert_status = (status_addr >> 4) & 0x0F
        addr_type = status_addr & 0x0F
        
        icao = struct.unpack('>I', b'\x00' + data[1:4])[0]
        lat_raw = struct.unpack('>I', b'\x00' + data[4:7])[0]
        lon_raw = struct.unpack('>I', b'\x00' + data[7:10])[0]
        
        # Convert coordinates to degrees
        def decode_coord(raw_val):
            if raw_val & 0x800000:  # Negative (2's complement)
                return (raw_val - 0x1000000) * (180.0 / 0x800000)
            else:
                return raw_val * (180.0 / 0x800000)
        
        latitude = decode_coord(lat_raw)
        longitude = decode_coord(lon_raw)
        
        # Parse altitude and misc
        alt_misc = struct.unpack('>H', data[10:12])[0]
        altitude_encoded = (alt_misc >> 4) & 0xFFF
        misc = alt_misc & 0x0F
        
        altitude_ft = (altitude_encoded * 25) - 1000 if altitude_encoded != 0xFFF else None
        
        # Parse misc field
        airborne = bool(misc & 0x08)
        report_updated = not bool(misc & 0x04)
        track_type = misc & 0x03
        
        # Parse NIC/NACp
        nic_nacp = data[12]
        nic = (nic_nacp >> 4) & 0x0F
        nacp = nic_nacp & 0x0F
        
        # Parse velocities
        vel_data = struct.unpack('>I', b'\x00' + data[13:16])[0]
        h_velocity = (vel_data >> 12) & 0xFFF
        v_velocity_raw = vel_data & 0xFFF
        
        # Convert vertical velocity (12-bit signed)
        if v_velocity_raw & 0x800:  # Negative
            v_velocity = (v_velocity_raw - 0x1000) * 64
        else:
            v_velocity = v_velocity_raw * 64
            
        if h_velocity == 0xFFF:
            h_velocity = None
        if v_velocity_raw == 0x800:
            v_velocity = None
        
        # Track/heading
        track = data[16] * (360.0 / 256)
        
        # Emitter category
        emitter_cat = data[17]
        
        # Callsign
        callsign = data[18:26].decode('ascii', errors='replace').rstrip()
        
        # Emergency code
        emergency = (data[26] >> 4) & 0x0F
        
        return {
            "alert_status": alert_status,
            "address_type": addr_type,
            "icao_address": f"0x{icao:06X}",
            "latitude": latitude,
            "longitude": longitude,
            "altitude_ft": altitude_ft,
            "airborne": airborne,
            "report_updated": report_updated,
            "track_type": track_type,
            "nic": nic,
            "nacp": nacp,
            "horizontal_velocity_kt": h_velocity,
            "vertical_velocity_fpm": v_velocity,
            "track_deg": track,
            "emitter_category": emitter_cat,
            "callsign": callsign,
            "emergency_code": emergency
        }
    
    def analyze_message(self, raw_data: bytes) -> Dict[str, Any]:
        """Analyze a single GDL-90 message."""
        self.stats['total_packets'] += 1
        
        # Validate message structure
        is_valid, error_msg, parsed = self.validator.validate_message_structure(raw_data)
        
        if not is_valid:
            self.stats['invalid_messages'] += 1
            if "CRC" in error_msg:
                self.stats['crc_errors'] += 1
            else:
                self.stats['format_errors'] += 1
            return {
                "valid": False,
                "error": error_msg,
                "raw_length": len(raw_data)
            }
        
        self.stats['valid_messages'] += 1
        message_id = parsed['message_id']
        message_data = parsed['data']
        
        result = {
            "valid": True,
            "message_id": f"0x{message_id:02X}",
            "data_length": len(message_data),
            "total_length": parsed['total_length'],
            "crc": f"0x{parsed['crc']:04X}"
        }
        
        # Decode specific message types
        if message_id == self.validator.MSG_HEARTBEAT:
            self.stats['heartbeats'] += 1
            result["type"] = "Heartbeat"
            result["details"] = self.decode_heartbeat(message_data)
            
        elif message_id == self.validator.MSG_OWNSHIP_REPORT:
            self.stats['ownship_reports'] += 1
            result["type"] = "Ownship Report"
            result["details"] = self.decode_position_report(message_data, "ownship")
            
        elif message_id == self.validator.MSG_TRAFFIC_REPORT:
            self.stats['traffic_reports'] += 1
            result["type"] = "Traffic Report"
            result["details"] = self.decode_position_report(message_data, "traffic")
            
        else:
            self.stats['unknown_messages'] += 1
            result["type"] = f"Unknown (0x{message_id:02X})"
            result["details"] = {"raw_data": message_data.hex()}
        
        return result
    
    def capture_messages(self, count: int = 100, timeout: float = 30.0):
        """Capture and analyze GDL-90 messages."""
        sock = self.setup_socket()
        sock.settimeout(1.0)  # 1 second timeout for each receive
        
        start_time = time.time()
        messages_captured = 0
        
        print(f"Capturing up to {count} messages (timeout: {timeout}s)...\n")
        
        try:
            while messages_captured < count and (time.time() - start_time) < timeout:
                try:
                    data, addr = sock.recvfrom(1024)
                    
                    print(f"\n--- Message {messages_captured + 1} from {addr} ---")
                    print(f"Raw ({len(data)} bytes): {data.hex()}")
                    
                    analysis = self.analyze_message(data)
                    
                    if analysis['valid']:
                        print(f"✓ {analysis['type']} - ID: {analysis['message_id']}")
                        
                        details = analysis.get('details', {})
                        if analysis['type'] == 'Heartbeat':
                            print(f"  Timestamp: {details.get('timestamp')}s")
                            print(f"  GPS Valid: {details.get('gps_valid')}")
                            print(f"  UTC OK: {details.get('utc_ok')}")
                            
                        elif analysis['type'] in ['Ownship Report', 'Traffic Report']:
                            print(f"  ICAO: {details.get('icao_address')}")
                            print(f"  Position: ({details.get('latitude'):.6f}, {details.get('longitude'):.6f})")
                            print(f"  Altitude: {details.get('altitude_ft')} ft")
                            print(f"  Callsign: '{details.get('callsign')}'")
                            print(f"  Airborne: {details.get('airborne')}")
                            
                    else:
                        print(f"✗ Invalid: {analysis['error']}")
                    
                    messages_captured += 1
                    
                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"Error receiving message: {e}")
                    
        except KeyboardInterrupt:
            print("\nCapture interrupted by user")
        finally:
            sock.close()
        
        self.print_statistics()
    
    def print_statistics(self):
        """Print capture statistics."""
        print("\n" + "=" * 50)
        print("CAPTURE STATISTICS")
        print("=" * 50)
        print(f"Total packets received: {self.stats['total_packets']}")
        print(f"Valid messages: {self.stats['valid_messages']}")
        print(f"Invalid messages: {self.stats['invalid_messages']}")
        print(f"  - CRC errors: {self.stats['crc_errors']}")
        print(f"  - Format errors: {self.stats['format_errors']}")
        print()
        print("Message type breakdown:")
        print(f"  - Heartbeats: {self.stats['heartbeats']}")
        print(f"  - Ownship reports: {self.stats['ownship_reports']}")
        print(f"  - Traffic reports: {self.stats['traffic_reports']}")
        print(f"  - Unknown messages: {self.stats['unknown_messages']}")
        
        if self.stats['total_packets'] > 0:
            success_rate = (self.stats['valid_messages'] / self.stats['total_packets']) * 100
            print(f"\nSuccess rate: {success_rate:.1f}%")


def main():
    parser = argparse.ArgumentParser(description="Capture and validate GDL-90 messages")
    parser.add_argument("--ip", default="127.0.0.1", help="IP address to listen on")
    parser.add_argument("--port", type=int, default=4000, help="UDP port to listen on")
    parser.add_argument("--count", type=int, default=100, help="Number of messages to capture")
    parser.add_argument("--timeout", type=float, default=30.0, help="Capture timeout in seconds")
    
    args = parser.parse_args()
    
    print("GDL-90 Network Capture Tool")
    print("=" * 50)
    print("Make sure XP2GDL90 plugin is running and broadcasting to the specified address.")
    print(f"Listening on {args.ip}:{args.port}")
    print("Press Ctrl+C to stop capture early.\n")
    
    capture = GDL90NetworkCapture(args.ip, args.port)
    capture.capture_messages(args.count, args.timeout)


if __name__ == "__main__":
    main()
