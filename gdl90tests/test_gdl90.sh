#!/bin/bash
"""
GDL-90 Testing Script
Convenient script to run various GDL-90 compliance tests

Usage:
    ./test_gdl90.sh compliance    # Run compliance tests
    ./test_gdl90.sh edge         # Run edge case tests
    ./test_gdl90.sh live         # Live capture and decode
    ./test_gdl90.sh decode <hex> # Decode specific hex message
"""

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}======================================${NC}"
}

check_python() {
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}Error: python3 is required but not installed${NC}"
        exit 1
    fi
}

run_compliance_tests() {
    print_header "Running GDL-90 Compliance Tests"
    echo -e "${YELLOW}This will capture live messages from XP2GDL90 for 10 seconds${NC}"
    echo -e "${YELLOW}Make sure X-Plane is running with the XP2GDL90 plugin loaded${NC}"
    echo ""
    read -p "Press Enter to continue..."
    
    python3 test_gdl90_compliance.py
}

run_edge_case_tests() {
    print_header "Running GDL-90 Edge Case Tests"
    python3 test_edge_cases.py
}

run_live_capture() {
    print_header "Live GDL-90 Message Capture"
    echo -e "${YELLOW}Starting live capture mode - Press Ctrl+C to stop${NC}"
    python3 test_edge_cases.py --live
}

decode_message() {
    if [ -z "$2" ]; then
        echo -e "${RED}Error: Hex string required${NC}"
        echo "Usage: $0 decode '7E 00 81 41 DB D0 08 02 B3 8B 7E'"
        exit 1
    fi
    
    print_header "Decoding GDL-90 Message"
    python3 gdl90_decoder.py "$2"
}

test_spec_example() {
    print_header "Testing GDL-90 Specification Example"
    echo "Testing the example from GDL-90 spec Section 2.2.4:"
    echo "Heartbeat message: [0x7E 0x00 0x81 0x41 0xDB 0xD0 0x08 0x02 0xB3 0x8B 0x7E]"
    echo ""
    
    python3 gdl90_decoder.py "7E 00 81 41 DB D0 08 02 B3 8B 7E"
}

show_help() {
    echo "GDL-90 Testing Script"
    echo ""
    echo "Commands:"
    echo "  compliance  - Run full compliance test suite"
    echo "  edge        - Run edge case tests" 
    echo "  live        - Live message capture and decode"
    echo "  decode <hex> - Decode specific hex message"
    echo "  spec        - Test the specification example message"
    echo "  help        - Show this help"
    echo ""
    echo "Examples:"
    echo "  ./test_gdl90.sh compliance"
    echo "  ./test_gdl90.sh decode '7E 00 81 41 DB D0 08 02 B3 8B 7E'"
    echo "  ./test_gdl90.sh live"
}

# Main script logic
check_python

case "${1:-help}" in
    "compliance")
        run_compliance_tests
        ;;
    "edge")
        run_edge_case_tests
        ;;
    "live")
        run_live_capture
        ;;
    "decode")
        decode_message "$@"
        ;;
    "spec")
        test_spec_example
        ;;
    "help"|*)
        show_help
        ;;
esac
