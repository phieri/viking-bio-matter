#!/bin/bash
#
# Example script to run the Viking Bio Matter host bridge
# 
# Prerequisites:
# 1. Build the host bridge with: cmake -DENABLE_MATTER=ON .. && make
# 2. Set MATTER_ROOT environment variable
# 3. Ensure your user is in the 'dialout' group for serial access
#

set -e

# Configuration
SERIAL_DEVICE="${SERIAL_DEVICE:-/dev/ttyUSB0}"
BAUD_RATE="${BAUD_RATE:-9600}"
SETUP_CODE="${SETUP_CODE:-20202021}"

# Check if host_bridge executable exists
if [ ! -f "./host_bridge/host_bridge" ] && [ ! -f "./build_host/host_bridge/host_bridge" ]; then
    echo "Error: host_bridge executable not found!"
    echo "Please build the host bridge first:"
    echo "  mkdir build_host && cd build_host"
    echo "  cmake .. -DENABLE_MATTER=ON"
    echo "  make"
    exit 1
fi

# Find the executable
if [ -f "./host_bridge/host_bridge" ]; then
    EXECUTABLE="./host_bridge/host_bridge"
elif [ -f "./build_host/host_bridge/host_bridge" ]; then
    EXECUTABLE="./build_host/host_bridge/host_bridge"
fi

# Check if MATTER_ROOT is set
if [ -z "$MATTER_ROOT" ]; then
    echo "Warning: MATTER_ROOT environment variable not set"
    echo "Matter functionality may not work properly"
    echo "Set it with: export MATTER_ROOT=/path/to/connectedhomeip"
fi

# Check if serial device exists
if [ ! -e "$SERIAL_DEVICE" ]; then
    echo "Error: Serial device $SERIAL_DEVICE not found!"
    echo "Available serial devices:"
    ls -l /dev/tty* 2>/dev/null | grep -E "(USB|AMA|ACM)" || echo "  None found"
    exit 1
fi

# Check if user has access to serial device
if [ ! -r "$SERIAL_DEVICE" ] || [ ! -w "$SERIAL_DEVICE" ]; then
    echo "Error: No read/write access to $SERIAL_DEVICE"
    echo "Solutions:"
    echo "  1. Add your user to the dialout group: sudo usermod -a -G dialout $USER"
    echo "  2. Run with sudo: sudo $0"
    exit 1
fi

echo "=== Viking Bio Matter Bridge ==="
echo "Executable: $EXECUTABLE"
echo "Serial device: $SERIAL_DEVICE"
echo "Baud rate: $BAUD_RATE"
echo "Setup code: $SETUP_CODE"
echo ""
echo "Starting bridge..."
echo "Press Ctrl+C to stop"
echo ""

# Run the host bridge
exec "$EXECUTABLE" \
    --device "$SERIAL_DEVICE" \
    --baud "$BAUD_RATE" \
    --setup-code "$SETUP_CODE" \
    "$@"
