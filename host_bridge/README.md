# Viking Bio Matter Bridge - Raspberry Pi Zero Host

This directory contains a host-based Matter bridge implementation that runs on Raspberry Pi Zero (or any Linux system) and exposes Viking Bio 20 burner data over Matter/WiFi.

## Overview

The host bridge:
- Runs as a Linux user-space application on Raspberry Pi Zero (Raspbian / Raspberry Pi OS)
- Reads Viking Bio serial data from a configured serial device (e.g., `/dev/ttyUSB0` or `/dev/ttyAMA0`)
- Parses the Viking Bio protocol (binary and text formats)
- Maps parsed data to Matter clusters/attributes:
  - **On/Off cluster (0x0006)**: Flame state (On = flame detected)
  - **Level Control cluster (0x0008)**: Fan speed (0-100 mapped to CurrentLevel)
  - **Temperature Measurement cluster (0x0402)**: Measured temperature
- Supports Matter commissioning (QR code / setup code) for pairing with controllers
- Sends attribute reports when values change

## Prerequisites

### 1. Hardware

- Raspberry Pi Zero, Zero W, or any Raspberry Pi model
- USB-to-serial adapter OR direct UART connection from Viking Bio 20
- Viking Bio 20 burner with TTL serial output

### 2. Software Dependencies

```bash
# Update system
sudo apt-get update
sudo apt-get upgrade

# Install build tools
sudo apt-get install -y git cmake build-essential \
    pkg-config libssl-dev libglib2.0-dev \
    libavahi-client-dev libavahi-common-dev \
    python3-pip ninja-build

# Install additional dependencies for Matter SDK
sudo apt-get install -y autoconf automake libtool \
    libdbus-1-dev libgirepository1.0-dev

# Install Python dependencies
pip3 install --user cryptography
```

## Building

### Step 1: Clone and Build Matter SDK

The host bridge requires the [Project CHIP / connectedhomeip](https://github.com/project-chip/connectedhomeip) SDK.

**Recommended version**: v1.2.0.1 or later stable release

```bash
# Clone Matter SDK
cd ~
git clone https://github.com/project-chip/connectedhomeip.git
cd connectedhomeip

# Checkout stable release (recommended)
git checkout v1.2.0.1

# Initialize submodules (this may take a while)
git submodule update --init --depth 1

# Activate the Matter environment
source scripts/activate.sh

# Build for Linux host (this will take 15-60 minutes on Raspberry Pi Zero)
# For Raspberry Pi Zero, use a smaller job count to avoid memory issues
gn gen out/host
ninja -C out/host -j 1

# On faster systems, you can use more jobs:
# ninja -C out/host -j 4
```

**Note for Raspberry Pi Zero users**: Building the Matter SDK on a Pi Zero can take over an hour due to limited resources. Consider cross-compiling on a more powerful machine or using a pre-built SDK if available.

### Step 2: Set Environment Variable

```bash
# Add to your ~/.bashrc or ~/.profile
export MATTER_ROOT=~/connectedhomeip

# Or set for current session
export MATTER_ROOT=~/connectedhomeip
```

### Step 3: Build the Host Bridge

```bash
# Navigate to the viking-bio-matter repository
cd /path/to/viking-bio-matter

# Create build directory
mkdir -p build_host
cd build_host

# Configure with Matter support
cmake .. -DENABLE_MATTER=ON

# Build
make -j$(nproc)

# The executable will be at: build_host/host_bridge/host_bridge
```

## Configuration

### Serial Device

The host bridge reads from a serial device. Common devices:

- **USB-to-serial adapter**: `/dev/ttyUSB0` (most common)
- **Raspberry Pi UART**: `/dev/ttyAMA0` or `/dev/serial0`
- **Bluetooth serial**: `/dev/rfcomm0`

To identify your serial device:

```bash
# List available serial devices
ls -l /dev/tty*

# Check for USB serial devices
dmesg | grep tty

# Monitor serial port (to test Viking Bio connection)
sudo cat /dev/ttyUSB0
```

### Permissions

Add your user to the `dialout` group to access serial ports without sudo:

```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

## Running the Bridge

### Basic Usage

```bash
# Run with default settings (device: /dev/ttyUSB0, baud: 9600)
./host_bridge

# Specify serial device
./host_bridge --device /dev/ttyAMA0

# Specify custom setup code for commissioning
./host_bridge --device /dev/ttyUSB0 --setup-code 34567890

# Verbose mode
./host_bridge --device /dev/ttyUSB0 --verbose

# Show help
./host_bridge --help
```

### Command-Line Options

```
Options:
  -d, --device <path>      Serial device path (default: /dev/ttyUSB0)
  -b, --baud <rate>        Baud rate (default: 9600)
  -s, --setup-code <code>  Matter setup code (default: 20202021)
  -v, --verbose            Verbose output
  -h, --help               Show help message
```

## Commissioning

### Using chip-tool (Command Line)

The Matter SDK includes `chip-tool`, a command-line Matter controller:

```bash
# Navigate to Matter SDK
cd ~/connectedhomeip

# Commission the bridge (replace with your setup code)
./out/host/chip-tool pairing code <node-id> 20202021

# Example with node ID 1
./out/host/chip-tool pairing code 1 20202021

# Read on/off state (flame detection)
./out/host/chip-tool onoff read on-off 1 1

# Read current level (fan speed)
./out/host/chip-tool levelcontrol read current-level 1 1

# Read temperature
./out/host/chip-tool temperaturemeasurement read measured-value 1 1
```

### Using Mobile Apps

You can also commission the bridge using Matter-compatible mobile apps:

- **Google Home** (Android/iOS)
- **Apple Home** (iOS)
- **Amazon Alexa** (Android/iOS)
- **Samsung SmartThings** (Android/iOS)

Steps:
1. Start the host bridge
2. Open your Matter-compatible app
3. Add a new device
4. Select "Matter" or scan QR code
5. Enter the setup code: `20202021` (or your custom code)

### Generating QR Code

To generate a QR code for easier commissioning:

```bash
# Using Matter SDK's qr code generator
cd ~/connectedhomeip
./out/host/chip-tool payload generate-qr-code 20202021

# Or use Python helper (if available)
python3 ../examples/generate_qr_code.py 20202021
```

### Security Note

**Important**: The default setup code `20202021` is for testing only. For production deployments, use a secure, randomly generated setup code.

## Running as a Service

To run the host bridge automatically on boot, use the provided systemd service file.

### Installation

```bash
# Copy the service file
sudo cp host_bridge.service /etc/systemd/system/

# Edit the service file to match your setup
sudo nano /etc/systemd/system/host_bridge.service

# Update these lines in the file:
#   ExecStart=/path/to/host_bridge --device /dev/ttyUSB0
#   User=pi  (or your username)
#   Environment="MATTER_ROOT=/home/pi/connectedhomeip"

# Reload systemd
sudo systemctl daemon-reload

# Enable the service to start on boot
sudo systemctl enable host_bridge.service

# Start the service now
sudo systemctl start host_bridge.service

# Check status
sudo systemctl status host_bridge.service

# View logs
sudo journalctl -u host_bridge.service -f
```

## Troubleshooting

### Serial Port Issues

**Problem**: `Failed to open serial device`

**Solutions**:
- Check device exists: `ls -l /dev/ttyUSB0`
- Check permissions: Add user to `dialout` group
- Check device is not in use: `lsof /dev/ttyUSB0`
- Try with sudo: `sudo ./host_bridge`

### Matter SDK Build Issues

**Problem**: Out of memory during build

**Solutions**:
- Use fewer jobs: `ninja -C out/host -j 1`
- Increase swap space on Raspberry Pi
- Cross-compile on a more powerful machine

**Problem**: Missing dependencies

**Solutions**:
- Re-run the dependency installation commands
- Ensure Python 3.6+ is installed
- Check Matter SDK documentation for your platform

### Bridge Not Advertising

**Problem**: Bridge starts but cannot be commissioned

**Solutions**:
- Check WiFi is connected and working
- Ensure mDNS/Avahi is running: `sudo systemctl status avahi-daemon`
- Check firewall rules (UDP port 5540)
- Verify Matter SDK built successfully

### Data Not Updating

**Problem**: Bridge runs but attributes don't update

**Solutions**:
- Check serial connection to Viking Bio
- Monitor serial data: `cat /dev/ttyUSB0`
- Run with `--verbose` flag
- Test with Viking Bio simulator: `python3 ../examples/viking_bio_simulator.py`

## Development

### Building Without Matter SDK

For development and testing, you can build a stub version without the Matter SDK:

```bash
# Build without ENABLE_MATTER flag (stub mode)
mkdir build_stub
cd build_stub
cmake ..
make
```

The stub version will compile and run but won't provide actual Matter functionality. It will print debug messages showing what would be sent to Matter.

### Testing with Simulator

Use the Viking Bio simulator to test without hardware:

```bash
# In terminal 1: Start the bridge with a virtual serial port
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the device paths, e.g., /dev/pts/2 and /dev/pts/3

./host_bridge --device /dev/pts/2

# In terminal 2: Send test data
cd examples
python3 viking_bio_simulator.py /dev/pts/3
```

## Architecture

```
Viking Bio 20
    ↓ (TTL Serial 9600 baud)
USB-Serial Adapter
    ↓ (/dev/ttyUSB0)
Host Bridge (Linux)
    ├─ Serial Reader (POSIX termios)
    ├─ Protocol Parser (viking_bio_protocol_linux.c)
    └─ Matter Bridge (matter_bridge.cpp)
        ↓ (Matter over WiFi)
Matter Controller
```

### Source Files

- **main.cpp**: Entry point, CLI parsing, serial port handling
- **matter_bridge.h/cpp**: Matter stack initialization and cluster management
- **viking_bio_protocol.h**: Protocol definitions (shared with Pico firmware)
- **viking_bio_protocol_linux.c**: Protocol parser (ported from Pico version)
- **CMakeLists.txt**: Build configuration with Matter SDK integration

## Differences from Pico Firmware

The host bridge differs from the Pico firmware in several ways:

1. **Platform**: Runs on Linux (Raspberry Pi) instead of Pico microcontroller
2. **Serial**: Uses POSIX termios instead of Pico SDK UART
3. **Matter**: Uses full Matter SDK instead of stub/placeholder
4. **Threading**: Matter SDK runs its event loop in a separate thread
5. **Commissioning**: Full commissioning support with persistent storage

## References

- [Project CHIP / connectedhomeip](https://github.com/project-chip/connectedhomeip)
- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Documentation](https://www.raspberrypi.com/documentation/)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)

## License

This project is open source and available under the MIT License.
