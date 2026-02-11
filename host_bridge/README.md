# Viking Bio Matter Bridge - Raspberry Pi Zero Host Application

This is a Linux host-side Matter bridge that runs on Raspberry Pi Zero and exposes Viking Bio 20 burner data over Matter/WiFi.

## Overview

The host bridge reads serial data from a Viking Bio 20 burner and exposes it through the Matter (Project CHIP) protocol, allowing integration with Matter-compatible smart home controllers (e.g., Apple Home, Google Home, Amazon Alexa with Matter support).

### Supported Matter Clusters

The bridge implements the following Matter clusters for the burner:

1. **On/Off Cluster (0x0006)** - Flame detection state
   - `OnOff` attribute: `true` when flame is detected, `false` otherwise

2. **Level Control Cluster (0x0008)** - Fan speed
   - `CurrentLevel` attribute: 0-254 (mapped from 0-100% fan speed)

3. **Temperature Measurement Cluster (0x0402)** - Burner temperature
   - `MeasuredValue` attribute: Temperature in 0.01°C units

## Prerequisites

### Hardware Requirements

- Raspberry Pi Zero or Zero W (Raspbian/Raspberry Pi OS)
- USB-to-TTL serial adapter or direct TTL serial connection to Viking Bio 20
- WiFi connectivity (built-in on Zero W, or USB WiFi adapter for Zero)

### Software Requirements

1. **Build Tools**
   ```bash
   sudo apt-get update
   sudo apt-get install -y build-essential cmake git pkg-config \
       libssl-dev libglib2.0-dev libavahi-client-dev libdbus-1-dev
   ```

2. **Matter SDK (connectedhomeip)**
   
   The bridge requires the Project CHIP (connectedhomeip) SDK. Clone and build it:
   
   ```bash
   # Clone Matter SDK (recommended release: v1.2-branch or latest stable)
   cd ~
   git clone --depth 1 --branch v1.2-branch https://github.com/project-chip/connectedhomeip.git
   cd connectedhomeip
   
   # Bootstrap and build
   ./scripts/checkout_submodules.py --shallow --platform linux
   source scripts/activate.sh
   
   # Build the SDK (this takes a while on Pi Zero - consider cross-compiling)
   ./scripts/build/build_examples.py --target linux-x64-chip-tool build
   
   # Or for ARM (Raspberry Pi):
   # ./scripts/build/build_examples.py --target linux-arm64-chip-tool build
   
   # Set MATTER_ROOT environment variable
   export MATTER_ROOT=$(pwd)
   ```
   
   **Note**: Building on Raspberry Pi Zero can take several hours. Consider cross-compiling on a faster machine.
   
   **Recommended Matter SDK Version**: `v1.2-branch` or later stable release.
   
   For the latest information on Matter SDK versions and build instructions, see:
   - https://github.com/project-chip/connectedhomeip
   - https://github.com/project-chip/connectedhomeip/blob/master/docs/guides/BUILDING.md

## Building the Host Bridge

### With Matter Support (Full Functionality)

```bash
# Navigate to the repository root
cd /path/to/viking-bio-matter

# Set MATTER_ROOT to your connectedhomeip checkout
export MATTER_ROOT=/path/to/connectedhomeip

# Create build directory
mkdir -p build_host && cd build_host

# Configure with Matter enabled
cmake .. -DENABLE_MATTER=ON

# Build
make host_bridge -j$(nproc)
```

The executable will be created at `build_host/host_bridge/host_bridge`.

### Without Matter Support (Stub Mode for Testing)

To build without the Matter SDK (for testing serial parsing only):

```bash
mkdir -p build_host && cd build_host
cmake .. -DENABLE_MATTER=OFF
make host_bridge
```

This creates a stub version that reads serial data and logs changes but doesn't actually expose Matter endpoints.

## Running the Bridge

### Basic Usage

```bash
# Run with default settings (expects /dev/ttyUSB0 at 9600 baud)
./host_bridge/host_bridge

# Specify custom serial device
./host_bridge/host_bridge --device /dev/ttyAMA0

# Specify custom setup code for commissioning
./host_bridge/host_bridge --device /dev/ttyUSB0 --setup-code 12345678
```

### Command Line Options

```
Usage: host_bridge [OPTIONS]

Options:
  -d, --device PATH      Serial device path (default: /dev/ttyUSB0)
  -b, --baud RATE        Baud rate (default: 9600)
  -s, --setup-code CODE  Matter setup code (default: 20202021)
  -h, --help             Show help message

Examples:
  host_bridge --device /dev/ttyAMA0
  host_bridge --device /dev/ttyUSB0 --setup-code 12345678
```

### Serial Device Setup

Identify your serial device:

```bash
# List available serial devices
ls -l /dev/tty*

# Common device names:
# /dev/ttyUSB0  - USB-to-serial adapter
# /dev/ttyAMA0  - Raspberry Pi hardware UART (GPIO 14/15)
# /dev/ttyS0    - Alternative UART name
```

#### Using Raspberry Pi Hardware UART (GPIO)

To use the Pi's built-in UART (GPIO 14=TX, GPIO 15=RX):

1. Enable UART in `/boot/config.txt`:
   ```
   enable_uart=1
   dtoverlay=disable-bt
   ```

2. Disable serial console (if enabled):
   ```bash
   sudo raspi-config
   # Go to: Interface Options -> Serial Port
   # "Would you like a login shell accessible over serial?" -> No
   # "Would you like the serial port hardware enabled?" -> Yes
   ```

3. Reboot and use `/dev/ttyAMA0` as the device.

### Commissioning the Matter Bridge

Once the bridge is running, commission it using a Matter controller:

#### Using chip-tool (Matter SDK CLI)

```bash
# Generate QR code for commissioning (from Matter SDK)
cd $MATTER_ROOT
./out/chip-tool payload generate-qrcode 20202021

# Commission the bridge (replace with your setup code)
./out/chip-tool pairing onnetwork 1 20202021

# Read attributes
./out/chip-tool onoff read on-off 1 1
./out/chip-tool levelcontrol read current-level 1 1
./out/chip-tool temperaturemeasurement read measured-value 1 1
```

#### Using Apple Home, Google Home, or Alexa

1. Open your smart home app
2. Select "Add Device" or "Add Matter Device"
3. Scan the QR code or enter the setup code (20202021 by default)
4. Follow the app's commissioning flow

**Security Note**: The default setup code `20202021` is for testing only. For production use, generate a unique setup code:

```bash
# Generate a secure 8-digit setup code
python3 -c "import random; print(random.randint(10000000, 99999999))"
```

## Running as a System Service

To run the bridge automatically on boot, use the provided systemd service template:

```bash
# Copy service file to systemd directory
sudo cp host_bridge.service /etc/systemd/system/

# Edit the service file to set correct paths and options
sudo nano /etc/systemd/system/host_bridge.service

# Enable and start the service
sudo systemctl enable host_bridge.service
sudo systemctl start host_bridge.service

# Check status
sudo systemctl status host_bridge.service

# View logs
journalctl -u host_bridge.service -f
```

## Troubleshooting

### Serial Port Access Denied

If you get a "Permission denied" error:

```bash
# Add your user to the dialout group
sudo usermod -a -G dialout $USER

# Log out and back in, or:
newgrp dialout
```

### No Serial Data Received

1. Verify connections:
   - Viking Bio TX → Pi RX (GP15 or USB adapter RX)
   - Ground connected

2. Check baud rate matches Viking Bio 20 (default 9600)

3. Test with serial monitor:
   ```bash
   # Install minicom
   sudo apt-get install minicom
   
   # Test serial connection
   minicom -D /dev/ttyUSB0 -b 9600
   ```

### Matter SDK Build Errors

If the Matter SDK fails to build on Raspberry Pi Zero:

1. **Cross-compile** on a faster machine:
   ```bash
   # On your development machine
   ./scripts/build/build_examples.py --target linux-arm-chip-tool build
   
   # Copy built libraries to Raspberry Pi
   scp -r out/linux-arm-chip-tool/lib pi@raspberrypi:~/matter-libs/
   ```

2. Or use a **pre-built Matter SDK** if available for ARM

### Bridge Won't Commission

1. Check WiFi connectivity
2. Ensure firewall allows Matter UDP traffic (port 5540)
3. Verify setup code is correct
4. Check logs for errors: `journalctl -u host_bridge.service`

## Development

### Project Structure

```
host_bridge/
├── CMakeLists.txt                # Build configuration
├── main.cpp                      # Application entry point, serial I/O
├── matter_bridge.h/cpp           # Matter stack integration
├── viking_bio_protocol.h         # Protocol definitions (shared)
├── viking_bio_protocol_linux.c   # Protocol parser (Linux port)
├── README.md                     # This file
└── host_bridge.service           # Systemd service template
```

### Adding Features

To extend the bridge with additional clusters or attributes:

1. Update `matter_bridge.h` with new methods
2. Implement cluster updates in `matter_bridge.cpp`
3. Add necessary Matter SDK includes and API calls
4. Call update methods from `main.cpp` when data changes

### Testing Without Hardware

Use the Viking Bio simulator from `examples/`:

```bash
# In one terminal - run simulator
cd examples
python3 viking_bio_simulator.py /dev/pts/X  # Use a pseudo-terminal

# In another terminal - run bridge pointing to the same device
./host_bridge/host_bridge --device /dev/pts/X
```

Or create a virtual serial port pair:

```bash
sudo apt-get install socat

# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the device names (e.g., /dev/pts/1 and /dev/pts/2)

# Send test data to one end
echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/pts/1

# Run bridge on the other end
./host_bridge/host_bridge --device /dev/pts/2
```

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Project CHIP GitHub](https://github.com/project-chip/connectedhomeip)
- [Matter Device Types](https://github.com/project-chip/connectedhomeip/tree/master/src/app/zap-templates/zcl/data-model/chip)
- [Viking Bio 20 Protocol Documentation](../../ARCHITECTURE.md)

## License

This project is open source under the MIT License. See [LICENSE](../LICENSE) for details.
