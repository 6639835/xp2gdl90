#!/usr/bin/env python3
"""
Real-time GDL-90 UDP Capture and Decoder
Receives UDP data from XP2GDL90 plugin, decodes it in real-time, and saves decoded data.
Combines capture + decode + save functionality in one efficient script.
"""

import socket
import time
import sys
import json
import struct
from datetime import datetime
from typing import Dict, List, Any, Optional

# GDL-90 CRC lookup table
GDL90_CRC16_TABLE = [
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

class RealtimeGDL90Capture:
    def __init__(self, ip="127.0.0.1", port=4000, output_file="realtime_capture.json"):
        self.ip = ip
        self.port = port
        self.output_file = output_file
        self.socket = None
        self.running = False
        self.decoded_packets = []
        self.statistics = {
            "total_packets": 0,
            "valid_packets": 0,
            "crc_errors": 0,
            "decode_errors": 0,
            "message_types": {}
        }
        
    def compute_crc(self, data: bytes) -> int:
        """Compute GDL-90 CRC16"""
        crc = 0
        for byte in data:
            m = (crc << 8) & 0xFFFF
            crc = GDL90_CRC16_TABLE[(crc >> 8)] ^ m ^ byte
        return crc & 0xFFFF
        
    def unescape_data(self, data: bytes) -> bytes:
        """Remove GDL-90 escape sequences"""
        if len(data) < 2 or data[0] != 0x7E or data[-1] != 0x7E:
            return data
            
        # Remove frame flags
        payload = data[1:-1]
        unescaped = bytearray()
        
        i = 0
        while i < len(payload):
            if payload[i] == 0x7D and i + 1 < len(payload):
                unescaped.append(payload[i + 1] ^ 0x20)
                i += 2
            else:
                unescaped.append(payload[i])
                i += 1
                
        return bytes(unescaped)
        
    def decode_lat_lon(self, raw_value: int, is_longitude: bool = False) -> float:
        """Decode 24-bit latitude/longitude"""
        # Handle 2's complement for negative values
        if raw_value & 0x800000:
            raw_value = raw_value - 0x1000000
            
        # Convert to degrees (GDL-90 spec uses 180.0 for both lat and lon)
        max_range = 180.0  # Both latitude and longitude use 180.0 range
        return (raw_value * max_range) / 0x800000
        
    def decode_altitude(self, raw_value: int) -> int:
        """Decode altitude from 12-bit value"""
        if raw_value == 0:
            return -1000  # Special case
        return (raw_value * 25) - 1000
        
    def decode_velocity(self, raw_value: int) -> str:
        """Decode horizontal velocity"""
        if raw_value == 0xFFE:
            return "No data"
        return str(raw_value)
        
    def decode_vertical_velocity(self, raw_value: int) -> str:
        """Decode vertical velocity from 12-bit 2's complement"""
        if raw_value == 0x800:
            return "No data"
            
        # Handle 2's complement
        if raw_value & 0x800:
            vs_64fpm = raw_value - 0x1000
        else:
            vs_64fpm = raw_value
            
        return str(vs_64fpm * 64)
        
    def decode_track(self, raw_value: int) -> float:
        """Decode track/heading"""
        return (raw_value * 360.0) / 256.0
        
    def decode_heartbeat(self, data: bytes, capture_time: float) -> Dict[str, Any]:
        """Decode GDL-90 heartbeat message"""
        if len(data) < 7:
            return {"error": "Heartbeat too short", "raw_data": data.hex()}
            
        status1 = data[1]
        status2 = data[2]
        timestamp_low = struct.unpack('<H', data[3:5])[0]
        message_count = struct.unpack('>H', data[5:7])[0]
        
        # Handle timestamp bit 16
        timestamp_bit16 = (status2 & 0x80) >> 7
        timestamp = timestamp_low | (timestamp_bit16 << 16)
        
        # Convert to hours:minutes:seconds
        hours = timestamp // 3600
        minutes = (timestamp % 3600) // 60
        seconds = timestamp % 60
        
        return {
            "message_type": "Heartbeat",
            "capture_timestamp": capture_time,
            "status_byte_1": f"0x{status1:02X}",
            "status_byte_2": f"0x{status2:02X}",
            "timestamp": timestamp,
            "timestamp_hours": hours,
            "timestamp_minutes": minutes,
            "timestamp_seconds": seconds,
            "message_count": message_count,
            "raw_data": data.hex().upper()
        }
        
    def decode_position_report(self, data: bytes, capture_time: float, is_ownship: bool = False) -> Dict[str, Any]:
        """Decode position report (ownship or traffic)"""
        if len(data) < 28:
            return {"error": "Position report too short", "raw_data": data.hex()}
            
        try:
            # Parse fields
            msg_type = "Ownship Position Report" if is_ownship else "Traffic Report"
            
            if not is_ownship:
                alert_status = data[1]
                icao_address = struct.unpack('>I', b'\x00' + data[2:5])[0]
                offset = 5
            else:
                alert_status = None
                icao_address = None
                offset = 5  # FIX: Ownship coordinates also start at byte 5, not byte 1
                
            # Latitude (24-bit)
            lat_raw = struct.unpack('>I', b'\x00' + data[offset:offset+3])[0]
            latitude = self.decode_lat_lon(lat_raw, False)
            
            # Longitude (24-bit)  
            lon_raw = struct.unpack('>I', b'\x00' + data[offset+3:offset+6])[0]
            longitude = self.decode_lat_lon(lon_raw, True)
            
            # Altitude (12-bit) + Misc (4-bit)
            alt_misc = struct.unpack('>H', data[offset+6:offset+8])[0]
            altitude_raw = (alt_misc >> 4) & 0xFFF
            misc = alt_misc & 0xF
            altitude = self.decode_altitude(altitude_raw)
            
            # NIC + NACp
            nic_nacp = data[offset+8]
            nic = (nic_nacp >> 4) & 0xF
            nacp = nic_nacp & 0xF
            
            # Velocities
            h_vel_raw = struct.unpack('>H', data[offset+9:offset+11])[0]
            v_vel_raw = struct.unpack('>H', data[offset+10:offset+12])[0] & 0xFFF
            
            horizontal_velocity = self.decode_velocity((h_vel_raw >> 4) & 0xFFF)
            vertical_velocity = self.decode_vertical_velocity(v_vel_raw)
            
            # Track
            track_raw = data[offset+12]
            track = self.decode_track(track_raw)
            
            # Emitter category
            emitter_category = data[offset+13]
            
            # Callsign (8 bytes)
            callsign_bytes = data[offset+14:offset+22]
            callsign = callsign_bytes.decode('ascii', errors='ignore').strip()
            
            # Emergency code
            emergency_code = data[offset+22] if len(data) > offset+22 else 0
            
            result = {
                "message_type": msg_type,
                "capture_timestamp": capture_time,
                "latitude": latitude,
                "longitude": longitude,
                "altitude": altitude,
                "horizontal_velocity": horizontal_velocity,
                "vertical_velocity": vertical_velocity,
                "track": track,
                "callsign": f'"{callsign}"',
                "emitter_category": emitter_category,
                "emergency_code": emergency_code,
                "nic": nic,
                "nacp": nacp,
                "raw_data": data.hex().upper()
            }
            
            if not is_ownship:
                result["alert_status"] = alert_status
                result["icao_address"] = f"0x{icao_address:06X}"
                
            return result
            
        except Exception as e:
            return {"error": f"Decode error: {e}", "raw_data": data.hex()}
            
    def decode_packet(self, data: bytes, capture_time: float) -> Dict[str, Any]:
        """Decode a single GDL-90 packet"""
        try:
            # Unescape the data
            unescaped = self.unescape_data(data)
            
            # Verify minimum length
            if len(unescaped) < 3:
                return {"error": "Packet too short", "raw_data": data.hex()}
                
            # Extract payload and CRC
            payload = unescaped[:-2]
            crc_received = struct.unpack('<H', unescaped[-2:])[0]
            
            # Verify CRC
            crc_calculated = self.compute_crc(payload)
            crc_valid = (crc_received == crc_calculated)
            
            # Decode based on message type
            msg_id = payload[0]
            
            base_result = {
                "crc_valid": crc_valid,
                "crc_received": f"0x{crc_received:04X}",
                "crc_calculated": f"0x{crc_calculated:04X}"
            }
            
            if msg_id == 0x00:  # Heartbeat
                result = self.decode_heartbeat(payload, capture_time)
            elif msg_id == 0x0A:  # Ownship Position Report
                result = self.decode_position_report(payload, capture_time, is_ownship=True)
            elif msg_id == 0x14:  # Traffic Report
                result = self.decode_position_report(payload, capture_time, is_ownship=False)
            else:
                result = {
                    "message_type": f"Unknown (0x{msg_id:02X})",
                    "capture_timestamp": capture_time,
                    "raw_data": data.hex().upper()
                }
                
            result.update(base_result)
            return result
            
        except Exception as e:
            return {
                "error": f"Decode exception: {e}",
                "raw_data": data.hex(),
                "capture_timestamp": capture_time
            }
            
    def update_statistics(self, decoded_packet: Dict[str, Any]):
        """Update capture statistics"""
        self.statistics["total_packets"] += 1
        
        if decoded_packet.get("crc_valid"):
            self.statistics["valid_packets"] += 1
        else:
            self.statistics["crc_errors"] += 1
            
        if "error" in decoded_packet:
            self.statistics["decode_errors"] += 1
            
        msg_type = decoded_packet.get("message_type", "Unknown")
        self.statistics["message_types"][msg_type] = self.statistics["message_types"].get(msg_type, 0) + 1
        
    def print_realtime_status(self, decoded_packet: Dict[str, Any]):
        """Print real-time status of received packet"""
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        msg_type = decoded_packet.get("message_type", "Unknown")
        crc_status = "✓" if decoded_packet.get("crc_valid") else "✗"
        
        # Format based on message type
        if "Heartbeat" in msg_type:
            msg_count = decoded_packet.get("message_count", "?")
            utc_time = f"{decoded_packet.get('timestamp_hours', 0):02d}:{decoded_packet.get('timestamp_minutes', 0):02d}:{decoded_packet.get('timestamp_seconds', 0):02d}"
            print(f"[{timestamp}] {crc_status} Heartbeat | Count: {msg_count} | UTC: {utc_time}")
            
        elif "Position" in msg_type or "Traffic" in msg_type:
            callsign = decoded_packet.get("callsign", "Unknown").strip('"')
            lat = decoded_packet.get("latitude", 0)
            lon = decoded_packet.get("longitude", 0)
            alt = decoded_packet.get("altitude", 0)
            print(f"[{timestamp}] {crc_status} {msg_type} | {callsign} | ({lat:.6f}, {lon:.6f}) | {alt}ft")
            
        else:
            print(f"[{timestamp}] {crc_status} {msg_type}")
            
        if "error" in decoded_packet:
            print(f"    └── Error: {decoded_packet['error']}")
            
    def start_capture(self, duration: int = 60, save_interval: int = 10):
        """Start real-time capture, decode, and save"""
        try:
            # Create UDP socket
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.bind((self.ip, self.port))
            self.socket.settimeout(1.0)
            
            print(f"=== Real-time GDL-90 Capture Started ===")
            print(f"Listening on: {self.ip}:{self.port}")
            print(f"Duration: {duration} seconds")
            print(f"Output file: {self.output_file}")
            print(f"Auto-save every: {save_interval} seconds")
            print("Press Ctrl+C to stop early\n")
            
            start_time = time.time()
            last_save = start_time
            self.running = True
            
            while self.running and (time.time() - start_time) < duration:
                try:
                    # Receive UDP packet
                    data, addr = self.socket.recvfrom(1024)
                    capture_time = time.time()
                    
                    # Decode packet in real-time
                    decoded_packet = self.decode_packet(data, capture_time)
                    
                    # Add to collection
                    self.decoded_packets.append(decoded_packet)
                    
                    # Update statistics
                    self.update_statistics(decoded_packet)
                    
                    # Print real-time status
                    self.print_realtime_status(decoded_packet)
                    
                    # Auto-save periodically
                    if time.time() - last_save >= save_interval:
                        self.save_data()
                        last_save = time.time()
                        print(f"    └── Auto-saved {len(self.decoded_packets)} packets")
                        
                except socket.timeout:
                    continue
                    
        except KeyboardInterrupt:
            print(f"\n*** Capture interrupted by user ***")
        except Exception as e:
            print(f"\n*** Error during capture: {e} ***")
        finally:
            self.running = False
            if self.socket:
                self.socket.close()
                
        # Final save and summary
        self.save_data()
        self.print_summary()
        
    def save_data(self):
        """Save decoded packets to JSON file"""
        report_data = {
            "capture_info": {
                "timestamp": datetime.now().isoformat(),
                "capture_duration": len(self.decoded_packets),
                "source": f"{self.ip}:{self.port}",
                "decoder_version": "realtime_v1.0"
            },
            "statistics": self.statistics,
            "packets": self.decoded_packets
        }
        
        with open(self.output_file, 'w') as f:
            json.dump(report_data, f, indent=2)
            
    def print_summary(self):
        """Print capture summary"""
        print(f"\n=== Capture Summary ===")
        print(f"Total packets: {self.statistics['total_packets']}")
        print(f"Valid packets: {self.statistics['valid_packets']}")
        print(f"CRC errors: {self.statistics['crc_errors']}")
        print(f"Decode errors: {self.statistics['decode_errors']}")
        print(f"Success rate: {(self.statistics['valid_packets']/max(1,self.statistics['total_packets'])*100):.1f}%")
        
        print(f"\nMessage types:")
        for msg_type, count in self.statistics["message_types"].items():
            print(f"  {msg_type}: {count}")
            
        print(f"\nData saved to: {self.output_file}")

def main():
    if len(sys.argv) < 2:
        print("Real-time GDL-90 UDP Capture and Decoder")
        print("\nUsage:")
        print("  python3 realtime_gdl90_capture.py <duration> [ip] [port] [output_file]")
        print("\nExamples:")
        print("  python3 realtime_gdl90_capture.py 30")
        print("  python3 realtime_gdl90_capture.py 60 127.0.0.1 4000")
        print("  python3 realtime_gdl90_capture.py 120 192.168.1.100 4000 flight_data.json")
        print("\nFeatures:")
        print("  • Real-time UDP packet capture")
        print("  • Live GDL-90 decoding")
        print("  • CRC validation")
        print("  • Auto-save every 10 seconds")
        print("  • Comprehensive statistics")
        print("  • JSON output format")
        sys.exit(1)
        
    duration = int(sys.argv[1])
    ip = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    output_file = sys.argv[4] if len(sys.argv) > 4 else "realtime_capture.json"
    
    capture = RealtimeGDL90Capture(ip, port, output_file)
    capture.start_capture(duration)

if __name__ == "__main__":
    main()
