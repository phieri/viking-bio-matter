# Viking Bio Matter Bridge - Coding Agent Instructions

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico/Pico W. Reads TTL serial data (9600 baud) from burner and exposes flame status, fan speed, and temperature via Matter protocol over WiFi.

**Size**: Small (~400KB, 13 source files)  
**Languages**: C (firmware), C++ (Matter platform port), Python (simulator)  
**Target**: ARM Cortex-M0+ (RP2040 microcontroller)  
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

### Standard Build (Matter Disabled - Default)

```bash
mkdir build && cd build
export PICO_SDK_PATH=/path/to/pico-sdk  # REQUIRED
cmake .. -DENABLE_MATTER=OFF  # or simply: cmake ..
make -j$(nproc)
```

**Output**: `viking_bio_matter.uf2` (~129KB) - Works on both Pico and Pico W  
**Build Time**: ~5 seconds (parallel build)  
**Status**: ✅ Always succeeds if PICO_SDK_PATH is set

### Matter-Enabled Build (Pico W Only)

```bash
# Initialize Matter SDK submodule (one-time, ~10-15 minutes)
git submodule update --init --recursive third_party/connectedhomeip

# Configure WiFi credentials in platform/pico_w_chip_port/network_adapter.cpp:
# #define WIFI_SSID "YourNetwork"
# #define WIFI_PASSWORD "YourPassword"

mkdir build-matter && cd build-matter
export PICO_SDK_PATH=/path/to/pico-sdk  # REQUIRED
cmake .. -DENABLE_MATTER=ON
make -j$(nproc)
```

**Output**: `viking_bio_matter.uf2` (larger) - Pico W only, includes full Matter stack  
**Build Time**: First build ~5-10 minutes (Matter SDK compilation), subsequent ~30 seconds  
**Status**: ⚠️ Requires connectedhomeip submodule initialized

### Common Build Issues & Workarounds

1. **"PICO_SDK_PATH not set"**  
   → Export `PICO_SDK_PATH=/path/to/pico-sdk` before cmake

2. **"arm-none-eabi-gcc not found"**  
   → Install ARM toolchain: `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`

3. **"connectedhomeip submodule not found"** (Matter builds only)  
   → Run: `git submodule update --init --recursive third_party/connectedhomeip`  
   → Takes 10-15 minutes, requires good internet connection

4. **Clean build recommended after**:
   - Toggling ENABLE_MATTER flag
   - Updating submodules
   - Changing WiFi credentials

### Testing & Validation

**No automated tests** - validation is manual via hardware/simulator.

**Test with simulator** (without hardware):
```bash
# Install dependency
pip3 install pyserial

# Run simulator (sends data to serial port)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0
# Options: -p text (text protocol), -i 2.0 (interval seconds)
```

**Monitor device** (requires Pico connected via USB):
```bash
screen /dev/ttyACM0 115200
# Or: ./run.sh monitor
```

## Project Structure & Key Files

### Root Directory Files
```
viking-bio-matter/
├── CMakeLists.txt              # Main build config (ENABLE_MATTER option)
├── pico_sdk_import.cmake       # Pico SDK integration
├── setup.sh                    # Interactive setup script
├── run.sh                      # Helper: monitor/simulate/commission/test
├── README.md                   # User documentation
├── ARCHITECTURE.md             # Technical architecture details
├── CONTRIBUTING.md             # Development guidelines
├── IMPLEMENTATION.md           # Matter implementation details
├── LICENSE                     # MIT License
├── .gitignore                  # Excludes: build/, pico-sdk/, *.uf2, third_party/
└── .gitmodules                 # connectedhomeip submodule config
```

### Source Code Layout
```
src/
├── main.c                      # Entry point, main loop
├── serial_handler.c            # UART interrupt-driven RX (GP1)
├── viking_bio_protocol.c       # Protocol parser (binary/text formats)
└── matter_bridge.c             # Matter integration (stub or full)

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
└── connectedhomeip/            # Matter SDK submodule (git submodule)
```

### Configuration Files

- **Build**: `CMakeLists.txt` (ENABLE_MATTER flag, line 4)
- **Matter WiFi**: `platform/pico_w_chip_port/network_adapter.cpp` (WIFI_SSID, WIFI_PASSWORD)
- **Matter Credentials**: `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h` (PIN, discriminator)
- **No linting config** - follow existing code style

## GPIO Pin Assignments

| Pin   | Function    | Description                    |
|-------|-------------|--------------------------------|
| GP0   | UART0 TX    | Serial output (unused)         |
| GP1   | UART0 RX    | Viking Bio serial input        |
| GP25  | LED         | Status indicator               |

## Serial Protocol Formats

**Binary** (6 bytes): `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]`  
**Text**: `F:1,S:50,T:75\n` (Flame, Speed, Temperature)

## Matter Clusters (ENABLE_MATTER=ON only)

- **OnOff (0x0006)**: Flame detected state
- **LevelControl (0x0008)**: Fan speed 0-100%
- **TemperatureMeasurement (0x0402)**: Temperature (centidegrees)

**Default Commissioning Credentials** (TEST ONLY, from CHIPDevicePlatformConfig.h):
- PIN: 20202021
- Discriminator: 3840 (0x0F00)
- Manual Code: 34970112332
- QR Code: MT:Y.K9042C00KA0648G00 (verify format if modifying)

## CI/CD Pipeline

**Workflow**: `.github/workflows/build-firmware.yml`

**Jobs**:
1. `build-standard`: Builds with ENABLE_MATTER=OFF (Pico/Pico W)
2. `build-matter`: Builds with ENABLE_MATTER=ON (Pico W only, clones connectedhomeip)

**Artifacts**: `*.uf2`, `*.elf`, `*.bin`, `*.hex` files

**Triggers**: Push to main/develop, PRs to main, manual dispatch

**Dependencies**:
- Pico SDK 1.5.1 (cached between runs)
- ARM toolchain (gcc-arm-none-eabi)
- connectedhomeip submodule (Matter build only)

## Key Development Facts

1. **Two operating modes**:
   - Stub mode (default): Console logging only, no network
   - Full mode (ENABLE_MATTER=ON): Complete Matter/WiFi stack

2. **Conditional compilation**: Uses `#ifdef ENABLE_MATTER` throughout codebase

3. **Interrupt-driven serial**: Uses Pico SDK `hardware_sync` library for interrupt safety (save_and_disable_interrupts, restore_interrupts)

4. **No OTA support**: Firmware updates require physical USB/BOOTSEL button

5. **Flash storage**: Matter builds reserve last 256KB of Pico's 2MB flash for fabric data

6. **Memory constraints**: Small RAM (264KB) limits fabric count to ~5 max

## Common Code Change Scenarios

### Modifying Serial Protocol
- Edit `src/viking_bio_protocol.c` and `include/viking_bio_protocol.h`
- Update `viking_bio_data_t` struct if changing data fields
- Test with simulator: `python3 examples/viking_bio_simulator.py`

### Adding Matter Clusters
- Edit `src/matter_bridge.c` (add cluster definitions/handlers)
- Update `platform/pico_w_chip_port/platform_manager.cpp` for initialization
- Requires Matter build (ENABLE_MATTER=ON)

### Changing GPIO Pins
- Edit `include/serial_handler.h` (pin definitions)
- Update `src/serial_handler.c` (UART configuration)
- Update README.md wiring diagrams

### WiFi Configuration
- Edit `platform/pico_w_chip_port/network_adapter.cpp` (WIFI_SSID/PASSWORD)
- Or use CMake: `cmake -DWIFI_SSID="..." -DWIFI_PASSWORD="..." ..`

## Essential Validation Before PR

1. ✅ Build standard firmware: `cmake .. && make` (must succeed)
2. ✅ Check build artifacts exist: `ls *.uf2 *.elf`
3. ⚠️ Matter build optional but recommended if modifying platform port
4. ✅ Verify no new TODOs/HACKs without good reason
5. ✅ Update documentation if changing APIs or build steps

## Trust These Instructions

These instructions have been validated against actual builds and repository structure. Only search/explore if:
- Instructions are incomplete for your specific task
- You encounter errors not documented here
- Repository has changed significantly (check git log)

Otherwise, follow these steps exactly to minimize exploration time and build failures.
