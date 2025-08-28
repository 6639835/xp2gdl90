#!/usr/bin/env python3
"""
Test Data Generator for XP2GDL90 Plugin
Creates known test cases for validating GDL-90 encoding/decoding
"""

import json
import math
from typing import Dict, List, Any

class TestDataGenerator:
    def __init__(self):
        self.test_cases = []
        
    def generate_test_cases(self) -> List[Dict[str, Any]]:
        """Generate comprehensive test cases"""
        
        # Test Case 1: San Francisco Airport (KSFO) - Ownship
        test_case_1 = {
            "name": "KSFO Ownship - Normal Flight",
            "type": "ownship",
            "description": "Aircraft at San Francisco Airport in normal flight",
            "input": {
                "latitude": 37.621311,
                "longitude": -122.378968,
                "altitude": 1247.0,  # feet MSL
                "ground_speed": 145.0,  # knots
                "vertical_speed": -580.0,  # fpm (descending)
                "track": 267.4,  # degrees
                "callsign": "PYTHON1 ",
                "icao_address": 0xABCDEF
            },
            "expected_gdl90": {
                "message_id": "0x0A",
                "latitude_24bit": 0x1AC04E,  # Calculated: 37.621311 * (0x800000 / 180.0)
                "longitude_24bit": 0xA901F4,  # Calculated: -122.378968 * (0x800000 / 180.0) in 2's complement
                "altitude_encoded": 89,  # (1247 + 1000) / 25 = 89.88 -> 89
                "ground_speed_encoded": 145,  # knots
                "vertical_speed_encoded": 0xFF7,  # -580/64 = -9.06 -> -9 in 12-bit 2's complement
                "track_encoded": 190  # 267.4 * (256/360) = 190.04 -> 190
            }
        }
        
        # Test Case 2: Traffic Target - Commercial Aircraft  
        test_case_2 = {
            "name": "Commercial Traffic - UAL123",
            "type": "traffic",
            "description": "United Airlines flight at cruise altitude", 
            "input": {
                "latitude": 37.7749,  # San Francisco area
                "longitude": -122.4194,
                "altitude": 35000.0,  # feet MSL
                "ground_speed": 420.0,  # knots
                "vertical_speed": 0.0,  # level flight
                "track": 45.0,  # northeast heading
                "callsign": "UAL123  ",
                "icao_address": 0x100001
            },
            "expected_gdl90": {
                "message_id": "0x14",
                "latitude_24bit": 0x1AD9B8,  # 37.7749 * (0x800000 / 180.0)
                "longitude_24bit": 0xA8EE62,  # -122.4194 * (0x800000 / 180.0) in 2's complement
                "altitude_encoded": 1440,  # (35000 + 1000) / 25 = 1440
                "ground_speed_encoded": 420,
                "vertical_speed_encoded": 0x800,  # No data (0 fpm)
                "track_encoded": 32  # 45.0 * (256/360) = 32
            }
        }
        
        # Test Case 3: Low Altitude Traffic
        test_case_3 = {
            "name": "Pattern Traffic - N737BA", 
            "type": "traffic",
            "description": "Light aircraft in traffic pattern",
            "input": {
                "latitude": 37.615,
                "longitude": -122.375,
                "altitude": 800.0,  # pattern altitude
                "ground_speed": 85.0,  # knots
                "vertical_speed": 500.0,  # climbing
                "track": 180.0,  # south
                "callsign": "N737BA  ",
                "icao_address": 0x100002
            },
            "expected_gdl90": {
                "message_id": "0x14", 
                "latitude_24bit": 0x1ABE4A,
                "longitude_24bit": 0xA90383,
                "altitude_encoded": 72,  # (800 + 1000) / 25 = 72
                "ground_speed_encoded": 85,
                "vertical_speed_encoded": 8,  # 500/64 = 7.8 -> 8
                "track_encoded": 128  # 180.0 * (256/360) = 128
            }
        }
        
        # Test Case 4: Edge Cases
        test_case_4 = {
            "name": "Edge Cases - Extreme Values",
            "type": "traffic", 
            "description": "Testing boundary conditions",
            "input": {
                "latitude": -90.0,  # South pole
                "longitude": 180.0,  # International date line
                "altitude": -1000.0,  # Sea level at low tide
                "ground_speed": 0.0,  # Stationary
                "vertical_speed": -6000.0,  # Rapid descent
                "track": 0.0,  # North
                "callsign": "EDGE1   ",
                "icao_address": 0x100003
            },
            "expected_gdl90": {
                "message_id": "0x14",
                "latitude_24bit": 0xC00000,  # -90.0 encoded
                "longitude_24bit": 0x800000,  # 180.0 encoded  
                "altitude_encoded": 0,  # (-1000 + 1000) / 25 = 0
                "ground_speed_encoded": 0,
                "vertical_speed_encoded": 0xE1D,  # -6000/64 = -93.75 -> -94 in 2's complement
                "track_encoded": 0
            }
        }
        
        # Test Case 5: High Speed Traffic
        test_case_5 = {
            "name": "High Speed - Military",
            "type": "traffic",
            "description": "High speed military aircraft",
            "input": {
                "latitude": 40.0,
                "longitude": -120.0, 
                "altitude": 45000.0,
                "ground_speed": 600.0,  # High speed
                "vertical_speed": 4000.0,  # Rapid climb
                "track": 270.0,  # West
                "callsign": "ARMY01  ",
                "icao_address": 0x100004
            },
            "expected_gdl90": {
                "message_id": "0x14",
                "latitude_24bit": 0x1C71C7,
                "longitude_24bit": 0xAAAB00,
                "altitude_encoded": 1840,  # (45000 + 1000) / 25 = 1840
                "ground_speed_encoded": 600,
                "vertical_speed_encoded": 62,  # 4000/64 = 62.5 -> 62
                "track_encoded": 192  # 270.0 * (256/360) = 192
            }
        }
        
        self.test_cases = [test_case_1, test_case_2, test_case_3, test_case_4, test_case_5]
        return self.test_cases
        
    def calculate_expected_encoding(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """Calculate expected GDL-90 encoding values"""
        
        # Latitude encoding (24-bit signed)
        lat = input_data["latitude"]
        lat = max(-90.0, min(90.0, lat))  # Clamp to valid range
        lat_raw = int(lat * (0x800000 / 180.0))
        if lat_raw < 0:
            lat_24bit = (0x1000000 + lat_raw) & 0xFFFFFF
        else:
            lat_24bit = lat_raw & 0xFFFFFF
            
        # Longitude encoding (24-bit signed)
        lon = input_data["longitude"] 
        lon = max(-180.0, min(180.0, lon))  # Clamp to valid range
        lon_raw = int(lon * (0x800000 / 180.0))
        if lon_raw < 0:
            lon_24bit = (0x1000000 + lon_raw) & 0xFFFFFF
        else:
            lon_24bit = lon_raw & 0xFFFFFF
            
        # Altitude encoding (25-foot increments, +1000 ft offset)
        alt = input_data["altitude"]
        alt_encoded = int((alt + 1000) / 25.0)
        alt_encoded = max(0, min(0xFFE, alt_encoded))  # Clamp to valid range
        
        # Ground speed encoding (knots)
        speed = int(input_data["ground_speed"])
        speed_encoded = max(0, min(0xFFE, speed))
        
        # Vertical speed encoding (64 fpm units, 12-bit 2's complement)
        vs = input_data["vertical_speed"]
        if vs == 0:
            vs_encoded = 0x800  # No data flag
        else:
            vs_64fpm = int(vs / 64)
            if vs_64fpm < 0:
                vs_encoded = (0x1000 + vs_64fpm) & 0xFFF
            else:
                vs_encoded = vs_64fpm & 0xFFF
                
        # Track encoding (8-bit angular)
        track = input_data["track"] % 360.0  # Normalize to 0-360
        track_encoded = int(track / (360.0 / 256))
        
        return {
            "latitude_24bit": lat_24bit,
            "longitude_24bit": lon_24bit, 
            "altitude_encoded": alt_encoded,
            "ground_speed_encoded": speed_encoded,
            "vertical_speed_encoded": vs_encoded,
            "track_encoded": track_encoded
        }
        
    def save_test_cases(self, filename: str = "test_cases.json"):
        """Save test cases to JSON file"""
        if not self.test_cases:
            self.generate_test_cases()
            
        # Recalculate expected values to ensure accuracy
        for test_case in self.test_cases:
            calculated = self.calculate_expected_encoding(test_case["input"])
            test_case["calculated_gdl90"] = calculated
            
        with open(filename, 'w') as f:
            json.dump(self.test_cases, f, indent=2)
            
        print(f"Saved {len(self.test_cases)} test cases to {filename}")
        
    def print_test_summary(self):
        """Print summary of test cases"""
        if not self.test_cases:
            self.generate_test_cases()
            
        print("=== GDL-90 Test Cases ===\n")
        
        for i, test_case in enumerate(self.test_cases, 1):
            print(f"{i}. {test_case['name']}")
            print(f"   Type: {test_case['type']}")
            print(f"   Description: {test_case['description']}")
            
            input_data = test_case["input"]
            print(f"   Position: {input_data['latitude']:.6f}°, {input_data['longitude']:.6f}°")
            print(f"   Altitude: {input_data['altitude']:.0f} ft")
            print(f"   Speed: {input_data['ground_speed']:.0f} kt")
            print(f"   V/S: {input_data['vertical_speed']:.0f} fpm")
            print(f"   Track: {input_data['track']:.1f}°")
            print(f"   Callsign: '{input_data['callsign'].strip()}'")
            print()

def main():
    generator = TestDataGenerator()
    generator.generate_test_cases()
    generator.print_test_summary()
    generator.save_test_cases()

if __name__ == "__main__":
    main()
