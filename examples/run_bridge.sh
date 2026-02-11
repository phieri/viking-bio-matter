#!/bin/bash
#
# Example script to run the Viking Bio Matter Bridge
#
# This script demonstrates how to start the host bridge with
# custom configuration. Adjust paths and parameters as needed.
#

set -e

# Configuration
SERIAL_DEVICE="${SERIAL_DEVICE:-/dev/ttyUSB0}"
SETUP_CODE="${SETUP_CODE:-20202021}"
DISCRIMINATOR="${DISCRIMINATOR:-3840}"
BRIDGE_PATH="./build_host/host_bridge"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "Viking Bio Matter Bridge Runner"
echo "======================================"
echo ""

# Check if bridge executable exists
if [ ! -f "$BRIDGE_PATH" ]; then
    echo -e "${RED}Error: Bridge executable not found at $BRIDGE_PATH${NC}"
    echo ""
    echo "Build the bridge first:"
    echo "  mkdir -p build_host"
    echo "  cd build_host"
    echo "  cmake .. -DENABLE_MATTER=ON"
    echo "  make host_bridge"
    echo ""
    exit 1
fi

# Check if serial device exists
if [ ! -e "$SERIAL_DEVICE" ]; then
    echo -e "${YELLOW}Warning: Serial device $SERIAL_DEVICE not found${NC}"
    echo ""
    echo "Available serial devices:"
    ls -l /dev/tty{USB,AMA,S}* 2>/dev/null || echo "  None found"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check serial port permissions
if [ -e "$SERIAL_DEVICE" ] && [ ! -r "$SERIAL_DEVICE" ]; then
    echo -e "${YELLOW}Warning: No read permission for $SERIAL_DEVICE${NC}"
    echo ""
    echo "Grant permission with:"
    echo "  sudo usermod -a -G dialout \$USER"
    echo "  (then log out and back in)"
    echo ""
    echo "Or run with sudo (not recommended):"
    echo "  sudo $0"
    echo ""
    exit 1
fi

# Check if MATTER_ROOT is set (for non-stub builds)
if [ -z "$MATTER_ROOT" ] && [ -z "$CHIP_ROOT" ]; then
    echo -e "${YELLOW}Warning: MATTER_ROOT not set${NC}"
    echo "If bridge was built with ENABLE_MATTER=ON, set:"
    echo "  export MATTER_ROOT=/path/to/connectedhomeip"
    echo ""
fi

# Print configuration
echo "Configuration:"
echo "  Serial Device: $SERIAL_DEVICE"
echo "  Setup Code: $SETUP_CODE"
echo "  Discriminator: $DISCRIMINATOR"
echo ""

# Check if another instance is running
if pgrep -x "host_bridge" > /dev/null; then
    echo -e "${YELLOW}Warning: Another instance of host_bridge is running${NC}"
    echo ""
    ps aux | grep host_bridge | grep -v grep
    echo ""
    read -p "Kill existing instance and continue? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        pkill -x host_bridge
        sleep 2
    else
        exit 1
    fi
fi

# Start bridge
echo -e "${GREEN}Starting bridge...${NC}"
echo ""

exec "$BRIDGE_PATH" \
    --device "$SERIAL_DEVICE" \
    --setup-code "$SETUP_CODE" \
    --discriminator "$DISCRIMINATOR"
