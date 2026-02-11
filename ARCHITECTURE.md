# Matter Bridge Architecture

## Overview

This firmware implements a Matter bridge for the Viking Bio 20 burner, enabling integration with Matter-compatible smart home systems.

## Components

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

## Future Enhancements

1. **Full Matter SDK Integration**
   - Commissioning flow
   - Attribute subscriptions
   - Command handling

2. **Network Connectivity**
   - WiFi support (Pico W)
   - Thread support (with external radio)
   - Ethernet support (with W5500 module)

3. **Advanced Features**
   - OTA firmware updates
   - Error reporting
   - Historical data logging
   - Alarm notifications

4. **Protocol Extensions**
   - Support for multiple Viking Bio devices
   - Bidirectional communication (control burner)
   - Enhanced diagnostics

## Host Bridge Architecture (Raspberry Pi Zero)

The host bridge provides a full Matter implementation running on Linux (Raspberry Pi Zero or similar).

### Architecture Overview

```
Viking Bio 20 Burner
    ↓ (TTL Serial 9600 baud)
USB-to-Serial Adapter (or direct UART)
    ↓ (/dev/ttyUSB0 or /dev/ttyAMA0)
┌─────────────────────────────────────────┐
│  Host Bridge (Linux User Space)         │
│                                          │
│  ┌────────────────────────────────────┐ │
│  │  Serial Reader (POSIX termios)     │ │
│  │  - Opens serial device             │ │
│  │  - Non-blocking reads              │ │
│  │  - 8N1, 9600 baud                  │ │
│  └─────────────┬──────────────────────┘ │
│                ↓                         │
│  ┌────────────────────────────────────┐ │
│  │  Protocol Parser                   │ │
│  │  (viking_bio_protocol_linux.c)     │ │
│  │  - Binary format: [AA][FLAGS]...   │ │
│  │  - Text format: F:1,S:50,T:75      │ │
│  └─────────────┬──────────────────────┘ │
│                ↓                         │
│  ┌────────────────────────────────────┐ │
│  │  Matter Bridge (matter_bridge.cpp) │ │
│  │  ┌──────────────────────────────┐  │ │
│  │  │ Matter SDK (connectedhomeip) │  │ │
│  │  │ - Device Layer               │  │ │
│  │  │ - Cluster implementations    │  │ │
│  │  │ - Commissioning              │  │ │
│  │  │ - Event loop (threaded)      │  │ │
│  │  └──────────────────────────────┘  │ │
│  └─────────────┬──────────────────────┘ │
└────────────────┼───────────────────────┘
                 ↓ (Matter over WiFi)
         Matter Controller
     (Google Home, Apple Home, etc.)
```

### Matter Cluster Mapping

The host bridge exposes one endpoint (Endpoint 1) with three clusters:

| Cluster | ID | Attribute | Viking Bio Field | Mapping |
|---------|-----|-----------|------------------|---------|
| On/Off | 0x0006 | OnOff (0x0000) | flame_detected | true = flame ON, false = flame OFF |
| Level Control | 0x0008 | CurrentLevel (0x0000) | fan_speed | 0-100% → 0-254 Matter level |
| Temperature Measurement | 0x0402 | MeasuredValue (0x0000) | temperature | °C × 100 (Matter uses 0.01°C units) |

### Key Differences from Pico Firmware

| Aspect | Pico Firmware | Host Bridge |
|--------|---------------|-------------|
| **Platform** | RP2040 microcontroller | Linux (Raspberry Pi) |
| **Matter Stack** | Stub/Placeholder | Full connectedhomeip SDK |
| **Serial API** | Pico SDK UART | POSIX termios |
| **Threading** | Single-threaded | Matter event loop in separate thread |
| **Commissioning** | Not implemented | Full commissioning with persistent storage |
| **Network** | Requires Pico W | Uses system WiFi |
| **Updates** | Flash new firmware | Standard software update |
| **Resources** | 264KB RAM, 2MB Flash | Full Linux resources |

### Data Flow

1. **Serial Reading**: Main loop polls serial device using `read()` with non-blocking I/O
2. **Protocol Parsing**: `viking_bio_parse_data()` identifies packets and extracts fields
3. **Change Detection**: Only updates Matter attributes when values actually change
4. **Matter Updates**: Calls `MatterBridge::updateXxx()` methods which use CHIP APIs
5. **Attribute Reporting**: Matter SDK automatically sends reports to subscribed controllers

### Build System Integration

The host bridge is opt-in via CMake:

```cmake
# Default: Build Pico firmware
cmake ..

# Host bridge: Build for Raspberry Pi
cmake .. -DENABLE_MATTER=ON
```

When `ENABLE_MATTER=ON`:
- Top-level CMakeLists.txt switches to host mode
- Skips Pico SDK initialization
- Adds `host_bridge/` subdirectory
- Requires `MATTER_ROOT` environment variable

### Matter SDK Integration

The host bridge integrates with connectedhomeip at build time:

1. User clones and builds Matter SDK separately
2. Sets `MATTER_ROOT` environment variable
3. CMake finds required headers and libraries
4. Links against Matter static libraries
5. Executable contains full Matter stack

### Security Considerations

- **Setup Code**: Default code (20202021) is for testing only
- **Production**: Use secure, randomly generated setup codes
- **Commissioning**: Matter standard security (PASE, CASE)
- **Persistent Storage**: Fabric information stored by Matter SDK
- **Network**: Relies on WiFi security (WPA2/WPA3)

### Performance

- **Latency**: Serial read → Matter update typically < 200ms
- **CPU Usage**: < 5% on Raspberry Pi Zero (when idle)
- **Memory**: ~50MB resident (includes Matter SDK)
- **Network**: Minimal traffic (attribute reports on change only)

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Project CHIP](https://github.com/project-chip/connectedhomeip)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)
