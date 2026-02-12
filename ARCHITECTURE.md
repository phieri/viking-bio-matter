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

Exposes burner data via Matter protocol over WiFi:

- Complete Matter/CHIP stack integration
- WiFi connectivity via CYW43439 chip (Pico W)
- Matter commissioning with QR code
- Persistent fabric storage in flash
- Exposes three standard Matter clusters:

**Matter Clusters:**

1. **OnOff Cluster (0x0006, Endpoint 1)**
   - Attribute: OnOff (bool) - Flame detected state
   - Updates when flame state changes

2. **LevelControl Cluster (0x0008, Endpoint 1)**
   - Attribute: CurrentLevel (uint8, 0-100) - Fan speed percentage
   - Updates when fan speed changes

3. **TemperatureMeasurement Cluster (0x0402, Endpoint 1)**
   - Attribute: MeasuredValue (int16, centidegrees) - Burner temperature
   - Updates when temperature changes

**Matter Platform Port (platform/pico_w_chip_port/):**
- **Network Adapter**: CYW43439 WiFi integration with lwIP
- **Storage Adapter**: Flash-based key-value store for fabric data
- **Crypto Adapter**: mbedTLS integration for security
- **Platform Manager**: Coordinates initialization and commissioning

### 4. Main Application (`main.c`)

Coordinates all components:
1. Initializes peripherals and subsystems
2. Reads serial data in main loop
3. Parses Viking Bio protocol
4. Updates Matter attributes
5. Provides status LED feedback

## Data Flow

### Standard Mode (Matter Disabled)
```
Viking Bio 20
    ↓ (TTL Serial 9600 baud)
Serial Handler (UART0)
    ↓ (Circular Buffer)
Protocol Parser
    ↓ (viking_bio_data_t)
Matter Bridge (Stub)
    ↓ (Console Output)
Debug Serial (USB)
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
Platform Manager
    ├─→ Network (WiFi/lwIP)
    ├─→ Storage (Flash NVM)
    └─→ Crypto (mbedTLS)
    ↓
Matter Stack (connectedhomeip)
    ↓ (Matter protocol over WiFi)
Matter Controller (chip-tool, etc.)
```

## GPIO Pin Assignment

| GPIO | Function | Description |
|------|----------|-------------|
| GP0  | UART0 TX | Serial output (unused) |
| GP1  | UART0 RX | Serial input from Viking Bio |

**Note**: The Pico W LED is controlled via the CYW43 chip, not GPIO 25.

## Matter Device Type

The bridge implements a **Temperature Sensor** device type with additional custom attributes:

- **Device Type ID**: 0x0302 (Temperature Sensor)
- **Vendor ID**: TBD
- **Product ID**: TBD

## Building the Firmware

### Build Steps

```bash
# Initialize Matter SDK submodule
git submodule update --init --recursive third_party/connectedhomeip

# Configure and build
mkdir build && cd build
cmake ..
make
```

**Target platform**: Raspberry Pi Pico W (WiFi required for Matter)  
**Output**: Full Matter device firmware with commissioning support

### Configuration

The Matter platform port is located in `platform/pico_w_chip_port/` and includes:

1. **CHIPDevicePlatformConfig.h** - Device identification and limits
2. **network_adapter.cpp** - WiFi credentials and network setup
3. **storage_adapter.cpp** - Flash storage configuration
4. **crypto_adapter.cpp** - Cryptographic primitives
5. **platform_manager.cpp** - Platform coordination

See [platform/pico_w_chip_port/README.md](../platform/pico_w_chip_port/README.md) for detailed configuration options.

### WiFi Setup

Edit `platform/pico_w_chip_port/network_adapter.cpp`:

```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

### Commissioning Credentials

**Default test credentials** (defined in `CHIPDevicePlatformConfig.h`):
- Setup PIN Code: **20202021**
- Discriminator: **3840** (0x0F00)
- Manual Code: **34970112332**
- QR Code: **MT:Y.K9042C00KA0648G00**

⚠️ **WARNING:** These are test credentials. For production:
1. Generate unique per-device discriminators
2. Use device-specific setup codes
3. Program during manufacturing
4. Update configuration header with production values

## Testing

### Serial Simulator

For testing without hardware, use the included simulator:

```bash
# Install dependencies
pip3 install pyserial

# Run simulator with binary protocol (default)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0

# Run simulator with text protocol
python3 examples/viking_bio_simulator.py -p text /dev/ttyUSB0
```

### Manual Testing

Send test data directly:

```bash
# Binary protocol (hex)
echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/ttyUSB0

# Text protocol
echo "F:1,S:80,T:75" > /dev/ttyUSB0
```

### Debug Output (Standard Mode)

Connect to the Pico's USB serial port:

```bash
screen /dev/ttyACM0 115200
```

Expected output:
```
Viking Bio Matter Bridge starting...
Initialization complete. Reading serial data...
Flame: ON, Fan Speed: 80%, Temp: 75°C
Matter: OnOff cluster updated - Flame ON
Matter: LevelControl cluster updated - Fan speed 80%
```

### Testing

1. **Build and flash firmware:**
   ```bash
   cmake .. && make
   # Flash viking_bio_matter.uf2 to Pico W
   ```

2. **Monitor commissioning info:**
   ```bash
   screen /dev/ttyACM0 115200
   ```
   
   Look for:
   ```
   ====================================
       Matter Commissioning Info
   ====================================
   Setup PIN Code: 20202021
   ...
   ```

3. **Commission with chip-tool:**
   ```bash
   # Build chip-tool from Matter SDK
   cd third_party/connectedhomeip
   ./scripts/examples/gn_build_example.sh examples/chip-tool out/host
   
   # Commission device
   ./out/host/chip-tool pairing code 1 34970112332
   ```

4. **Test attribute reads:**
   ```bash
   # Flame status
   ./out/host/chip-tool onoff read on-off 1 1
   
   # Fan speed
   ./out/host/chip-tool levelcontrol read current-level 1 1
   
   # Temperature
   ./out/host/chip-tool temperaturemeasurement read measured-value 1 1
   ```

5. **Test attribute updates with simulator:**
   ```bash
   # In one terminal, run simulator
   python3 examples/viking_bio_simulator.py /dev/ttyUSB0
   
   # In another, watch attributes change
   watch -n 2 './out/host/chip-tool onoff read on-off 1 1'
   ```

## Future Enhancements

1. **Full Matter SDK Integration (Done for Pico W)**
   - ✅ Commissioning flow
   - ✅ Attribute subscriptions
   - ✅ WiFi support (Pico W)
   - ⏳ Command handling (future)
   - ⏳ OTA firmware updates (future)

2. **Network Connectivity**
   - ✅ WiFi support (Pico W)
   - ⏳ Thread support (with external radio)
   - ⏳ Ethernet support (with W5500 module)

3. **Advanced Features**
   - ⏳ OTA firmware updates over Matter
   - ⏳ Enhanced error reporting via Matter events
   - ⏳ Historical data logging
   - ⏳ Alarm notifications

4. **Protocol Extensions**
   - ⏳ Support for multiple Viking Bio devices
   - ⏳ Bidirectional communication (control burner)
   - ⏳ Enhanced diagnostics

5. **Production Readiness**
   - ⏳ Per-device unique commissioning credentials
   - ⏳ Secure boot and attestation
   - ⏳ Flash wear leveling for storage
   - ⏳ Watchdog and fault recovery

## Performance Optimizations

The firmware is built with aggressive compiler optimizations to prioritize execution speed over binary size:

### Compiler Optimizations

The following GCC flags are enabled in `CMakeLists.txt`:

- **`-O3`**: Maximum optimization for speed
  - Aggressive function inlining
  - Loop optimizations and vectorization
  - Instruction scheduling for target CPU

- **`-ffast-math`**: Fast floating-point math
  - Assumes IEEE compliance is not critical
  - Safe for this application (no sensitive FP calculations)

- **`-fno-signed-zeros`**: Treat +0.0 and -0.0 as equivalent
  - Enables additional optimizations

- **`-fno-trapping-math`**: Assume no FP exceptions
  - Safe for embedded systems without FP exception handlers

- **`-funroll-loops`**: Unroll loops for speed
  - Reduces loop overhead at the cost of code size

**Note**: Link Time Optimization (LTO) is intentionally disabled due to compatibility issues with Pico SDK's wrapped functions (`__wrap_printf`, etc.).

### Code-Level Optimizations

1. **Inline Hot Path Functions**
   - `serial_handler_data_available()`: Inlined for zero-overhead checks in main loop
   - Declared as `static inline` in header for cross-module optimization

2. **Function Attributes**
   - `__attribute__((hot))`: Applied to `viking_bio_parse_data()` to prioritize optimization
   - Hints to compiler that this is a frequently-called function

3. **Branch Prediction**
   - `likely()` / `unlikely()` macros for expected paths in protocol parser
   - Helps CPU branch predictor and compiler optimization
   - Used in packet validation and format detection

### Memory Usage

With optimizations enabled:
- **Text (code)**: 379 KB
- **BSS (uninitialized data)**: 49 KB
- **Data (initialized data)**: 0 KB
- **Total UF2 image**: 742 KB

### Trade-offs

- **Speed over size**: `-O3` produces larger binaries than `-Os`, but significantly faster execution
- **No LTO**: Would reduce size further but causes linker errors with SDK wrappers
- **Fast math**: Safe for sensor data processing but may not be suitable for all applications

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Project CHIP](https://github.com/project-chip/connectedhomeip)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)
