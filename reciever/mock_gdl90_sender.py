#!/usr/bin/env python3
"""
Mock GDL-90 UDP Sender
Simulates the XP2GDL90 plugin output for testing the decoder without X-Plane
"""

import socket
import time
import struct
import math
from typing import List

# Same CRC table as in decoder
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

class MockGDL90Sender:
    def __init__(self, target_ip="127.0.0.1", target_port=4000):
        self.target_ip = target_ip
        self.target_port = target_port
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.message_count = 0
        
    def compute_crc(self, data: bytes) -> int:
        """Compute CRC exactly like the C++ implementation"""
        crc = 0
        mask16bit = 0xFFFF
        for byte in data:
            m = (crc << 8) & mask16bit
            crc = GDL90_CRC16_TABLE[(crc >> 8)] ^ m ^ byte
        return crc & 0xFFFF
        
    def escape_data(self, data: bytes) -> bytes:
        """Apply GDL-90 escape sequences"""
        escaped = bytearray()
        for byte in data:
            if byte == 0x7D or byte == 0x7E:
                escaped.append(0x7D)
                escaped.append(byte ^ 0x20)
            else:
                escaped.append(byte)
        return bytes(escaped)
        
    def create_frame(self, message_data: bytes) -> bytes:
        """Create complete GDL-90 frame with CRC and escaping"""
        # Calculate CRC
        crc = self.compute_crc(message_data)
        
        # Add CRC (low byte first)
        payload = bytearray(message_data)
        payload.append(crc & 0xFF)
        payload.append((crc >> 8) & 0xFF)
        
        # Escape data
        escaped = self.escape_data(bytes(payload))
        
        # Add frame flags
        frame = bytearray([0x7E])
        frame.extend(escaped)
        frame.append(0x7E)
        
        return bytes(frame)
        
    def create_heartbeat(self) -> bytes:
        """Create GDL-90 heartbeat message"""
        # Get current UTC time
        import datetime
        now = datetime.datetime.utcnow()
        timestamp = now.hour * 3600 + now.minute * 60 + now.second
        
        # Build message
        msg = bytearray([0x00])  # Message ID
        
        # Status bytes
        st1 = 0x81
        st2 = 0x01
        
        # Handle timestamp bit 16
        ts_bit16 = (timestamp & 0x10000) >> 16
        st2 = (st2 & 0x7F) | (ts_bit16 << 7)
        
        msg.append(st1)
        msg.append(st2)
        
        # Timestamp (little endian)
        ts_low = timestamp & 0xFFFF
        msg.append(ts_low & 0xFF)
        msg.append((ts_low >> 8) & 0xFF)
        
        # Message count (big endian)
        msg.append((self.message_count >> 8) & 0xFF)
        msg.append(self.message_count & 0xFF)
        
        self.message_count += 1
        return self.create_frame(bytes(msg))
        
    def encode_latitude(self, lat_deg: float) -> int:
        """Encode latitude to 24-bit GDL-90 format"""
        lat_deg = max(-90.0, min(90.0, lat_deg))
        lat_raw = int(lat_deg * (0x800000 / 180.0))
        if lat_raw < 0:
            lat_raw = (0x1000000 + lat_raw) & 0xFFFFFF
        return lat_raw & 0xFFFFFF
        
    def encode_longitude(self, lon_deg: float) -> int:
        """Encode longitude to 24-bit GDL-90 format"""
        lon_deg = max(-180.0, min(180.0, lon_deg))
        lon_raw = int(lon_deg * (0x800000 / 180.0))
        if lon_raw < 0:
            lon_raw = (0x1000000 + lon_raw) & 0xFFFFFF
        return lon_raw & 0xFFFFFF
        
    def create_traffic_report(self, lat: float, lon: float, alt: float, 
                            speed: float, vs: float, track: float, 
                            callsign: str, icao: int) -> bytes:
        """Create GDL-90 traffic report message"""
        msg = bytearray([0x14])  # Message ID
        
        # Alert status and address type
        msg.append(0x00)  # No alert, ADS-B with ICAO
        
        # ICAO address (24-bit, big endian)
        msg.append((icao >> 16) & 0xFF)
        msg.append((icao >> 8) & 0xFF)
        msg.append(icao & 0xFF)
        
        # Latitude (24-bit)
        lat_encoded = self.encode_latitude(lat)
        msg.append((lat_encoded >> 16) & 0xFF)
        msg.append((lat_encoded >> 8) & 0xFF)
        msg.append(lat_encoded & 0xFF)
        
        # Longitude (24-bit)
        lon_encoded = self.encode_longitude(lon)
        msg.append((lon_encoded >> 16) & 0xFF)
        msg.append((lon_encoded >> 8) & 0xFF)
        msg.append(lon_encoded & 0xFF)
        
        # Altitude (12-bit) + Misc (4-bit)
        alt_encoded = int((alt + 1000) / 25.0)
        alt_encoded = max(0, min(0xFFE, alt_encoded))
        misc = 9  # Airborne, updated, true track
        msg.append((alt_encoded >> 4) & 0xFF)
        msg.append(((alt_encoded & 0xF) << 4) | (misc & 0xF))
        
        # NIC + NACp
        nic = 11
        nacp = 10
        msg.append(((nic & 0xF) << 4) | (nacp & 0xF))
        
        # Velocities
        h_velocity = int(speed)
        h_velocity = max(0, min(0xFFE, h_velocity))
        
        if vs == 0:
            v_velocity = 0x800  # No data
        else:
            vs_64fpm = int(vs / 64)
            if vs_64fpm < 0:
                v_velocity = (0x1000 + vs_64fpm) & 0xFFF
            else:
                v_velocity = vs_64fpm & 0xFFF
                
        msg.append((h_velocity >> 4) & 0xFF)
        msg.append(((h_velocity & 0xF) << 4) | ((v_velocity >> 8) & 0xF))
        msg.append(v_velocity & 0xFF)
        
        # Track
        track_encoded = int(track / (360.0 / 256))
        msg.append(track_encoded & 0xFF)
        
        # Emitter category
        msg.append(1)  # Light aircraft
        
        # Callsign (8 bytes)
        callsign_bytes = (callsign + "        ")[:8].encode('ascii')
        msg.extend(callsign_bytes)
        
        # Emergency code
        msg.append(0x00)
        
        return self.create_frame(bytes(msg))
        
    def send_message(self, message: bytes):
        """Send message via UDP"""
        self.socket.sendto(message, (self.target_ip, self.target_port))
        
    def simulate_flight(self, duration: int = 30):
        """Simulate a complete flight scenario"""
        print(f"Starting GDL-90 simulation for {duration} seconds...")
        print(f"Sending to {self.target_ip}:{self.target_port}")
        
        start_time = time.time()
        last_heartbeat = 0
        last_position = 0
        last_traffic = 0
        
        # Flight scenarios
        ownship_data = {
            "lat": 37.621311,
            "lon": -122.378968,
            "alt": 1247.0,
            "speed": 145.0,
            "vs": -580.0,
            "track": 267.4
        }
        
        traffic_data = [
            {"lat": 37.7749, "lon": -122.4194, "alt": 35000, "speed": 420, 
             "vs": 0, "track": 45.0, "callsign": "UAL123", "icao": 0x100001},
            {"lat": 37.615, "lon": -122.375, "alt": 800, "speed": 85,
             "vs": 500, "track": 180.0, "callsign": "N737BA", "icao": 0x100002}
        ]
        
        packet_count = 0
        
        while time.time() - start_time < duration:
            current_time = time.time() - start_time
            
            # Send heartbeat every 1 second
            if current_time - last_heartbeat >= 1.0:
                heartbeat = self.create_heartbeat()
                self.send_message(heartbeat)
                print(f"[{current_time:5.1f}s] Sent heartbeat ({len(heartbeat)} bytes)")
                last_heartbeat = current_time
                packet_count += 1
                
            # Send ownship position every 0.5 seconds
            if current_time - last_position >= 0.5:
                position = self.create_traffic_report(
                    ownship_data["lat"], ownship_data["lon"], ownship_data["alt"],
                    ownship_data["speed"], ownship_data["vs"], ownship_data["track"],
                    "PYTHON1", 0xABCDEF
                )
                self.send_message(position)
                print(f"[{current_time:5.1f}s] Sent ownship position ({len(position)} bytes)")
                last_position = current_time
                packet_count += 1
                
            # Send traffic every 0.5 seconds
            if current_time - last_traffic >= 0.5:
                for i, traffic in enumerate(traffic_data):
                    traffic_msg = self.create_traffic_report(
                        traffic["lat"], traffic["lon"], traffic["alt"],
                        traffic["speed"], traffic["vs"], traffic["track"],
                        traffic["callsign"], traffic["icao"]
                    )
                    self.send_message(traffic_msg)
                    print(f"[{current_time:5.1f}s] Sent traffic {traffic['callsign']} ({len(traffic_msg)} bytes)")
                    packet_count += 1
                    
                last_traffic = current_time
                
            time.sleep(0.1)
            
        print(f"\nSimulation complete. Sent {packet_count} packets.")

def main():
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python3 mock_gdl90_sender.py <duration_seconds> [ip] [port]")
        print("Example: python3 mock_gdl90_sender.py 10")
        print("Example: python3 mock_gdl90_sender.py 30 127.0.0.1 4000")
        sys.exit(1)
        
    duration = int(sys.argv[1])
    ip = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    
    sender = MockGDL90Sender(ip, port)
    sender.simulate_flight(duration)

if __name__ == "__main__":
    main()
