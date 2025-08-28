#!/usr/bin/env python3
"""
GDL-90 UDP Packet Capture Script
Captures UDP packets from XP2GDL90 plugin for testing and verification
"""

import socket
import time
import sys
from datetime import datetime

class UDPCapture:
    def __init__(self, ip="127.0.0.1", port=4000, output_file="captured_gdl90.bin"):
        self.ip = ip
        self.port = port
        self.output_file = output_file
        self.socket = None
        self.running = False
        
    def start_capture(self, duration=60):
        """Start capturing UDP packets for specified duration (seconds)"""
        try:
            # Create UDP socket
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.socket.bind((self.ip, self.port))
            self.socket.settimeout(1.0)  # 1 second timeout for checking stop condition
            
            print(f"Starting UDP capture on {self.ip}:{self.port}")
            print(f"Capturing for {duration} seconds...")
            print(f"Output file: {self.output_file}")
            print("Press Ctrl+C to stop early\n")
            
            packets = []
            start_time = time.time()
            self.running = True
            
            while self.running and (time.time() - start_time) < duration:
                try:
                    data, addr = self.socket.recvfrom(1024)
                    timestamp = datetime.now()
                    
                    packet_info = {
                        'timestamp': timestamp,
                        'source': addr,
                        'data': data,
                        'size': len(data)
                    }
                    packets.append(packet_info)
                    
                    print(f"[{timestamp.strftime('%H:%M:%S.%f')[:-3]}] "
                          f"Packet from {addr}: {len(data)} bytes")
                    
                except socket.timeout:
                    continue  # Check if we should stop
                    
        except KeyboardInterrupt:
            print("\nCapture interrupted by user")
        except Exception as e:
            print(f"Error during capture: {e}")
        finally:
            if self.socket:
                self.socket.close()
            self.running = False
            
        # Save captured packets
        if packets:
            self.save_packets(packets)
            print(f"\nCaptured {len(packets)} packets")
            self.analyze_packets(packets)
        else:
            print("\nNo packets captured")
            
    def save_packets(self, packets):
        """Save captured packets to binary file"""
        with open(self.output_file, 'wb') as f:
            for packet in packets:
                # Write packet header: timestamp (8 bytes) + size (4 bytes)
                timestamp_bytes = int(packet['timestamp'].timestamp() * 1000000).to_bytes(8, 'little')
                size_bytes = packet['size'].to_bytes(4, 'little')
                f.write(timestamp_bytes)
                f.write(size_bytes)
                f.write(packet['data'])
                
    def analyze_packets(self, packets):
        """Basic analysis of captured packets"""
        print("\n=== Packet Analysis ===")
        
        message_types = {}
        total_bytes = 0
        
        for packet in packets:
            data = packet['data']
            total_bytes += len(data)
            
            # Check for GDL-90 frame (starts and ends with 0x7E)
            if len(data) >= 2 and data[0] == 0x7E and data[-1] == 0x7E:
                if len(data) >= 3:
                    # Message ID is first byte after start flag (before escaping)
                    msg_id = data[1]
                    msg_name = self.get_message_name(msg_id)
                    message_types[msg_name] = message_types.get(msg_name, 0) + 1
                    
        print(f"Total packets: {len(packets)}")
        print(f"Total bytes: {total_bytes}")
        print("\nMessage types:")
        for msg_type, count in message_types.items():
            print(f"  {msg_type}: {count}")
            
    def get_message_name(self, msg_id):
        """Get GDL-90 message type name"""
        message_types = {
            0x00: "Heartbeat",
            0x0A: "Ownship Position",
            0x14: "Traffic Report",
            0x0B: "Ownship Geometric Altitude",
        }
        return message_types.get(msg_id, f"Unknown (0x{msg_id:02X})")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 test_udp_capture.py <duration_seconds> [ip] [port]")
        print("Example: python3 test_udp_capture.py 30")
        print("Example: python3 test_udp_capture.py 60 127.0.0.1 4000")
        sys.exit(1)
        
    duration = int(sys.argv[1])
    ip = sys.argv[2] if len(sys.argv) > 2 else "127.0.0.1"
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 4000
    
    capture = UDPCapture(ip, port)
    capture.start_capture(duration)

if __name__ == "__main__":
    main()
