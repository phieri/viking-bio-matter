# Viking Bio Matter Bridge - Coding Agent Instructions

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W. Reads TTL serial data (9600 baud) from burner and exposes flame status, fan speed, and temperature via Matter protocol over WiFi.

**Size**: Small (~484KB core files, 17 source files excluding dependencies)  
**Languages**: C (firmware), C++ (Matter platform port), Python (simulator/tools)  
**Target**: ARM Cortex-M0+ (RP2040 microcontroller) on Raspberry Pi Pico W  
**Key Frameworks**: Pico SDK 1.5.1, connectedhomeip (Matter SDK v1.3-branch)

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

**Pico SDK submodule initialization time**: ~60 seconds (required for lwIP, mbedTLS, cyw43-driver, btstack, tinyusb)

### Build Instructions (Matter Always Enabled)

**IMPORTANT**: Matter support is **always enabled** in this firmware. There is no ENABLE_MATTER flag. The firmware **requires Pico W** (not standard Pico) and the connectedhomeip SDK.

```bash
# Step 1: Clone connectedhomeip (one-time, REQUIRED)
# NOTE: .gitmodules is not configured in this repo, must clone manually
mkdir -p third_party
cd third_party
git clone --depth 1 --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git
cd ..

# Step 2: Create build directory and configure
mkdir build && cd build
export PICO_SDK_PATH=/path/to/pico-sdk  # REQUIRED - must be absolute path

# Step 3: Configure with CMake
cmake ..

# Step 4: Build firmware
make -j$(nproc)
```

**Output**: `viking_bio_matter.uf2` (~717KB) - Pico W only, includes full Matter stack  
**Build Time**: 
- First build: ~12 seconds (parallel build on 8 cores)
- Clean builds: ~12 seconds
- Incremental builds: ~3-5 seconds

**Status**: ✅ Always succeeds if PICO_SDK_PATH is set and connectedhomeip is cloned

### Common Build Issues & Workarounds

1. **"PICO_SDK_PATH not set"**  
   → Export `PICO_SDK_PATH=/path/to/pico-sdk` before cmake  
   → Use absolute path, not relative path

2. **"arm-none-eabi-gcc not found"**  
   → Install ARM toolchain: `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`

3. **"ERROR: Matter SDK (connectedhomeip) not found"**  
   → The repository does NOT have .gitmodules configured  
   → Cannot use `git submodule update --init`  
   → Must manually clone: `git clone --depth 1 --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git third_party/connectedhomeip`  
   → Takes 2-3 minutes to clone (~16K files)

4. **"This firmware requires PICO_BOARD=pico_w"**  
   → CMakeLists.txt automatically sets PICO_BOARD=pico_w  
   → Standard Pico (without WiFi) is NOT supported  
   → Only Pico W works

5. **Clean build recommended after**:
   - Changing WiFi credentials in platform/pico_w_chip_port/network_adapter.cpp
   - Updating connectedhomeip version
   - Modifying platform port files

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

**No automated tests** - validation is manual via hardware/simulator.

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

## Project Structure & Key Files

### Root Directory Files
```
viking-bio-matter/
├── CMakeLists.txt              # Main build config (enforces Pico W, Matter always enabled)
├── pico_sdk_import.cmake       # Pico SDK integration
├── setup.sh                    # Interactive setup script
├── run.sh                      # Helper: monitor/simulate/commission/test
├── README.md                   # User documentation
├── ARCHITECTURE.md             # Technical architecture details
├── CONTRIBUTING.md             # Development guidelines
├── IMPLEMENTATION.md           # Matter implementation details
├── LICENSE                     # MIT License
├── .gitignore                  # Excludes: build/, pico-sdk/, *.uf2, third_party/
└── (no .gitmodules)            # connectedhomeip must be cloned manually
```

### Source Code Layout
```
src/
├── main.c                      # Entry point, main loop
├── serial_handler.c            # UART interrupt-driven RX (GP1)
├── viking_bio_protocol.c       # Protocol parser (binary/text formats)
└── matter_bridge.c             # Matter integration (always enabled)

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

third_party/
└── connectedhomeip/            # Matter SDK (must be cloned manually, not a git submodule)
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
   - Clones Pico SDK 1.5.1 (cached between runs)
   - Clones connectedhomeip (v1.3-branch, ~2-3 minutes)
   - Builds with cmake and make (~12 seconds)
   - Uploads artifacts: *.uf2, *.elf, *.bin, *.hex

**Triggers**: 
- Push to main/develop branches
- Pull requests to main
- Manual workflow dispatch

**Dependencies**:
- Pico SDK 1.5.1 (cached, ~60 seconds to initialize submodules if not cached)
- ARM toolchain (gcc-arm-none-eabi, libnewlib-arm-none-eabi)
- connectedhomeip SDK (cloned fresh each time, not cached due to size)

**Build artifacts location**: Actions tab → Select workflow run → Artifacts section

## Key Development Facts

1. **Matter always enabled**:
   - No ENABLE_MATTER flag - Matter support is mandatory
   - Firmware only works on Pico W (WiFi required)
   - Standard Pico (without WiFi) is NOT supported

2. **No .gitmodules configuration**: 
   - Repository lacks .gitmodules file for connectedhomeip
   - Cannot use `git submodule update --init`
   - Must manually clone connectedhomeip to third_party/

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

1. ✅ **Build firmware**: `cmake .. && make` (must succeed, ~12 seconds)
2. ✅ **Check build artifacts exist**: `ls *.uf2 *.elf *.bin *.hex`
3. ✅ **Verify PICO_SDK_PATH is set**: `echo $PICO_SDK_PATH` (must be non-empty)
4. ✅ **Verify connectedhomeip exists**: `ls third_party/connectedhomeip/src` (must exist)
5. ✅ **Check for credential leaks**: `git diff | grep -i password` (must be empty)
6. ✅ **Verify no new TODOs/HACKs**: `git diff | grep -E "TODO|HACK|FIXME"` (without good reason)
7. ✅ **Update documentation if changing**:
   - Build steps → Update README.md and this file
   - APIs → Update ARCHITECTURE.md and code comments
   - Matter configuration → Update platform/pico_w_chip_port/README.md

## Trust These Instructions

These instructions have been validated against actual builds and repository structure. Only search/explore if:
- Instructions are incomplete for your specific task
- You encounter errors not documented here
- Repository has changed significantly (check git log)

Otherwise, follow these steps exactly to minimize exploration time and build failures.
