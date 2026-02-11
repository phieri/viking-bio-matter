# Matter Bridge Architecture

## Overview

This project provides two implementations of a Matter bridge for the Viking Bio 20 burner:

1. **Pico Firmware** (`src/`): Embedded firmware for Raspberry Pi Pico
2. **Host Bridge** (`host_bridge/`): User-space Linux application with full Matter support

Both share the same Viking Bio serial protocol parser and expose burner data through Matter clusters.

## Implementations

### Pico Firmware (Embedded)

The Pico firmware runs on Raspberry Pi Pico/Pico W and provides a lightweight bridge with basic Matter attribute management (full SDK integration planned).

### Host Bridge (Linux)

The host bridge runs on Raspberry Pi Zero or any Linux system and provides complete Matter 1.3 integration using the connectedhomeip SDK. This is the **recommended implementation** for production use.

## Pico Firmware Components

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

## GPIO Pin Assignment

| GPIO | Function | Description |
|------|----------|-------------|
| GP0  | UART0 TX | Serial output (unused) |
| GP1  | UART0 RX | Serial input from Viking Bio |
| GP25 | LED      | Status indicator |

## Matter Device Type

The bridge implements a **Temperature Sensor** device type with additional custom attributes:

- **Device Type ID**: 0x0302 (Temperature Sensor)
- **Vendor ID**: TBD
- **Product ID**: TBD

## Building with Full Matter SDK

To integrate the full Matter SDK:

1. Clone the Matter (Project CHIP) repository
2. Update CMakeLists.txt to link Matter libraries
3. Implement Matter device callbacks in `matter_bridge.c`
4. Add Matter commissioning support
5. Configure network stack (WiFi for Pico W, Thread for Pico + radio)

### Required Changes

```cmake
# Add to CMakeLists.txt
include(matter_sdk_import.cmake)

target_link_libraries(viking_bio_matter
    # Existing libraries...
    chip_core
    chip_app
    chip_shell
)
```

## Testing

### Serial Simulator

For testing without hardware, you can send test data to the Pico:

```bash
# Binary protocol (hex)
echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/ttyUSB0

# Text protocol
echo "F:1,S:80,T:75" > /dev/ttyUSB0
```

### Debug Output

Connect to the Pico's USB serial port to see debug output:

```bash
screen /dev/ttyACM0 115200
```

Expected output:
```
Viking Bio Matter Bridge starting...
Initialization complete. Reading serial data...
Flame: ON, Fan Speed: 80%, Temp: 75°C
Matter: Flame state changed to ON
Matter: Fan speed changed to 80%
```

## Host Bridge Architecture (Linux)

The host bridge provides full Matter integration on Linux systems. See `host_bridge/README.md` for detailed setup instructions.

### Architecture Overview

```
Viking Bio 20 Burner
    ↓ (TTL Serial 9600 baud)
Linux Serial Device (/dev/ttyUSB0, /dev/ttyAMA0)
    ↓ (POSIX termios)
Viking Bio Protocol Parser
    ↓ (viking_bio_data_t)
Matter Bridge (connectedhomeip SDK)
    ↓ (Matter over WiFi)
Matter Controllers (Apple Home, Google Home, etc.)
```

### Host Bridge Components

#### 1. main.cpp
- Program entry point and main event loop
- Parses command line arguments (serial device, setup code, discriminator)
- Configures POSIX serial port with termios (9600 8N1)
- Reads serial data with select() for non-blocking I/O
- Calls Viking Bio parser and updates Matter attributes
- Handles signals for graceful shutdown

#### 2. matter_bridge.cpp/h
- Wraps connectedhomeip SDK functionality
- Initializes Matter stack and device layer
- Creates endpoint with three clusters:
  - **On/Off (0x0006)**: Maps flame_detected → OnOff attribute
  - **Level Control (0x0008)**: Maps fan_speed (0-100) → CurrentLevel (0-254)
  - **Temperature Measurement (0x0402)**: Maps temperature (°C) → MeasuredValue (hundredths of °C)
- Manages commissioning with QR code generation
- Reports attribute changes to subscribed controllers
- Provides stub implementation when ENABLE_MATTER=OFF

#### 3. viking_bio_protocol_linux.c/h
- Linux port of Viking Bio serial protocol parser
- Identical parsing logic to Pico version
- No hardware dependencies (pure C standard library)
- Supports both binary and text protocols

#### 4. CMakeLists.txt
- Detects MATTER_ROOT environment variable
- Links against connectedhomeip SDK libraries when ENABLE_MATTER=ON
- Builds stub version when Matter SDK not available
- Provides clear error messages for missing dependencies

#### 5. host_bridge.service
- Systemd service unit for automatic startup
- Runs as unprivileged user with serial port access
- Configurable serial device and commissioning parameters
- Journal logging for debugging

### Matter Cluster Mapping

| Viking Bio Field | Matter Cluster | Cluster ID | Attribute | Type | Mapping |
|------------------|----------------|------------|-----------|------|---------|
| flame_detected | On/Off | 0x0006 | OnOff | bool | Direct |
| fan_speed | Level Control | 0x0008 | CurrentLevel | uint8 | 0-100% → 0-254 |
| temperature | Temperature Measurement | 0x0402 | MeasuredValue | int16 | °C → hundredths of °C |

### Data Flow

1. **Serial Input**: Linux kernel receives data from UART/USB-serial device
2. **Read**: Application reads data with POSIX read() (non-blocking with select())
3. **Parse**: Viking Bio parser extracts flame, speed, temperature from binary/text format
4. **Update**: Matter bridge updates cluster attributes via SDK APIs
5. **Report**: SDK automatically sends reports to subscribed Matter controllers
6. **Event Loop**: SDK event loop processes network events, subscriptions, commands

### Commissioning Flow

1. **Initialization**: Bridge starts and prints QR code / setup code
2. **Discovery**: Matter controller discovers device via mDNS
3. **PASE**: Controller establishes secure channel using setup code
4. **Provisioning**: Bridge receives WiFi credentials (if needed) and operational certificate
5. **CASE**: Ongoing encrypted communication with controller
6. **Subscription**: Controller subscribes to attribute reports
7. **Reporting**: Bridge sends attribute changes when Viking Bio data updates

### Build Modes

The host bridge supports two build modes:

#### Full Matter Mode (ENABLE_MATTER=ON)
- Requires connectedhomeip SDK installation
- Links against Matter libraries
- Provides complete commissioning and reporting
- Recommended for production

#### Stub Mode (ENABLE_MATTER=OFF, default)
- No Matter SDK required
- Prints attribute updates to console
- Useful for testing parser without full Matter stack
- Automatically built when MATTER_ROOT not set

### Security Considerations

- Default setup code (20202021) is for testing only
- Production deployments should use unique setup codes and discriminators
- Bridge runs as unprivileged user with serial port access only
- Systemd service includes security hardening (NoNewPrivileges, PrivateTmp, etc.)
- Matter stack handles all network security (PASE, CASE encryption)

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
1. **Configuration File**
   - YAML/INI configuration instead of command line only
   - Multiple device support

2. **Advanced Features**
   - OTA firmware updates
   - Error reporting
   - Historical data logging
   - Alarm notifications

4. **Protocol Extensions**
   - Support for multiple Viking Bio devices
   - Bidirectional communication (control burner)
   - Enhanced diagnostics

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Project CHIP](https://github.com/project-chip/connectedhomeip)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)
