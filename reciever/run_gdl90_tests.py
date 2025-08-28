#!/usr/bin/env python3
"""
Comprehensive GDL-90 Test Suite
Tests the complete XP2GDL90 plugin pipeline: encoding -> transmission -> decoding -> verification
"""

import os
import sys
import time
import json
import subprocess
import threading
from datetime import datetime
from typing import Dict, List, Any, Optional

# Import our test modules
from test_udp_capture import UDPCapture
from gdl90_decoder import GDL90Decoder
from test_data_generator import TestDataGenerator

class GDL90TestSuite:
    def __init__(self):
        self.test_results = []
        self.capture_file = "test_capture.bin"
        self.test_cases_file = "test_cases.json"
        
    def run_complete_test(self, capture_duration: int = 30) -> Dict[str, Any]:
        """Run the complete test suite"""
        
        print("=== XP2GDL90 Plugin Test Suite ===\n")
        
        # Step 1: Generate test cases
        print("Step 1: Generating test cases...")
        generator = TestDataGenerator()
        test_cases = generator.generate_test_cases()
        generator.save_test_cases(self.test_cases_file)
        generator.print_test_summary()
        
        # Step 2: Instructions for user
        print("\nStep 2: Plugin Setup Instructions")
        print("=" * 50)
        print("1. Make sure X-Plane is running with the XP2GDL90 plugin loaded")
        print("2. Enable AI traffic in X-Plane (Aircraft & Situations -> AI Aircraft)")
        print("3. Ensure plugin is broadcasting to 127.0.0.1:4000")
        print("4. You should see traffic data in X-Plane (or create test scenarios)")
        print("\nPress Enter when ready to start capture...")
        input()
        
        # Step 3: Capture UDP packets
        print(f"\nStep 3: Capturing UDP packets for {capture_duration} seconds...")
        self.capture_packets(capture_duration)
        
        # Step 4: Decode packets
        print("\nStep 4: Decoding captured packets...")
        decoded_packets = self.decode_packets()
        
        # Step 5: Analyze results
        print("\nStep 5: Analyzing results...")
        analysis = self.analyze_results(decoded_packets, test_cases)
        
        # Step 6: Generate report
        print("\nStep 6: Generating test report...")
        report = self.generate_report(analysis)
        
        print(f"\n=== Test Complete ===")
        print(f"Captured packets: {len(decoded_packets)}")
        print(f"Valid packets: {analysis['summary']['valid_packets']}")
        print(f"CRC errors: {analysis['summary']['crc_errors']}")
        print(f"Report saved to: test_report.json")
        
        return report
        
    def capture_packets(self, duration: int):
        """Capture UDP packets from the plugin"""
        try:
            capture = UDPCapture(output_file=self.capture_file)
            capture.start_capture(duration)
        except Exception as e:
            print(f"Error during packet capture: {e}")
            
    def decode_packets(self) -> List[Dict[str, Any]]:
        """Decode captured GDL-90 packets"""
        decoder = GDL90Decoder()
        
        if not os.path.exists(self.capture_file):
            print(f"Warning: Capture file {self.capture_file} not found")
            return []
            
        packets = decoder.decode_packets_from_file(self.capture_file)
        print(f"Decoded {len(packets)} packets")
        
        return packets
        
    def analyze_results(self, packets: List[Dict[str, Any]], test_cases: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Analyze decoded packets and compare with expected values"""
        
        analysis = {
            "summary": {
                "total_packets": len(packets),
                "valid_packets": 0,
                "crc_errors": 0,
                "message_types": {},
                "position_reports": [],
                "traffic_reports": [],
                "heartbeats": []
            },
            "detailed_analysis": [],
            "encoding_validation": []
        }
        
        # Analyze each packet
        for i, packet in enumerate(packets):
            packet_analysis = {
                "packet_number": i + 1,
                "timestamp": packet.get("capture_timestamp"),
                "message_type": packet.get("message_type", "Unknown"),
                "crc_valid": packet.get("crc_valid", False),
                "issues": []
            }
            
            # Count by message type
            msg_type = packet.get("message_type", "Unknown")
            analysis["summary"]["message_types"][msg_type] = analysis["summary"]["message_types"].get(msg_type, 0) + 1
            
            # Check CRC
            if packet.get("crc_valid"):
                analysis["summary"]["valid_packets"] += 1
            else:
                analysis["summary"]["crc_errors"] += 1
                packet_analysis["issues"].append("CRC validation failed")
                
            # Detailed analysis by message type
            if msg_type == "Ownship Position Report":
                self.analyze_position_report(packet, packet_analysis, is_ownship=True)
                analysis["summary"]["position_reports"].append(packet_analysis)
            elif msg_type == "Traffic Report":
                self.analyze_traffic_report(packet, packet_analysis)
                analysis["summary"]["traffic_reports"].append(packet_analysis)
            elif msg_type == "Heartbeat":
                self.analyze_heartbeat(packet, packet_analysis)
                analysis["summary"]["heartbeats"].append(packet_analysis)
                
            analysis["detailed_analysis"].append(packet_analysis)
            
        # Validate encoding accuracy
        self.validate_encoding_accuracy(analysis, test_cases)
        
        return analysis
        
    def analyze_position_report(self, packet: Dict[str, Any], analysis: Dict[str, Any], is_ownship: bool):
        """Analyze position report for correctness"""
        
        # Check coordinate validity
        lat = packet.get("latitude")
        lon = packet.get("longitude") 
        
        if lat is not None:
            if not (-90 <= lat <= 90):
                analysis["issues"].append(f"Invalid latitude: {lat}")
        else:
            analysis["issues"].append("Missing latitude")
            
        if lon is not None:
            if not (-180 <= lon <= 180):
                analysis["issues"].append(f"Invalid longitude: {lon}")
        else:
            analysis["issues"].append("Missing longitude")
            
        # Check altitude validity
        alt = packet.get("altitude")
        if alt is not None and alt != "Invalid":
            if not (-1000 <= alt <= 101350):  # GDL-90 valid range
                analysis["issues"].append(f"Altitude out of GDL-90 range: {alt}")
                
        # Check speed validity
        speed = packet.get("horizontal_velocity")
        if speed is not None and speed != "No data":
            if speed > 4094:  # GDL-90 max speed
                analysis["issues"].append(f"Speed exceeds GDL-90 maximum: {speed}")
                
        # Check callsign format
        callsign = packet.get("callsign", "")
        if len(callsign) > 10:  # Including quotes
            analysis["issues"].append(f"Callsign too long: {callsign}")
            
        analysis["coordinates"] = f"({lat:.6f}, {lon:.6f})" if lat and lon else "Invalid"
        analysis["altitude_ft"] = alt
        analysis["speed_knots"] = speed
        analysis["callsign"] = callsign
        
    def analyze_traffic_report(self, packet: Dict[str, Any], analysis: Dict[str, Any]):
        """Analyze traffic report for correctness"""
        self.analyze_position_report(packet, analysis, is_ownship=False)
        
        # Additional traffic-specific checks
        icao = packet.get("icao_address")
        if icao:
            analysis["icao_address"] = icao
            
    def analyze_heartbeat(self, packet: Dict[str, Any], analysis: Dict[str, Any]):
        """Analyze heartbeat message"""
        timestamp = packet.get("timestamp")
        msg_count = packet.get("message_count")
        
        analysis["utc_time"] = f"{packet.get('timestamp_hours', 0):02d}:{packet.get('timestamp_minutes', 0):02d}:{packet.get('timestamp_seconds', 0):02d}"
        analysis["message_count"] = msg_count
        
    def validate_encoding_accuracy(self, analysis: Dict[str, Any], test_cases: List[Dict[str, Any]]):
        """Validate encoding accuracy against known test cases"""
        
        encoding_tests = []
        
        # For each position/traffic report, validate encoding
        for report in analysis["summary"]["position_reports"] + analysis["summary"]["traffic_reports"]:
            if not report.get("crc_valid"):
                continue
                
            # Find matching test case (if any)
            # This is simplified - in practice you'd need more sophisticated matching
            validation = {
                "packet_number": report["packet_number"],
                "validation_results": {
                    "latitude_encoding": "Unknown", 
                    "longitude_encoding": "Unknown",
                    "altitude_encoding": "Unknown",
                    "speed_encoding": "Unknown"
                }
            }
            
            # Here you would compare against expected values from test cases
            # This requires either injecting known data or having reference values
            
            encoding_tests.append(validation)
            
        analysis["encoding_validation"] = encoding_tests
        
    def generate_report(self, analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Generate comprehensive test report"""
        
        report = {
            "test_info": {
                "timestamp": datetime.now().isoformat(),
                "test_suite_version": "1.0",
                "plugin_tested": "XP2GDL90"
            },
            "summary": analysis["summary"],
            "detailed_results": analysis["detailed_analysis"],
            "encoding_validation": analysis.get("encoding_validation", []),
            "recommendations": []
        }
        
        # Generate recommendations based on analysis
        summary = analysis["summary"]
        
        if summary["crc_errors"] > 0:
            pct_errors = (summary["crc_errors"] / summary["total_packets"]) * 100
            report["recommendations"].append(f"CRC error rate: {pct_errors:.1f}% - Check for transmission issues")
            
        if summary["total_packets"] == 0:
            report["recommendations"].append("No packets captured - Verify plugin is running and broadcasting")
            
        if "Traffic Report" not in summary["message_types"]:
            report["recommendations"].append("No traffic reports found - Enable AI traffic in X-Plane")
            
        if "Ownship Position Report" not in summary["message_types"]:
            report["recommendations"].append("No ownship position reports found - Check plugin functionality")
            
        # Save report
        with open("test_report.json", "w") as f:
            json.dump(report, f, indent=2)
            
        return report
        
    def manual_decode_test(self, hex_data: str):
        """Manually test decoding of specific hex data"""
        try:
            data = bytes.fromhex(hex_data.replace(" ", ""))
            decoder = GDL90Decoder()
            result = decoder.decode_frame(data)
            
            print("Manual decode test:")
            print(f"Input: {hex_data}")
            print(f"Result: {json.dumps(result, indent=2)}")
            
        except Exception as e:
            print(f"Error in manual decode test: {e}")

def main():
    if len(sys.argv) > 1:
        if sys.argv[1] == "--quick":
            # Quick test with short capture
            suite = GDL90TestSuite()
            suite.run_complete_test(capture_duration=10)
        elif sys.argv[1] == "--decode" and len(sys.argv) > 2:
            # Manual decode test
            suite = GDL90TestSuite()
            suite.manual_decode_test(sys.argv[2])
        elif sys.argv[1] == "--generate-only":
            # Just generate test cases
            generator = TestDataGenerator()
            generator.generate_test_cases()
            generator.print_test_summary()
            generator.save_test_cases()
        else:
            print("Usage:")
            print("  python3 run_gdl90_tests.py                    # Full test (30s capture)")
            print("  python3 run_gdl90_tests.py --quick            # Quick test (10s capture)")
            print("  python3 run_gdl90_tests.py --decode <hex>     # Decode hex data")
            print("  python3 run_gdl90_tests.py --generate-only    # Generate test cases only")
    else:
        # Full test
        suite = GDL90TestSuite()
        suite.run_complete_test(capture_duration=30)

if __name__ == "__main__":
    main()
