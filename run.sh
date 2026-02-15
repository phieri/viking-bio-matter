#!/bin/bash
# Viking Bio Matter Bridge Run Script
# This script helps manage the Viking Bio Matter device

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_header() {
    echo ""
    echo "======================================"
    echo "  Viking Bio Matter Bridge Manager"
    echo "======================================"
    echo ""
}

print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ $1${NC}"
}

# Function to find Pico device
find_pico() {
    local devices=$(ls /dev/ttyACM* 2>/dev/null || true)
    
    if [ -z "$devices" ]; then
        print_error "No Pico device found on /dev/ttyACM*"
        echo "Please connect your Pico and try again."
        return 1
    fi
    
    echo "$devices" | head -n 1
}

# Function to monitor device output
monitor() {
    print_header
    
    local device=$(find_pico)
    [ $? -ne 0 ] && exit 1
    
    print_info "Monitoring device: $device"
    print_info "Press Ctrl+C to exit"
    echo ""
    
    if command -v screen &> /dev/null; then
        screen "$device" 115200
    elif command -v minicom &> /dev/null; then
        minicom -D "$device" -b 115200
    else
        print_error "No serial terminal found. Please install 'screen' or 'minicom'"
        exit 1
    fi
}

# Function to test with simulator
test_simulator() {
    print_header
    
    if [ ! -f "$SCRIPT_DIR/examples/viking_bio_simulator.py" ]; then
        print_error "Simulator not found at $SCRIPT_DIR/examples/viking_bio_simulator.py"
        exit 1
    fi
    
    # Check for pyserial
    if ! python3 -c "import serial" &> /dev/null; then
        print_error "pyserial not installed"
        echo "Install with: pip3 install pyserial"
        exit 1
    fi
    
    # Find Viking Bio serial device
    print_info "Looking for Viking Bio serial device..."
    if [ ! -e "/dev/ttyUSB0" ]; then
        print_error "Viking Bio device not found at /dev/ttyUSB0"
        echo "Update the device path in the simulator call"
    fi
    
    print_success "Starting Viking Bio simulator"
    python3 "$SCRIPT_DIR/examples/viking_bio_simulator.py" /dev/ttyUSB0 "$@"
}

# Function to show Matter commissioning info
commissioning_info() {
    print_header
    
    echo "Matter Commissioning Information:"
    echo ""
    echo "Setup PIN Code: [Derived from device MAC]"
    echo "Discriminator:  [Random value 3840-4095, stored in flash]"
    echo ""
    echo "⚠️  NOTE: Discriminator is randomly generated on first boot"
    echo "         and persisted to flash storage."
    echo ""
    echo "To get actual commissioning values:"
    echo "  1. Flash firmware to device"
    echo "  2. Connect via serial monitor"
    echo "  3. Device will display actual PIN and discriminator"
    echo ""
    echo "Commission with chip-tool:"
    echo "  chip-tool pairing code 1 34970112332"
    echo ""
    print_info "WARNING: These are TEST credentials only!"
    echo ""
}

# Function to test Matter attributes
test_matter() {
    print_header
    
    if [ ! -f "third_party/connectedhomeip/out/host/chip-tool" ]; then
        print_error "chip-tool not found"
        echo "Build it with:"
        echo "  cd third_party/connectedhomeip"
        echo "  ./scripts/examples/gn_build_example.sh examples/chip-tool out/host"
        exit 1
    fi
    
    local CHIP_TOOL="$SCRIPT_DIR/third_party/connectedhomeip/out/host/chip-tool"
    local NODE_ID=${1:-1}
    local ENDPOINT=${2:-1}
    
    print_info "Testing Matter attributes on node $NODE_ID, endpoint $ENDPOINT"
    echo ""
    
    echo "Reading OnOff (flame status)..."
    $CHIP_TOOL onoff read on-off $NODE_ID $ENDPOINT
    echo ""
    
    echo "Reading LevelControl (fan speed)..."
    $CHIP_TOOL levelcontrol read current-level $NODE_ID $ENDPOINT
    echo ""
    
    echo "Reading TemperatureMeasurement (temperature)..."
    $CHIP_TOOL temperaturemeasurement read measured-value $NODE_ID $ENDPOINT
    echo ""
}

# Function to show usage
usage() {
    print_header
    
    echo "Usage: $0 <command> [options]"
    echo ""
    echo "Commands:"
    echo "  monitor              Monitor Pico device serial output"
    echo "  simulate             Run Viking Bio simulator"
    echo "  commissioning        Show Matter commissioning information"
    echo "  test-matter [node]   Test Matter attributes (default node: 1)"
    echo "  help                 Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 monitor                    # Watch device output"
    echo "  $0 simulate -p text           # Run simulator with text protocol"
    echo "  $0 commissioning              # Show setup codes"
    echo "  $0 test-matter 1              # Test Matter attributes on node 1"
    echo ""
}

# Main script
case "${1:-help}" in
    monitor)
        monitor
        ;;
    simulate)
        shift
        test_simulator "$@"
        ;;
    commissioning)
        commissioning_info
        ;;
    test-matter)
        test_matter "${2:-1}" "${3:-1}"
        ;;
    help|--help|-h)
        usage
        ;;
    *)
        print_error "Unknown command: $1"
        usage
        exit 1
        ;;
esac
