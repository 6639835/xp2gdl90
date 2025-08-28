#!/usr/bin/env python3
"""
Complete Pipeline Test
Tests mock sender -> UDP capture -> GDL-90 decoding -> verification
"""

import subprocess
import time
import threading
import os
import signal
import sys

def run_capture():
    """Run UDP capture in a separate thread"""
    print("Starting UDP capture...")
    try:
        result = subprocess.run(
            [sys.executable, "test_udp_capture.py", "8"],
            capture_output=True,
            text=True,
            timeout=10
        )
        print("Capture completed:")
        print(result.stdout)
        if result.stderr:
            print("Capture errors:")
            print(result.stderr)
    except subprocess.TimeoutExpired:
        print("Capture timed out")
    except Exception as e:
        print(f"Capture error: {e}")

def run_sender():
    """Run mock sender"""
    print("Starting mock sender...")
    time.sleep(1)  # Let capture start first
    try:
        result = subprocess.run(
            [sys.executable, "mock_gdl90_sender.py", "6"],
            capture_output=True,
            text=True,
            timeout=10
        )
        print("Sender completed:")
        print(result.stdout)
        if result.stderr:
            print("Sender errors:")
            print(result.stderr)
    except subprocess.TimeoutExpired:
        print("Sender timed out")
    except Exception as e:
        print(f"Sender error: {e}")

def test_decoder():
    """Test the decoder with captured data"""
    print("\nTesting decoder...")
    if os.path.exists("captured_gdl90.bin"):
        try:
            result = subprocess.run(
                [sys.executable, "gdl90_decoder.py", "captured_gdl90.bin"],
                capture_output=True,
                text=True,
                timeout=10
            )
            print("Decoder results:")
            print(result.stdout)
            if result.stderr:
                print("Decoder errors:")
                print(result.stderr)
        except Exception as e:
            print(f"Decoder error: {e}")
    else:
        print("No captured data file found")

def main():
    print("=== Complete GDL-90 Pipeline Test ===\n")
    
    # Start capture in background thread
    capture_thread = threading.Thread(target=run_capture)
    capture_thread.start()
    
    # Start sender
    run_sender()
    
    # Wait for capture to complete
    capture_thread.join()
    
    # Test decoder
    test_decoder()
    
    print("\n=== Pipeline Test Complete ===")

if __name__ == "__main__":
    main()
