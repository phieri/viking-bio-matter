# Viking Bio Matter Bridge - Host Application

A Linux user-space Matter bridge that runs on Raspberry Pi Zero and exposes Viking Bio 20 burner data over Matter/WiFi.

## Overview

This host-side bridge reads Viking Bio serial data from a UART device and exposes it through the Matter protocol, allowing integration with Matter-compatible smart home systems like Apple HomeKit, Google Home, Amazon Alexa, and others.

### Matter Cluster Mapping

The bridge exposes the following Matter clusters:

| Cluster | ID | Attribute | Viking Bio Data |
|---------|-----|-----------|----------------|
| On/Off | 0x0006 | OnOff | Flame detected state |
| Level Control | 0x0008 | CurrentLevel | Fan speed (0-100%) |
| Temperature Measurement | 0x0402 | MeasuredValue | Temperature (°C) |

## Hardware Requirements

- Raspberry Pi Zero, Zero W, or Zero 2 W (or any Linux system)
- USB-to-serial adapter or direct UART connection to Viking Bio 20
- WiFi connectivity (for Matter over WiFi)

## Prerequisites

### System Dependencies

Install required packages on Raspberry Pi OS (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    libssl-dev \
    libglib2.0-dev \
    libavahi-client-dev \
    libreadline-dev
```

### Matter SDK Setup

The Matter SDK (connectedhomeip) is required to build the bridge with full Matter support.

#### 1. Clone Matter SDK

```bash
cd ~
git clone --depth 1 --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git
cd connectedhomeip
```

**Recommended version**: v1.3-branch (Matter 1.3)  
**Alternative**: Use `master` branch for latest features

#### 2. Bootstrap Matter SDK

```bash
# Initialize submodules and install dependencies
./scripts/checkout_submodules.py --shallow --platform linux
./scripts/build/gn_bootstrap.sh

# Activate Matter environment
source scripts/activate.sh
```

#### 3. Build Matter SDK

```bash
# Build Matter libraries for Linux
./scripts/build/build_examples.py --target linux-x64-all-clusters build

# Or for ARM (Raspberry Pi)
./scripts/build/build_examples.py --target linux-arm64-all-clusters build
```

This will create the necessary libraries in `out/linux-x64-all-clusters` or `out/linux-arm64-all-clusters`.

#### 4. Set Environment Variable

Add to your `~/.bashrc` or `~/.profile`:

```bash
export MATTER_ROOT=~/connectedhomeip
```

Or set for the current session:

```bash
export MATTER_ROOT=~/connectedhomeip
```

## Building the Host Bridge

### With Matter Support (Recommended)

```bash
cd /path/to/viking-bio-matter
mkdir build_host
cd build_host

# Configure with Matter support
cmake .. -DENABLE_MATTER=ON

# Build
make host_bridge -j$(nproc)
```

The `host_bridge` executable will be created in the build directory.

### Without Matter Support (Stub Mode)

For testing or development without the full Matter SDK:

```bash
cd /path/to/viking-bio-matter
mkdir build_host
cd build_host

# Configure without Matter (default)
cmake ..

# Build
make host_bridge -j$(nproc)
```

This builds a stub implementation that parses Viking Bio data but doesn't provide actual Matter functionality.

## Configuration

### Serial Device

The bridge reads Viking Bio data from a serial device. Common devices on Raspberry Pi:

- `/dev/ttyUSB0` - USB-to-serial adapter
- `/dev/ttyAMA0` - Raspberry Pi hardware UART (GPIO 14/15)
- `/dev/serial0` - Symlink to primary UART

### Permissions

Grant your user access to the serial port:

```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

Or run with sudo (not recommended for production):

```bash
sudo ./host_bridge
```

## Running the Bridge

### Basic Usage

```bash
./host_bridge
```

This uses default settings:
- Serial device: `/dev/ttyUSB0`
- Setup code: `20202021`
- Discriminator: `3840`

### Custom Configuration

```bash
./host_bridge \
    --device /dev/ttyAMA0 \
    --setup-code 34567890 \
    --discriminator 2000
```

### Command Line Options

```
Options:
  -d, --device <path>        Serial device path (default: /dev/ttyUSB0)
  -s, --setup-code <code>    Matter setup code (default: 20202021)
  -r, --discriminator <num>  Matter discriminator (default: 3840)
  -h, --help                 Show help message
```

### Example Output

```
====================================
Viking Bio Matter Bridge
====================================
Serial Device: /dev/ttyUSB0
Setup Code: 20202021
Discriminator: 3840
====================================

Viking Bio protocol initialized
Initializing Matter bridge...

=== Matter Commissioning ===
Setup Code: 20202021
Discriminator: 3840
QR Code: MT:Y.K9042C00KA0648G00
===========================

Serial port /dev/ttyUSB0 configured successfully (9600 8N1)

Starting main loop...
Waiting for Viking Bio data on /dev/ttyUSB0

Viking Bio data: Flame=ON, Speed=75%, Temp=65°C, Error=0x00
Matter: Flame state changed to ON
Matter: Fan speed changed to 75%
Matter: Temperature changed to 65°C
```

## Matter Commissioning

### Using the QR Code

1. Start the bridge - it will print a QR code string
2. Use your Matter controller app to scan the QR code:
   - **Apple Home**: Add Accessory → Scan QR Code
   - **Google Home**: Add Device → Matter Device
   - **chip-tool**: `chip-tool pairing qrcode <node-id> <qr-code>`

### Using Setup Code

If QR scanning isn't available, use the setup code manually:

```bash
chip-tool pairing onnetwork 1 20202021
```

### Verify Connection

```bash
# Read On/Off state
chip-tool onoff read on-off 1 1

# Read fan speed (level)
chip-tool levelcontrol read current-level 1 1

# Read temperature
chip-tool temperaturemeasurement read measured-value 1 1
```

## Running as a Service

### Install Systemd Service

```bash
cd /path/to/viking-bio-matter/host_bridge

# Edit host_bridge.service to set correct paths
sudo nano host_bridge.service

# Install service
sudo cp host_bridge.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable host_bridge
sudo systemctl start host_bridge
```

### Check Service Status

```bash
sudo systemctl status host_bridge
sudo journalctl -u host_bridge -f
```

## Troubleshooting

### Serial Port Issues

**Error: "Permission denied" when opening serial port**

```bash
sudo usermod -a -G dialout $USER
# Log out and back in
```

**Error: "No such file or directory"**

Check available serial ports:
```bash
ls -l /dev/tty{USB,AMA,S}*
```

### Matter Build Issues

**Error: "MATTER_ROOT not set"**

```bash
export MATTER_ROOT=~/connectedhomeip
```

**Error: "Could not find Matter libraries"**

Rebuild Matter SDK:
```bash
cd $MATTER_ROOT
./scripts/build/build_examples.py --target linux-x64-all-clusters build
```

### Commissioning Issues

**Device not found**

- Ensure bridge is running (`ps aux | grep host_bridge`)
- Check WiFi connectivity
- Verify firewall isn't blocking mDNS/Matter ports

**Cannot commission**

- Verify setup code is correct
- Try resetting: Stop bridge, delete `/tmp/chip_*`, restart
- Check logs: `sudo journalctl -u host_bridge -n 100`

## Development

### Code Structure

```
host_bridge/
├── main.cpp                      # Application entry point
├── matter_bridge.cpp/h           # Matter SDK integration
├── viking_bio_protocol_linux.c/h # Serial protocol parser (Linux port)
├── CMakeLists.txt                # Build configuration
├── README.md                     # This file
└── host_bridge.service           # Systemd service template
```

### Testing Without Hardware

Simulate Viking Bio data:

```bash
# Terminal 1: Start bridge with virtual serial port
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the two PTY paths (e.g., /dev/pts/3, /dev/pts/4)

./host_bridge --device /dev/pts/3

# Terminal 2: Send test data
# Binary format
echo -ne '\xAA\x01\x50\x00\x41\x55' > /dev/pts/4

# Text format
echo "F:1,S:80,T:65" > /dev/pts/4
```

### Debugging

Enable verbose Matter logs:

```bash
export CHIP_DEVICE_LAYER_NONE=1
./host_bridge --device /dev/ttyUSB0 2>&1 | tee bridge.log
```

## Security Considerations

### Production Deployment

⚠️ **Important**: The default setup code (`20202021`) is for testing only!

For production:

1. **Generate a secure setup code**: Use a random 8-digit code
2. **Use unique discriminators**: Assign unique values per device
3. **Secure the device**: Run as unprivileged user, use AppArmor/SELinux
4. **Update regularly**: Keep Matter SDK and bridge code up to date

### Recommended Setup

```bash
# Generate random setup code (8 digits)
SETUP_CODE=$(shuf -i 10000000-99999999 -n 1)
DISCRIMINATOR=$(shuf -i 0-4095 -n 1)

./host_bridge \
    --device /dev/ttyUSB0 \
    --setup-code $SETUP_CODE \
    --discriminator $DISCRIMINATOR
```

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Project CHIP (connectedhomeip)](https://github.com/project-chip/connectedhomeip)
- [Matter Developer Guide](https://developers.home.google.com/matter)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)

## License

This project is licensed under the MIT License - see the LICENSE file in the repository root.
