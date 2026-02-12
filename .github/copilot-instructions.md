# Viking Bio Matter Bridge - Coding Agent Instructions

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W. Reads TTL serial data (9600 baud) from burner and exposes flame status, fan speed, and temperature via Matter protocol over WiFi.

**Size**: Small (~484KB core files, 17 source files excluding dependencies)  
**Languages**: C (firmware), C++ (Matter platform port), Python (simulator/tools)  
**Target**: ARM Cortex-M0+ (RP2040 microcontroller) on Raspberry Pi Pico W  
**Key Frameworks**: Pico SDK 1.5.1, Minimal Matter Protocol Implementation

## Build System & Commands

### Prerequisites Setup (ALWAYS run first on fresh environment)

```bash
# Install toolchain (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Clone and setup Pico SDK (required)
git clone --depth 1 --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init && cd ..
export PICO_SDK_PATH=$(pwd)/pico-sdk
```

**CRITICAL**: `PICO_SDK_PATH` environment variable MUST be set before running cmake. Add to shell profile or export in every session.

**Pico SDK submodule initialization time**: ~30 seconds (required for lwIP, mbedTLS, cyw43-driver, btstack, tinyusb)

**Verified Versions**:
- CMake: 3.13+ (tested with 3.31.6)
- ARM GCC: gcc-arm-none-eabi 13.2.1 20231009
- Pico SDK: 1.5.1 (exact version required)

### Build Instructions (Matter Always Enabled)

**IMPORTANT**: Matter support is **always enabled** in this firmware using a minimal Matter protocol implementation. There is no ENABLE_MATTER flag. The firmware **requires Pico W** (not standard Pico).

```bash
# Step 1: Create build directory and configure
mkdir build && cd build
export PICO_SDK_PATH=/path/to/pico-sdk  # REQUIRED - must be absolute path

# Step 2: Configure with CMake (~1.3 seconds)
cmake ..

# Step 3: Build firmware (~26 seconds first build, ~0.4s incremental)
make -j$(nproc)
```

**Output**: `viking_bio_matter.uf2` (~837KB) - Pico W only, includes minimal Matter stack  
**Build Time** (validated on 8 cores): 
- Pico SDK submodule init: ~30 seconds (one-time)
- CMake configuration: ~1.3 seconds
- First/clean build: ~26 seconds
- Incremental builds: ~0.4 seconds

**Build Artifacts** (all generated in build/ directory):
- `viking_bio_matter.uf2` (837KB) - Flash to Pico W via USB mass storage
- `viking_bio_matter.elf` (765KB) - For debugging
- `viking_bio_matter.bin` (419KB) - Raw binary
- `viking_bio_matter.hex` (1.2MB) - Intel HEX format

**Firmware Size** (verified with arm-none-eabi-size):
- Text (code): 428KB
- BSS (uninitialized): 54KB  
- Data (initialized): 0KB

**Status**: ✅ Always succeeds if PICO_SDK_PATH is set

### Common Build Issues & Workarounds

1. **"PICO_SDK_PATH not set"** (MOST COMMON)
   - **Error**: `CMake Error: PICO_SDK_PATH not set`
   - **Solution**: Export `PICO_SDK_PATH=/absolute/path/to/pico-sdk` before cmake
   - **Must** use absolute path, not relative path
   - **Verify**: Run `echo $PICO_SDK_PATH` to confirm it's set

2. **"arm-none-eabi-gcc not found"**
   - **Error**: `CMake Error: Could not find arm-none-eabi-gcc`
   - **Solution**: Install ARM toolchain: `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`
   - **Verify**: Run `which arm-none-eabi-gcc` to confirm installation

3. **"This firmware requires PICO_BOARD=pico_w"**
   - **Error**: Fatal error during CMake configuration
   - **Cause**: CMakeLists.txt automatically enforces pico_w (lines 3-9)
   - **Note**: Standard Pico (without WiFi) is NOT supported - only Pico W works

4. **Pico SDK submodules not initialized**
   - **Error**: Build fails with missing lwIP, mbedTLS, or cyw43-driver headers
   - **Solution**: `cd pico-sdk && git submodule update --init` (~30 seconds)
   - **Symptom**: Missing files like `lwip/netif.h` or `pico/cyw43_arch.h`

5. **Clean build recommended after**:
   - Changing WiFi credentials in `platform/pico_w_chip_port/network_adapter.cpp`
   - Modifying platform port files
   - Updating PICO_SDK_PATH to different SDK version
   - **How**: `rm -rf build && mkdir build && cd build && cmake .. && make`

### WiFi Configuration

WiFi credentials are hardcoded in `platform/pico_w_chip_port/network_adapter.cpp`:

```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

**To configure WiFi**:
1. Edit `platform/pico_w_chip_port/network_adapter.cpp`
2. Update WIFI_SSID and WIFI_PASSWORD defines
3. Rebuild firmware (clean build recommended)
4. ⚠️ **IMPORTANT**: Never commit credentials to version control

**Note**: There is no CMake option for WiFi credentials. They must be edited in the source file.

### Testing & Validation

**No automated tests** - validation is manual via hardware/simulator. The repository has 214 CMakeLists.txt files and test infrastructure in tests/ directory for Matter components, but tests are disabled for Pico builds (host-only).

**Test with simulator** (without hardware):
```bash
# Install dependency
pip3 install pyserial

# Run simulator (sends test data to serial port)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0
# Options: -p text (text protocol), -i 2.0 (interval seconds)
```

**Monitor device** (requires Pico W connected via USB):
```bash
screen /dev/ttyACM0 115200
# Or: ./run.sh monitor
```

**Expected output on serial console**:
```
Viking Bio Matter Bridge starting...
Setting up WiFi...
Connecting to WiFi: YourNetworkName
WiFi connected, IP: 192.168.1.xxx
====================================
   Matter Commissioning Info
====================================
Device MAC:     28:CD:C1:00:00:01
Setup PIN Code: 24890840  (derived from MAC)
Discriminator:  3840 (0x0F00)
====================================
```

**Helper Scripts** (validated):
- `./setup.sh` - Interactive setup wizard (installs deps, downloads SDK, configures build)
- `./run.sh monitor` - Connect to device serial console
- `./run.sh simulate` - Run Viking Bio simulator
- `./run.sh commissioning` - Show Matter commissioning info

## Project Structure & Key Files

### Root Directory Files
```
viking-bio-matter/
├── CMakeLists.txt              # Main build config (enforces Pico W, Matter always enabled)
├── pico_sdk_import.cmake       # Pico SDK integration
├── setup.sh                    # Interactive setup script
├── run.sh                      # Helper: monitor/simulate/commission/test
├── README.md                   # User documentation
├── CONTRIBUTING.md             # Development guidelines
├── CHANGELOG.md                # Version history
├── LICENSE                     # MIT License
├── .gitignore                  # Excludes: build/, pico-sdk/, *.uf2, third_party/
├── .github/
│   └── copilot-instructions.md # AI agent instructions (includes architecture details)
└── (no .gitmodules)            # connectedhomeip must be cloned manually
```

### Source Code Layout
```
src/
├── main.c                      # Entry point, main loop
├── serial_handler.c            # UART interrupt-driven RX (GP1)
├── viking_bio_protocol.c       # Protocol parser (binary/text formats)
├── matter_bridge.cpp           # Matter integration (always enabled)
└── matter_minimal/             # Minimal Matter protocol implementation
    ├── codec/                  # TLV and message encoding
    ├── transport/              # UDP transport layer
    ├── security/               # PASE and session management
    ├── interaction/            # Interaction model (read/subscribe)
    └── clusters/               # Standard Matter clusters

include/
├── serial_handler.h
├── viking_bio_protocol.h
└── matter_bridge.h

platform/pico_w_chip_port/      # Matter platform port (Pico W only)
├── CMakeLists.txt              # Platform build config
├── CHIPDevicePlatformConfig.h  # Matter device configuration
├── network_adapter.cpp         # WiFi/lwIP integration
├── storage_adapter.cpp         # Flash persistence
├── crypto_adapter.cpp          # mbedTLS crypto
├── platform_manager.cpp/h      # Platform coordination
└── README.md                   # Detailed Matter documentation

examples/
├── viking_bio_simulator.py     # Serial data simulator for testing
└── README.md
```

### Configuration Files

- **Build**: `CMakeLists.txt` (enforces PICO_BOARD=pico_w, lines 3-9)
- **Matter WiFi**: `platform/pico_w_chip_port/network_adapter.cpp` (WIFI_SSID, WIFI_PASSWORD hardcoded)
- **Matter Credentials**: `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h` (discriminator 3840, PIN derived from MAC)
- **Matter Config**: `platform/pico_w_chip_port/config/` (lwipopts.h, mbedtls_config.h)
- **No linting config** - follow existing code style

## GPIO Pin Assignments

| Pin   | Function    | Description                    |
|-------|-------------|--------------------------------|
| GP0   | UART0 TX    | Serial output (unused)         |
| GP1   | UART0 RX    | Viking Bio serial input        |

**Note**: Pico W LED is controlled via CYW43 chip (cyw43_arch_gpio_put), not GPIO 25.

## Serial Protocol Formats

**Binary** (6 bytes): `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]`  
**Text**: `F:1,S:50,T:75\n` (Flame, Speed, Temperature)

## Matter Clusters

The firmware exposes three standard Matter clusters:

- **OnOff (0x0006)**: Flame detected state (bool)
- **LevelControl (0x0008)**: Fan speed 0-100% (uint8)
- **TemperatureMeasurement (0x0402)**: Temperature in centidegrees (int16)

**Commissioning Credentials**:
- **Setup PIN**: Derived from device MAC address using SHA256(MAC || "VIKINGBIO-2026") % 100000000
- **Discriminator**: 3840 (0x0F00) - **Reserved for testing per Matter Core Specification 5.1.3.1**
- **PIN Derivation Tool**: `python3 tools/derive_pin.py <MAC_ADDRESS>`

⚠️ **PRODUCTION WARNING**: 
- Discriminator 3840 is reserved for testing only
- Production devices MUST use unique discriminators (0-4095, excluding reserved ranges)
- Update `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h` for production
- Each device has a unique PIN derived from its MAC address (printed on serial console at boot)

## CI/CD Pipeline

**Workflow**: `.github/workflows/build-firmware.yml`

**Jobs**:
1. `build-matter`: Builds Matter-enabled firmware for Pico W
   - Uses Ubuntu latest runner
   - Installs ARM toolchain via apt-get
   - Clones Pico SDK 1.5.1 with submodules (cached between runs)
   - Configures with CMake in `build-matter/` directory
   - Builds with `make -j$(nproc)` (~26 seconds on GitHub runners)
   - Uploads artifacts: *.uf2, *.elf, *.bin, *.hex

**Triggers**: 
- Push to main/develop branches
- Pull requests to main
- Manual workflow dispatch

**Dependencies** (automatically installed in CI):
- cmake, gcc-arm-none-eabi, libnewlib-arm-none-eabi, build-essential
- Pico SDK 1.5.1 (cloned from GitHub, submodules initialized, ~30s first time, cached after)

**Build artifacts location**: Actions tab → Select workflow run → Artifacts section → `viking-bio-matter-firmware-matter`

**CI Build Always Succeeds**: If CI fails, check:
1. PICO_SDK_PATH is properly set (done automatically in workflow at line 46)
2. ARM toolchain installed (done at lines 24-27)
3. Pico SDK submodules initialized (done via `submodules: recursive` at line 43)

**Cache Strategy**: Pico SDK is cached with key `pico-sdk-1.5.1` to speed up subsequent builds

## Key Development Facts

1. **Matter always enabled**:
   - No ENABLE_MATTER flag - Matter support is mandatory
   - Firmware only works on Pico W (WiFi required)
   - Standard Pico (without WiFi) is NOT supported
   - Uses minimal Matter protocol implementation (src/matter_minimal/)

2. **Minimal Matter Implementation**: 
   - Custom lightweight Matter stack in src/matter_minimal/
   - No external Matter SDK dependencies
   - Includes TLV codec, UDP transport, security (PASE), interaction model, and clusters

3. **Interrupt-driven serial**: 
   - Uses Pico SDK `hardware_sync` library for interrupt safety
   - Functions: save_and_disable_interrupts(), restore_interrupts()
   - Circular buffer in serial_handler.c

4. **PIN derivation**: 
   - Setup PIN is unique per device, derived from MAC address
   - Algorithm: SHA256(MAC || "VIKINGBIO-2026") % 100000000
   - Use tools/derive_pin.py to compute offline from MAC

5. **No OTA support**: 
   - Firmware updates require physical USB connection
   - Hold BOOTSEL button and connect USB to flash new firmware

6. **Flash storage**: 
   - Last 256KB of Pico's 2MB flash reserved for Matter fabric data
   - Managed by storage_adapter.cpp
   - Simple key-value store without wear leveling

7. **Memory constraints**: 
   - Pico has 264KB RAM
   - Limits fabric count to ~5 maximum
   - mbedTLS crypto running in stub mode (DRBG/RNG not implemented due to SDK bugs)

8. **Known TODOs in code**:
   - crypto_adapter.cpp: DRBG and RNG are stubbed (Pico SDK 1.5.1 mbedTLS has bugs)
   - SHA256 and AES functions work correctly

## Architecture Details

### Components

#### 1. Serial Handler (`serial_handler.c`)

Manages UART communication with the Viking Bio 20 burner:
- Configures UART0 (GP0/GP1) at 9600 baud, 8N1
- Uses interrupt-driven reception with circular buffer
- Non-blocking read API for main loop integration

#### 2. Viking Bio Protocol Parser (`viking_bio_protocol.c`)

Parses incoming serial data from the burner:

**Binary Protocol** (6 bytes):
```
Byte 0: 0xAA (Start marker)
Byte 1: Flags (bit 0: flame, bits 1-7: errors)
Byte 2: Fan speed (0-100%)
Byte 3: Temperature high byte
Byte 4: Temperature low byte  
Byte 5: 0x55 (End marker)
```

**Text Protocol** (Fallback):
```
F:1,S:50,T:75\n
```
- F: Flame (0/1)
- S: Speed (0-100)
- T: Temperature (°C)

#### 3. Matter Bridge (`matter_bridge.c`)

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

#### 4. Main Application (`main.c`)

Coordinates all components:
1. Initializes peripherals and subsystems
2. Reads serial data in main loop
3. Parses Viking Bio protocol
4. Updates Matter attributes
5. Provides status LED feedback

### Data Flow

```
Viking Bio 20
    ↓ (TTL Serial 9600 baud)
Serial Handler (UART0)
    ↓ (Circular Buffer)
Protocol Parser
    ↓ (viking_bio_data_t)
Matter Bridge
    ↓ (Matter Attributes)
Minimal Matter Stack (src/matter_minimal/)
    ↓ (Matter protocol over WiFi)
Matter Controller (chip-tool, etc.)
```

### Matter Device Type

The bridge implements a **Temperature Sensor** device type with additional custom attributes:

- **Device Type ID**: 0x0302 (Temperature Sensor)
- **Vendor ID**: TBD
- **Product ID**: TBD

### Performance Optimizations

The firmware is built with aggressive compiler optimizations to prioritize execution speed over binary size:

#### Compiler Optimizations

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

#### Code-Level Optimizations

1. **Inline Hot Path Functions**
   - `serial_handler_data_available()`: Inlined for zero-overhead checks in main loop
   - Declared as `static inline` in header for cross-module optimization
   - Trade-off: Exposes internal state for inlining; prioritizes speed over encapsulation

2. **Function Attributes**
   - `__attribute__((hot))`: Applied to `viking_bio_parse_data()` to prioritize optimization
   - Hints to compiler that this is a frequently-called function

3. **Branch Prediction**
   - `unlikely()` macro used conservatively for truly exceptional cases
   - Applied to null pointer checks and invalid temperature ranges
   - Not used in protocol parsing hot path (packet arrival patterns vary)

#### Memory Usage

With optimizations enabled:
- **Text (code)**: 379 KB
- **BSS (uninitialized data)**: 49 KB
- **Data (initialized data)**: 0 KB
- **Total UF2 image**: 742 KB

#### Trade-offs

- **Speed over size**: `-O3` produces larger binaries than `-Os`, but significantly faster execution
- **No LTO**: Would reduce size further but causes linker errors with SDK wrappers
- **Fast math**: Safe for sensor data processing but may not be suitable for all applications

## Common Code Change Scenarios

### Modifying Serial Protocol
- Edit `src/viking_bio_protocol.c` and `include/viking_bio_protocol.h`
- Update `viking_bio_data_t` struct if changing data fields
- Test with simulator: `python3 examples/viking_bio_simulator.py`

### Adding Matter Clusters
- Edit `src/matter_bridge.c` (add cluster definitions/handlers)
- Update `platform/pico_w_chip_port/platform_manager.cpp` for initialization
- Rebuild firmware with clean build

### Changing GPIO Pins
- Edit `include/serial_handler.h` (pin definitions)
- Update `src/serial_handler.c` (UART configuration)
- Update README.md wiring diagrams

### WiFi Configuration

Edit `platform/pico_w_chip_port/network_adapter.cpp`:
```cpp
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```
Then rebuild firmware. ⚠️ Never commit credentials to version control.

## Essential Validation Before PR

**Pre-commit Checklist** (run these in order, MUST all pass):

1. ✅ **Set PICO_SDK_PATH**: `export PICO_SDK_PATH=/absolute/path/to/pico-sdk`
   - **Verify**: `echo $PICO_SDK_PATH` returns non-empty path
   
2. ✅ **Clean build succeeds**: 
   ```bash
   rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)
   ```
   - **Expected**: Completes in ~26 seconds with no errors
   - **Verify artifacts exist**: `ls *.uf2 *.elf *.bin *.hex`
   
3. ✅ **Check firmware size is reasonable**:
   ```bash
   arm-none-eabi-size viking_bio_matter.elf
   ```
   - **Expected**: text ~428KB, bss ~54KB (±10KB acceptable)
   
4. ✅ **No credential leaks**: 
   ```bash
   git diff | grep -i -E "password|ssid|secret|key" || echo "OK"
   ```
   - **Must output**: "OK" or no WiFi credentials in diff
   
5. ✅ **No new TODOs without justification**: 
   ```bash
   git diff | grep -E "TODO|HACK|FIXME"
   ```
   - **Acceptable**: Only if well-documented workarounds for known issues
   - **Known acceptable TODOs**: crypto_adapter.cpp DRBG/RNG (Pico SDK 1.5.1 mbedTLS bugs)
   
6. ✅ **CI workflow will pass**:
   - Build artifacts generated successfully (step 2)
   - No syntax errors in C/C++/CMake files
   - .gitignore excludes build/, pico-sdk/, *.uf2

7. ✅ **Update documentation if changing**:
   - Build steps → Update README.md and this file
   - APIs → Update code comments and this file  
   - Matter configuration → Update platform/pico_w_chip_port/README.md

**Quick validation command** (run from repo root):
```bash
export PICO_SDK_PATH=$(pwd)/pico-sdk && \
rm -rf build && mkdir build && cd build && \
cmake .. && make -j$(nproc) && \
ls -lh *.uf2 *.elf *.bin *.hex && \
arm-none-eabi-size viking_bio_matter.elf
```
**Expected time**: ~27 seconds total (CMake 1.3s + Make 26s)

## Matter Platform Port Implementation

### Overview

The Matter platform port at `platform/pico_w_chip_port/` provides a complete platform abstraction layer for the minimal Matter implementation on Raspberry Pi Pico W:

1. **Network Adapter** (`network_adapter.cpp`): Integrates CYW43439 WiFi chip with lwIP stack
2. **Storage Adapter** (`storage_adapter.cpp`): Flash-based persistent storage for fabric commissioning data
3. **Crypto Adapter** (`crypto_adapter.cpp`): mbedTLS integration for cryptographic operations
4. **Platform Manager** (`platform_manager.cpp`): Coordinates initialization and provides unified API

### Implementation File Structure

```
viking-bio-matter/
├── platform/pico_w_chip_port/      # Matter platform port
│   ├── CHIPDevicePlatformConfig.h
│   ├── network_adapter.cpp
│   ├── storage_adapter.cpp
│   ├── crypto_adapter.cpp
│   ├── platform_manager.cpp
│   ├── platform_manager.h
│   ├── CMakeLists.txt
│   ├── config/
│   │   ├── lwipopts.h             # lwIP configuration
│   │   └── mbedtls_config.h       # mbedTLS configuration
│   └── README.md
├── src/matter_bridge.cpp            # Matter integration
├── src/matter_minimal/              # Minimal Matter implementation
└── .github/workflows/
    └── build-firmware.yml          # CI build pipeline
```

### Known Limitations

1. **No OTA support**: Firmware updates require physical USB access
2. **Single WiFi only**: No Thread or Ethernet support
3. **Simple storage**: Basic key-value store without wear leveling
4. **Limited fabrics**: Maximum 5 fabrics due to memory constraints (264KB RAM)
5. **Test credentials**: Ships with discriminator 3840 (testing only)
6. **Crypto limitations**: DRBG and RNG are stubbed due to Pico SDK 1.5.1 mbedTLS bugs

### Future Enhancements

1. **Full Matter SDK Integration**
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

## Trust These Instructions

These instructions have been validated against actual builds and repository structure. Only search/explore if:
- Instructions are incomplete for your specific task
- You encounter errors not documented here
- Repository has changed significantly (check git log)

Otherwise, follow these steps exactly to minimize exploration time and build failures.
