#!/usr/bin/env python3
"""
GDL-90 Integration Test Suite

Comprehensive test of XP2GDL90 plugin against GDL-90 specification.
This script performs automated validation of the plugin's output.

Usage:
    python gdl90_integration_test.py [--duration 60] [--port 4000]
"""

import argparse
import socket
import time
import threading
import queue
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
from collections import defaultdict
import statistics

from gdl90_format_tests import GDL90Validator
from gdl90_unit_tests import TestGDL90Functions, TestCallsignValidation
import unittest

@dataclass
class TestResult:
    """Result of a single test."""
    name: str
    passed: bool
    message: str
    duration: float = 0.0
    details: Optional[Dict[str, Any]] = None

class GDL90IntegrationTester:
    """Comprehensive integration test for GDL-90 implementation."""
    
    def __init__(self, ip: str = "127.0.0.1", port: int = 4000):
        self.ip = ip
        self.port = port
        self.validator = GDL90Validator()
        self.results: List[TestResult] = []
        self.captured_messages: List[Dict[str, Any]] = []
        self.message_queue = queue.Queue()
        self.capture_thread: Optional[threading.Thread] = None
        self.stop_capture = threading.Event()
        
    def add_result(self, result: TestResult):
        """Add a test result."""
        self.results.append(result)
        status = "PASS" if result.passed else "FAIL"
        print(f"  {status}: {result.name} ({result.duration:.3f}s)")
        if not result.passed:
            print(f"    {result.message}")
    
    def start_message_capture(self):
        """Start capturing messages in background thread."""
        def capture_worker():
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                sock.bind((self.ip, self.port))
                sock.settimeout(1.0)
                
                while not self.stop_capture.is_set():
                    try:
                        data, addr = sock.recvfrom(1024)
                        timestamp = time.time()
                        self.message_queue.put((timestamp, data, addr))
                    except socket.timeout:
                        continue
                    except Exception as e:
                        if not self.stop_capture.is_set():
                            print(f"Capture error: {e}")
                        break
                        
                sock.close()
                
            except Exception as e:
                print(f"Failed to start message capture: {e}")
        
        self.capture_thread = threading.Thread(target=capture_worker, daemon=True)
        self.capture_thread.start()
        time.sleep(0.5)  # Give thread time to start
    
    def stop_message_capture(self):
        """Stop message capture and process captured messages."""
        if self.capture_thread:
            self.stop_capture.set()
            self.capture_thread.join(timeout=2.0)
        
        # Process captured messages
        while not self.message_queue.empty():
            try:
                timestamp, raw_data, addr = self.message_queue.get_nowait()
                
                is_valid, error_msg, parsed = self.validator.validate_message_structure(raw_data)
                
                message_info = {
                    'timestamp': timestamp,
                    'raw_data': raw_data,
                    'source_addr': addr,
                    'valid': is_valid,
                    'error': error_msg if not is_valid else None,
                    'parsed': parsed
                }
                
                self.captured_messages.append(message_info)
                
            except queue.Empty:
                break
    
    def test_basic_connectivity(self) -> TestResult:
        """Test basic UDP connectivity to plugin."""
        start_time = time.time()
        
        try:
            # Try to bind to the port to check if it's available
            test_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            test_sock.settimeout(5.0)
            
            try:
                test_sock.bind((self.ip, self.port))
                test_sock.close()
                return TestResult(
                    "Basic Connectivity",
                    False,
                    f"No service listening on {self.ip}:{self.port}. Is XP2GDL90 plugin running?",
                    time.time() - start_time
                )
            except OSError:
                # Port is in use (good - plugin is likely running)
                test_sock.close()
                return TestResult(
                    "Basic Connectivity", 
                    True,
                    f"Service detected on {self.ip}:{self.port}",
                    time.time() - start_time
                )
                
        except Exception as e:
            return TestResult(
                "Basic Connectivity",
                False,
                f"Connection test failed: {e}",
                time.time() - start_time
            )
    
    def test_message_reception(self, duration: float = 10.0) -> TestResult:
        """Test that we can receive GDL-90 messages."""
        start_time = time.time()
        
        initial_count = len(self.captured_messages)
        
        # Wait for messages to be captured
        end_time = start_time + duration
        while time.time() < end_time:
            time.sleep(0.1)
            if len(self.captured_messages) > initial_count:
                break
        
        messages_received = len(self.captured_messages) - initial_count
        
        if messages_received > 0:
            return TestResult(
                "Message Reception",
                True,
                f"Received {messages_received} messages in {duration}s",
                time.time() - start_time,
                {"messages_received": messages_received}
            )
        else:
            return TestResult(
                "Message Reception",
                False,
                f"No messages received in {duration}s",
                time.time() - start_time
            )
    
    def test_message_validity(self) -> TestResult:
        """Test that received messages are valid GDL-90 format."""
        start_time = time.time()
        
        if not self.captured_messages:
            return TestResult(
                "Message Validity",
                False,
                "No messages to validate",
                time.time() - start_time
            )
        
        valid_count = sum(1 for msg in self.captured_messages if msg['valid'])
        total_count = len(self.captured_messages)
        validity_rate = (valid_count / total_count) * 100
        
        passed = validity_rate >= 95.0  # At least 95% should be valid
        
        return TestResult(
            "Message Validity",
            passed,
            f"{valid_count}/{total_count} messages valid ({validity_rate:.1f}%)",
            time.time() - start_time,
            {
                "valid_count": valid_count,
                "total_count": total_count,
                "validity_rate": validity_rate
            }
        )
    
    def test_message_types(self) -> TestResult:
        """Test that expected message types are present."""
        start_time = time.time()
        
        if not self.captured_messages:
            return TestResult(
                "Message Types",
                False,
                "No messages to analyze",
                time.time() - start_time
            )
        
        type_counts = defaultdict(int)
        for msg in self.captured_messages:
            if msg['valid'] and msg['parsed']:
                msg_id = msg['parsed']['message_id']
                type_counts[msg_id] += 1
        
        # Check for expected message types
        expected_types = {
            0x00: "Heartbeat",
            0x0A: "Ownship Report"
        }
        
        missing_types = []
        for msg_id, name in expected_types.items():
            if msg_id not in type_counts:
                missing_types.append(name)
        
        if missing_types:
            return TestResult(
                "Message Types",
                False,
                f"Missing expected message types: {', '.join(missing_types)}",
                time.time() - start_time,
                {"type_counts": dict(type_counts)}
            )
        else:
            return TestResult(
                "Message Types",
                True,
                f"All expected message types present",
                time.time() - start_time,
                {"type_counts": dict(type_counts)}
            )
    
    def test_heartbeat_timing(self) -> TestResult:
        """Test that heartbeats are sent at correct intervals."""
        start_time = time.time()
        
        # Find heartbeat messages
        heartbeats = []
        for msg in self.captured_messages:
            if (msg['valid'] and msg['parsed'] and 
                msg['parsed']['message_id'] == 0x00):
                heartbeats.append(msg['timestamp'])
        
        if len(heartbeats) < 2:
            return TestResult(
                "Heartbeat Timing",
                False,
                f"Need at least 2 heartbeats, got {len(heartbeats)}",
                time.time() - start_time
            )
        
        # Calculate intervals
        intervals = []
        for i in range(1, len(heartbeats)):
            interval = heartbeats[i] - heartbeats[i-1]
            intervals.append(interval)
        
        mean_interval = statistics.mean(intervals)
        std_interval = statistics.stdev(intervals) if len(intervals) > 1 else 0
        
        # Should be approximately 1 second (±0.1s tolerance)
        expected_interval = 1.0
        tolerance = 0.1
        
        timing_ok = abs(mean_interval - expected_interval) <= tolerance
        
        return TestResult(
            "Heartbeat Timing",
            timing_ok,
            f"Mean interval: {mean_interval:.3f}s (±{std_interval:.3f}s), expected: {expected_interval}s",
            time.time() - start_time,
            {
                "mean_interval": mean_interval,
                "std_interval": std_interval,
                "interval_count": len(intervals)
            }
        )
    
    def test_coordinate_encoding(self) -> TestResult:
        """Test coordinate encoding in position reports."""
        start_time = time.time()
        
        # Find position reports (ownship or traffic)
        position_reports = []
        for msg in self.captured_messages:
            if (msg['valid'] and msg['parsed'] and 
                msg['parsed']['message_id'] in [0x0A, 0x14]):
                position_reports.append(msg)
        
        if not position_reports:
            return TestResult(
                "Coordinate Encoding",
                False,
                "No position reports found",
                time.time() - start_time
            )
        
        # Check that coordinates are encoded properly
        issues = []
        for msg in position_reports[:5]:  # Check first 5 messages
            data = msg['parsed']['data']
            if len(data) >= 10:
                # Extract and validate coordinate encoding
                lat_raw = int.from_bytes(data[4:7], 'big')
                lon_raw = int.from_bytes(data[7:10], 'big')
                
                # Check if coordinates are in valid range
                if lat_raw > 0xFFFFFF or lon_raw > 0xFFFFFF:
                    issues.append(f"Invalid coordinate encoding: lat=0x{lat_raw:06X}, lon=0x{lon_raw:06X}")
        
        if issues:
            return TestResult(
                "Coordinate Encoding",
                False,
                f"Encoding issues: {'; '.join(issues)}",
                time.time() - start_time
            )
        else:
            return TestResult(
                "Coordinate Encoding",
                True,
                f"Coordinate encoding valid in {len(position_reports)} position reports",
                time.time() - start_time
            )
    
    def run_unit_tests(self) -> TestResult:
        """Run the unit test suite."""
        start_time = time.time()
        
        # Create a test suite
        suite = unittest.TestSuite()
        suite.addTest(unittest.makeSuite(TestGDL90Functions))
        suite.addTest(unittest.makeSuite(TestCallsignValidation))
        
        # Run tests with custom result collector
        result = unittest.TestResult()
        suite.run(result)
        
        total_tests = result.testsRun
        failures = len(result.failures)
        errors = len(result.errors)
        passed_tests = total_tests - failures - errors
        
        success_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0
        
        all_passed = failures == 0 and errors == 0
        
        message = f"{passed_tests}/{total_tests} unit tests passed ({success_rate:.1f}%)"
        if failures > 0:
            message += f", {failures} failures"
        if errors > 0:
            message += f", {errors} errors"
        
        return TestResult(
            "Unit Tests",
            all_passed,
            message,
            time.time() - start_time,
            {
                "total_tests": total_tests,
                "passed": passed_tests,
                "failures": failures,
                "errors": errors
            }
        )
    
    def run_comprehensive_test(self, duration: float = 30.0) -> Dict[str, Any]:
        """Run comprehensive test suite."""
        print("Starting GDL-90 Comprehensive Test Suite")
        print("=" * 60)
        
        overall_start = time.time()
        
        # 1. Run unit tests first (don't need network)
        print("\n1. Running Unit Tests...")
        self.add_result(self.run_unit_tests())
        
        # 2. Test basic connectivity
        print("\n2. Testing Basic Connectivity...")
        connectivity_result = self.test_basic_connectivity()
        self.add_result(connectivity_result)
        
        if not connectivity_result.passed:
            print("\nSkipping network tests due to connectivity failure.")
        else:
            # 3. Start message capture
            print(f"\n3. Starting Message Capture ({duration}s)...")
            self.start_message_capture()
            
            # 4. Test message reception
            print("\n4. Testing Message Reception...")
            self.add_result(self.test_message_reception(duration * 0.3))
            
            # Continue capturing for full duration
            remaining_time = duration * 0.7
            if remaining_time > 0:
                print(f"\n5. Continuing capture for {remaining_time:.1f}s...")
                time.sleep(remaining_time)
            
            # 6. Stop capture and analyze
            print("\n6. Analyzing Captured Messages...")
            self.stop_message_capture()
            
            self.add_result(self.test_message_validity())
            self.add_result(self.test_message_types())
            self.add_result(self.test_heartbeat_timing())
            self.add_result(self.test_coordinate_encoding())
        
        # Generate summary report
        total_duration = time.time() - overall_start
        passed_tests = sum(1 for r in self.results if r.passed)
        total_tests = len(self.results)
        
        print("\n" + "=" * 60)
        print("TEST SUMMARY")
        print("=" * 60)
        
        for result in self.results:
            status = "PASS" if result.passed else "FAIL"
            print(f"{status:4s} | {result.name:25s} | {result.message}")
        
        overall_success = passed_tests == total_tests
        success_rate = (passed_tests / total_tests) * 100
        
        print(f"\nOverall Result: {passed_tests}/{total_tests} tests passed ({success_rate:.1f}%)")
        print(f"Total Duration: {total_duration:.2f}s")
        print(f"Messages Captured: {len(self.captured_messages)}")
        
        if overall_success:
            print("\n✅ GDL-90 Implementation PASSED all tests!")
        else:
            print("\n❌ GDL-90 Implementation FAILED some tests.")
        
        return {
            "overall_success": overall_success,
            "passed_tests": passed_tests,
            "total_tests": total_tests,
            "success_rate": success_rate,
            "duration": total_duration,
            "messages_captured": len(self.captured_messages),
            "results": [
                {
                    "name": r.name,
                    "passed": r.passed,
                    "message": r.message,
                    "duration": r.duration,
                    "details": r.details
                }
                for r in self.results
            ]
        }


def main():
    parser = argparse.ArgumentParser(description="GDL-90 Integration Test Suite")
    parser.add_argument("--ip", default="127.0.0.1", help="IP address to test")
    parser.add_argument("--port", type=int, default=4000, help="UDP port to test")
    parser.add_argument("--duration", type=float, default=30.0, help="Test duration in seconds")
    parser.add_argument("--output", help="Save detailed results to JSON file")
    
    args = parser.parse_args()
    
    print("GDL-90 Integration Test Suite")
    print("Testing XP2GDL90 Plugin Implementation")
    print(f"Target: {args.ip}:{args.port}")
    print(f"Duration: {args.duration}s")
    
    tester = GDL90IntegrationTester(args.ip, args.port)
    results = tester.run_comprehensive_test(args.duration)
    
    if args.output:
        import json
        with open(args.output, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"\nDetailed results saved to: {args.output}")
    
    # Exit with appropriate code
    exit(0 if results["overall_success"] else 1)


if __name__ == "__main__":
    main()
