#!/bin/bash
# Example script to run the Viking Bio Matter Bridge on Raspberry Pi Zero

# Configuration
SERIAL_DEVICE="${SERIAL_DEVICE:-/dev/ttyUSB0}"
BAUD_RATE="${BAUD_RATE:-9600}"
SETUP_CODE="${SETUP_CODE:-20202021}"
BRIDGE_BINARY="${BRIDGE_BINARY:-./build_host/host_bridge/host_bridge}"

# Check if Matter SDK is available
if [ -z "$MATTER_ROOT" ]; then
    echo "WARNING: MATTER_ROOT environment variable is not set."
    echo "The bridge may run in stub mode without full Matter functionality."
    echo "To enable Matter, set MATTER_ROOT to your connectedhomeip checkout:"
    echo "  export MATTER_ROOT=/path/to/connectedhomeip"
    echo ""
fi

# Check if bridge binary exists
if [ ! -f "$BRIDGE_BINARY" ]; then
    echo "ERROR: Bridge binary not found at $BRIDGE_BINARY"
    echo "Please build the bridge first:"
    echo "  mkdir -p build_host && cd build_host"
    echo "  cmake .. -DENABLE_MATTER=ON"
    echo "  make host_bridge"
    exit 1
fi

# Check if serial device exists
if [ ! -e "$SERIAL_DEVICE" ]; then
    echo "WARNING: Serial device $SERIAL_DEVICE not found"
    echo "Available serial devices:"
    ls -l /dev/tty* 2>/dev/null | grep -E "(USB|AMA|ACM)" || echo "  None found"
    echo ""
    echo "Please specify the correct device:"
    echo "  SERIAL_DEVICE=/dev/ttyAMA0 $0"
    exit 1
fi

# Check serial device permissions
if [ ! -r "$SERIAL_DEVICE" ] || [ ! -w "$SERIAL_DEVICE" ]; then
    echo "WARNING: No read/write permission for $SERIAL_DEVICE"
    echo "You may need to add your user to the 'dialout' group:"
    echo "  sudo usermod -a -G dialout \$USER"
    echo "Then log out and back in."
    echo ""
fi

echo "==================================="
echo "Viking Bio Matter Bridge"
echo "==================================="
echo "Serial device: $SERIAL_DEVICE"
echo "Baud rate: $BAUD_RATE"
echo "Setup code: $SETUP_CODE"
echo "Matter SDK: ${MATTER_ROOT:-Not set (stub mode)}"
echo ""
echo "Starting bridge..."
echo "Press Ctrl+C to stop"
echo "==================================="
echo ""

# Run the bridge
exec "$BRIDGE_BINARY" \
    --device "$SERIAL_DEVICE" \
    --baud "$BAUD_RATE" \
    --setup-code "$SETUP_CODE"
