#!/usr/bin/env python3
"""
GDL-90 Message Decoder
Decodes GDL-90 messages according to the official specification
"""

import struct
import math
from typing import List, Dict, Any, Optional

# GDL-90 CRC-16-CCITT lookup table (same as in C++ code)
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

class GDL90Decoder:
    def __init__(self):
        self.messages = []
        
    def compute_crc(self, data: bytes) -> int:
        """Compute CRC-16-CCITT checksum (matches C++ implementation)"""
        crc = 0
        mask16bit = 0xFFFF
        for byte in data:
            m = (crc << 8) & mask16bit
            crc = GDL90_CRC16_TABLE[(crc >> 8)] ^ m ^ byte
        return crc & 0xFFFF
        
    def unescape_data(self, data: bytes) -> bytes:
        """Remove GDL-90 escape sequences"""
        unescaped = bytearray()
        i = 0
        while i < len(data):
            if data[i] == 0x7D and i + 1 < len(data):
                # Escape sequence: 0x7D followed by modified byte
                unescaped.append(data[i + 1] ^ 0x20)
                i += 2
            else:
                unescaped.append(data[i])
                i += 1
        return bytes(unescaped)
        
    def decode_frame(self, frame: bytes) -> Optional[Dict[str, Any]]:
        """Decode a complete GDL-90 frame"""
        if len(frame) < 5:  # Minimum: start + msg_id + crc + end
            return {"error": "Frame too short"}
            
        # Check frame flags
        if frame[0] != 0x7E or frame[-1] != 0x7E:
            return {"error": "Invalid frame flags"}
            
        # Remove frame flags
        payload = frame[1:-1]
        
        # Unescape data
        unescaped = self.unescape_data(payload)
        
        if len(unescaped) < 3:  # msg_id + 2-byte CRC
            return {"error": "Payload too short after unescaping"}
            
        # Extract message data and CRC
        message_data = unescaped[:-2]
        received_crc = (unescaped[-2] << 8) | unescaped[-1]  # Low byte first (GDL90 spec)
        
        # Verify CRC
        calculated_crc = self.compute_crc(message_data)
        crc_valid = (calculated_crc == received_crc)
        
        # Decode message
        result = {
            "frame_length": len(frame),
            "payload_length": len(unescaped),
            "crc_received": f"0x{received_crc:04X}",
            "crc_calculated": f"0x{calculated_crc:04X}",
            "crc_valid": crc_valid,
            "raw_frame": frame.hex(),
            "raw_payload": unescaped.hex()
        }
        
        if len(message_data) > 0:
            msg_id = message_data[0]
            result["message_id"] = f"0x{msg_id:02X}"
            result["message_type"] = self.get_message_type_name(msg_id)
            
            if crc_valid:
                # Decode message content based on type
                decoded_msg = self.decode_message(msg_id, message_data[1:])
                result.update(decoded_msg)
            else:
                result["error"] = "CRC mismatch - message corrupted"
        else:
            result["error"] = "No message ID found"
            
        return result
        
    def get_message_type_name(self, msg_id: int) -> str:
        """Get human-readable message type name"""
        types = {
            0x00: "Heartbeat",
            0x0A: "Ownship Position Report", 
            0x0B: "Ownship Geometric Altitude", 
            0x14: "Traffic Report"
        }
        return types.get(msg_id, f"Unknown (0x{msg_id:02X})")
        
    def decode_message(self, msg_id: int, data: bytes) -> Dict[str, Any]:
        """Decode message content based on message ID"""
        if msg_id == 0x00:
            return self.decode_heartbeat(data)
        elif msg_id == 0x0A:
            return self.decode_position_report(data, is_ownship=True)
        elif msg_id == 0x14:
            return self.decode_position_report(data, is_ownship=False)
        else:
            return {"message_data": data.hex()}
            
    def decode_heartbeat(self, data: bytes) -> Dict[str, Any]:
        """Decode Heartbeat message (ID 0x00)"""
        if len(data) < 6:
            return {"error": "Heartbeat message too short"}
            
        st1 = data[0]
        st2 = data[1]
        timestamp_low = (data[3] << 8) | data[2]  # Little endian
        msg_count = (data[4] << 8) | data[5]      # Big endian
        
        # Extract timestamp bit 16 from status byte 2
        ts_bit16 = (st2 & 0x80) >> 7
        timestamp = timestamp_low | (ts_bit16 << 16)
        
        return {
            "status_byte_1": f"0x{st1:02X}",
            "status_byte_2": f"0x{st2:02X}", 
            "timestamp": timestamp,
            "timestamp_hours": timestamp // 3600,
            "timestamp_minutes": (timestamp % 3600) // 60,
            "timestamp_seconds": timestamp % 60,
            "message_count": msg_count
        }
        
    def decode_position_report(self, data: bytes, is_ownship: bool) -> Dict[str, Any]:
        """Decode Position Report message (ID 0x0A or 0x14)"""
        expected_length = 27 if is_ownship else 27
        if len(data) < expected_length:
            return {"error": f"Position report too short (got {len(data)}, expected {expected_length})"}
            
        result = {}
        offset = 0
        
        if not is_ownship:
            # Traffic reports have alert status and address type in first byte
            alert_addr = data[0]
            result["alert_status"] = (alert_addr >> 7) & 0x1
            result["address_type"] = (alert_addr >> 4) & 0x7
            offset = 1
        else:
            # Ownship reports have status and address type
            status_addr = data[0]
            result["status"] = (status_addr >> 4) & 0xF
            result["address_type"] = status_addr & 0xF
            offset = 1
            
        # ICAO address (24-bit)
        icao = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2]
        result["icao_address"] = f"0x{icao:06X}"
        offset += 3
        
        # Latitude (24-bit signed)
        lat_raw = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2]
        if lat_raw & 0x800000:  # Sign bit
            lat_raw = -(0x1000000 - lat_raw)
        latitude = lat_raw * (180.0 / 0x800000)
        result["latitude"] = latitude
        offset += 3
        
        # Longitude (24-bit signed)  
        lon_raw = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2]
        if lon_raw & 0x800000:  # Sign bit
            lon_raw = -(0x1000000 - lon_raw)
        longitude = lon_raw * (180.0 / 0x800000)
        result["longitude"] = longitude
        offset += 3
        
        # Altitude and miscellaneous (12+4 bits)
        alt_misc = (data[offset] << 8) | data[offset+1]
        altitude_encoded = (alt_misc >> 4) & 0xFFF
        misc = alt_misc & 0xF
        
        if altitude_encoded == 0xFFF:
            result["altitude"] = "Invalid"
        else:
            result["altitude"] = (altitude_encoded * 25) - 1000
        result["miscellaneous"] = f"0x{misc:X}"
        result["misc_air_ground"] = "Airborne" if (misc & 0x8) else "Ground"
        result["misc_extrapolated"] = bool(misc & 0x4)
        track_heading_type = misc & 0x3
        if track_heading_type == 0:
            result["misc_track_type"] = "Not valid"
        elif track_heading_type == 1:
            result["misc_track_type"] = "True Track"
        elif track_heading_type == 2:
            result["misc_track_type"] = "Magnetic Heading"
        else:
            result["misc_track_type"] = "True Heading"
        offset += 2
        
        # NIC and NACp (4+4 bits)
        nic_nacp = data[offset]
        result["nic"] = (nic_nacp >> 4) & 0xF
        result["nacp"] = nic_nacp & 0xF
        offset += 1
        
        # Velocities (12+12 bits packed in 3 bytes)
        vel_bytes = data[offset:offset+3]
        h_velocity = ((vel_bytes[0] << 4) | (vel_bytes[1] >> 4)) & 0xFFF
        v_velocity = ((vel_bytes[1] & 0xF) << 8) | vel_bytes[2]
        
        if h_velocity == 0xFFF:
            result["horizontal_velocity"] = "No data"
        else:
            result["horizontal_velocity"] = h_velocity
            
        if v_velocity == 0x800:
            result["vertical_velocity"] = "No data"
        else:
            # Convert 12-bit 2's complement to signed value
            if v_velocity & 0x800:
                v_velocity = -(0x1000 - v_velocity)
            result["vertical_velocity"] = v_velocity * 64  # Convert to fpm
        offset += 3
        
        # Track/Heading (8-bit)
        track_raw = data[offset]
        result["track_heading"] = track_raw * (360.0 / 256.0)
        offset += 1
        
        # Emitter category
        result["emitter_category"] = data[offset]
        offset += 1
        
        # Callsign (8 bytes)
        callsign_bytes = data[offset:offset+8]
        callsign = ''.join([chr(b) if 32 <= b <= 126 else ' ' for b in callsign_bytes])
        result["callsign"] = f"'{callsign.rstrip()}'"
        offset += 8
        
        # Emergency code (4 bits)
        emergency = (data[offset] >> 4) & 0xF
        result["emergency_code"] = emergency
        
        return result
        
    def decode_packets_from_file(self, filename: str) -> List[Dict[str, Any]]:
        """Decode GDL-90 packets from captured file"""
        packets = []
        
        try:
            with open(filename, 'rb') as f:
                while True:
                    # Read packet header
                    header = f.read(12)  # 8 bytes timestamp + 4 bytes size
                    if len(header) < 12:
                        break
                        
                    timestamp = int.from_bytes(header[:8], 'little') / 1000000.0
                    size = int.from_bytes(header[8:12], 'little')
                    
                    # Read packet data
                    data = f.read(size)
                    if len(data) < size:
                        break
                        
                    # Decode the frame
                    decoded = self.decode_frame(data)
                    decoded["capture_timestamp"] = timestamp
                    packets.append(decoded)
                    
        except FileNotFoundError:
            print(f"File {filename} not found")
        except Exception as e:
            print(f"Error reading file: {e}")
            
        return packets

def main():
    import sys
    
    if len(sys.argv) < 2:
        print("Usage: python3 gdl90_decoder.py <captured_file.bin>")
        print("   or: python3 gdl90_decoder.py --test")
        sys.exit(1)
        
    if sys.argv[1] == "--test":
        # Test with sample data
        decoder = GDL90Decoder()
        
        # Sample heartbeat frame (with correct CRC)
        test_frame = bytes([0x7E, 0x00, 0x81, 0x01, 0x00, 0x00, 0x00, 0x00, 0xBC, 0x9C, 0x7E])
        
        print("Testing with sample frame...")
        result = decoder.decode_frame(test_frame)
        
        import json
        print(json.dumps(result, indent=2))
    else:
        decoder = GDL90Decoder()
        packets = decoder.decode_packets_from_file(sys.argv[1])
        
        print(f"Decoded {len(packets)} packets")
        
        for i, packet in enumerate(packets):
            print(f"\n=== Packet {i+1} ===")
            import json
            print(json.dumps(packet, indent=2))

if __name__ == "__main__":
    main()
