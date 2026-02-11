# Matter Bridge Architecture

## Overview

This project implements Matter bridge functionality for the Viking Bio 20 burner in two ways:

1. **Raspberry Pi Pico Firmware** - Embedded implementation with Matter stubs
2. **Raspberry Pi Zero Host Bridge** - Full Matter SDK implementation (Linux userspace)

Both implementations share the same Viking Bio protocol parser and expose the same Matter clusters.

---

## Raspberry Pi Pico Implementation (Embedded)

This section describes the embedded firmware running on Raspberry Pi Pico.

### Components

### 1. Serial Handler (`serial_handler.c`)

Manages UART communication with the Viking Bio 20 burner:
- Configures UART0 (GP0/GP1) at 9600 baud, 8N1
- Uses interrupt-driven reception with circular buffer
- Non-blocking read API for main loop integration

### 2. Viking Bio Protocol Parser (`viking_bio_protocol.c`)

Parses incoming serial data from the burner:

#### Binary Protocol
```
Byte 0: 0xAA (Start marker)
Byte 1: Flags (bit 0: flame, bits 1-7: errors)
Byte 2: Fan speed (0-100%)
Byte 3: Temperature high byte
Byte 4: Temperature low byte  
Byte 5: 0x55 (End marker)
```

#### Text Protocol (Fallback)
```
F:1,S:50,T:75\n
```
- F: Flame (0/1)
- S: Speed (0-100)
- T: Temperature (°C)

### 3. Matter Bridge (`matter_bridge.c`)

Exposes burner data via Matter protocol:

**Current Implementation**: Basic attribute storage and change detection
**Future Implementation**: Full Matter SDK integration

#### Matter Clusters (Planned)

1. **On/Off Cluster (0x0006)**
   - Attribute: OnOff (bool) - Flame detected state

2. **Level Control Cluster (0x0008)**
   - Attribute: CurrentLevel (uint8) - Fan speed percentage

3. **Temperature Measurement Cluster (0x0402)**
   - Attribute: MeasuredValue (int16) - Burner temperature

### 4. Main Application (`main.c`)

Coordinates all components:
1. Initializes peripherals and subsystems
2. Reads serial data in main loop
3. Parses Viking Bio protocol
4. Updates Matter attributes
5. Provides status LED feedback

## Data Flow

```
Viking Bio 20
    ↓ (TTL Serial 9600 baud)
Serial Handler (UART0)
    ↓ (Circular Buffer)
Protocol Parser
    ↓ (viking_bio_data_t)
Matter Bridge
    ↓ (Matter Attributes)
Matter Controller
```

## GPIO Pin Assignment (Pico)

| GPIO | Function | Description |
|------|----------|-------------|
| GP0  | UART0 TX | Serial output (unused) |
| GP1  | UART0 RX | Serial input from Viking Bio |
| GP25 | LED      | Status indicator |

---

## Raspberry Pi Zero Host Bridge Implementation

The host bridge is a Linux userspace application that provides full Matter protocol support.

### Architecture

```
Viking Bio 20
    ↓ (TTL Serial 9600 baud, USB or GPIO UART)
POSIX Serial Handler (termios)
    ↓ (Read buffer)
Viking Bio Protocol Parser (Linux port)
    ↓ (viking_bio_data_t)
Matter Bridge (Full SDK)
    ↓ (Matter clusters/attributes)
Matter Controller (WiFi)
```

### Components

#### 1. Main Application (`host_bridge/main.cpp`)

Coordinates the bridge operation:
- Parses command-line arguments (serial device, baud rate, setup code)
- Opens and configures serial port using termios (POSIX)
- Initializes Viking Bio protocol parser
- Initializes Matter bridge with setup code
- Main event loop:
  - Reads serial data (non-blocking)
  - Parses Viking Bio protocol
  - Updates Matter attributes on change
  - Processes Matter events
- Handles graceful shutdown on SIGINT/SIGTERM

#### 2. Matter Bridge (`host_bridge/matter_bridge.cpp`)

Full Matter SDK integration:

**Initialization**:
- Initializes CHIP platform manager and stack
- Sets up device attestation credentials
- Configures commissioning parameters

**Cluster Implementation**:
1. **On/Off Cluster (0x0006)**
   - Endpoint: 1
   - Attribute: OnOff (bool)
   - Updated via: `updateFlame(bool)`
   
2. **Level Control Cluster (0x0008)**
   - Endpoint: 1
   - Attribute: CurrentLevel (uint8, 0-254)
   - Maps from 0-100% fan speed
   - Updated via: `updateFanSpeed(uint8_t)`
   
3. **Temperature Measurement Cluster (0x0402)**
   - Endpoint: 1
   - Attribute: MeasuredValue (int16, in 0.01°C units)
   - Updated via: `updateTemperature(int16_t)`

**Event Processing**:
- Runs Matter event loop in separate thread via PlatformMgr
- Schedules work on the Matter thread as needed
- Handles attribute reporting automatically

**Conditional Compilation**:
- Uses `#ifdef ENABLE_MATTER` to support building without Matter SDK
- Stub mode logs changes but doesn't expose Matter endpoints

#### 3. Viking Bio Protocol Parser (`host_bridge/viking_bio_protocol_linux.c`)

Port of the Pico protocol parser for Linux:
- Pure C code, no hardware dependencies
- Identical API to Pico version
- Supports both binary and text protocols
- Maintains current state for change detection

#### 4. Serial Handler (in `main.cpp`)

POSIX serial port handling using termios:
- Opens serial device with O_RDONLY | O_NOCTTY | O_NDELAY
- Configures 8N1 mode with specified baud rate
- Sets non-blocking read with timeout
- Flushes buffers on startup

### Data Flow

```
[Viking Bio 20] 
    ↓
[Serial Port: /dev/ttyUSB0 or /dev/ttyAMA0]
    ↓ (TTL 9600 baud, 8N1)
[POSIX termios - Non-blocking read]
    ↓ (uint8_t buffer[256])
[viking_bio_parse_data()]
    ↓ (viking_bio_data_t)
[Change Detection]
    ↓ (if flame changed)
[MatterBridge::updateFlame()]
    ↓ (CHIP API: OnOff::Attributes::OnOff::Set())
[Matter Attribute Report]
    ↓ (Over WiFi)
[Matter Controller/Hub]
```

### Matter Device Configuration

- **Device Type**: Bridge (0x000E) with aggregated endpoints
  - Endpoint 0: Root device (bridge)
  - Endpoint 1: Viking Bio burner (aggregated device)

- **Clusters on Endpoint 1**:
  - On/Off (0x0006) - Server
  - Level Control (0x0008) - Server
  - Temperature Measurement (0x0402) - Server

- **Network**: WiFi (802.11 b/g/n)
- **Commissioning**: QR code or manual setup code (11-digit)
- **Vendor ID**: Use test VID or registered VID
- **Product ID**: Configurable

### Configuration and Runtime

**Command Line Options**:
```
--device PATH       Serial device (default: /dev/ttyUSB0)
--baud RATE        Baud rate (default: 9600)
--setup-code CODE  Matter setup code (default: 20202021)
--help             Show help
```

**Environment Variables**:
```
MATTER_ROOT        Path to connectedhomeip SDK (required for build)
```

**Systemd Service**:
- Service file: `host_bridge/host_bridge.service`
- Runs as user `pi` with network-online.target dependency
- Auto-restart on failure
- Logs to systemd journal

### Security Considerations

1. **Setup Code**: Default code (20202021) is for testing only. Use a unique code in production.
2. **Serial Device Permissions**: User must be in `dialout` group for serial access.
3. **Network Security**: Matter uses PASE for commissioning, CASE for operational communications.
4. **Device Attestation**: Uses example DAC provider (should use production certificates).

### Building the Host Bridge

**Requirements**:
- CMake 3.16+
- C++17 compiler (g++ or clang)
- Matter SDK built for Linux ARM or x64
- System libraries: pthread, ssl, glib, avahi, dbus

**Build Process**:
```bash
# Set Matter SDK path
export MATTER_ROOT=/path/to/connectedhomeip

# Configure (from repo root)
mkdir -p build_host && cd build_host
cmake .. -DENABLE_MATTER=ON

# Build
make host_bridge

# Output: build_host/host_bridge/host_bridge
```

**Without Matter SDK** (stub mode):
```bash
cmake .. -DENABLE_MATTER=OFF
make host_bridge
```

---

## Shared Components

### Viking Bio Protocol

Both implementations use the same protocol specification:

#### Binary Protocol
```
Byte 0: 0xAA (Start marker)
Byte 1: Flags (bit 0: flame, bits 1-7: errors)
Byte 2: Fan speed (0-100%)
Byte 3: Temperature high byte
Byte 4: Temperature low byte  
Byte 5: 0x55 (End marker)
```

#### Text Protocol (Fallback)
```
F:1,S:50,T:75\n
```
- F: Flame (0/1)
- S: Speed (0-100)
- T: Temperature (°C)

### Matter Clusters (Common)

Both implementations expose the same Matter clusters:

1. **On/Off Cluster (0x0006)**
   - Attribute: OnOff (bool) - Flame detected state

2. **Level Control Cluster (0x0008)**
   - Attribute: CurrentLevel (uint8) - Fan speed percentage

3. **Temperature Measurement Cluster (0x0402)**
   - Attribute: MeasuredValue (int16) - Burner temperature

---

## Testing

### Serial Simulator

For testing without hardware, use the Viking Bio simulator:

```bash
# Run simulator on a pseudo-terminal
cd examples
python3 viking_bio_simulator.py /dev/pts/X
```

Or send test data directly:

```bash
# Binary protocol (hex)
echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/ttyUSB0

# Text protocol
echo "F:1,S:80,T:75" > /dev/ttyUSB0
```

### Host Bridge Testing

```bash
# Create virtual serial port pair
socat -d -d pty,raw,echo=0 pty,raw,echo=0
# Note the device names (e.g., /dev/pts/1 and /dev/pts/2)

# Send test data to one end
while true; do 
  echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/pts/1
  sleep 2
done

# Run bridge on the other end
./host_bridge/host_bridge --device /dev/pts/2
```

### Matter Commissioning Test

```bash
# Generate QR code (from Matter SDK)
cd $MATTER_ROOT
./out/chip-tool payload generate-qrcode 20202021

# Commission the bridge
./out/chip-tool pairing onnetwork 1 20202021

# Read attributes
./out/chip-tool onoff read on-off 1 1
./out/chip-tool levelcontrol read current-level 1 1
./out/chip-tool temperaturemeasurement read measured-value 1 1
```

---

## Future Enhancements

### Pico Firmware

1. **Full Matter SDK Integration**
   - Commissioning flow
   - Attribute subscriptions
   - Command handling

2. **Network Connectivity**
   - WiFi support (Pico W)
   - Thread support (with external radio)
   - Ethernet support (with W5500 module)

### Host Bridge

1. **Advanced Features**
   - OTA firmware updates via Matter
   - Error reporting and diagnostics
   - Historical data logging
   - Alarm notifications

2. **Protocol Extensions**
   - Support for multiple Viking Bio devices
   - Bidirectional communication (control burner)
   - Enhanced diagnostics

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Project CHIP](https://github.com/project-chip/connectedhomeip)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)
